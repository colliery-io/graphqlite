/*
 * Cypher AST to SQL transformation
 * Converts parsed Cypher queries into executable SQL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "transform/cypher_transform.h"
#include "transform/sql_builder.h"
#include "parser/cypher_debug.h"

/* Initial buffer sizes */
#define INITIAL_SQL_BUFFER_SIZE 1024
#define INITIAL_VARIABLE_CAPACITY 16

/* Transform context management */

cypher_transform_context* cypher_transform_create_context(sqlite3 *db)
{
    cypher_transform_context *ctx = calloc(1, sizeof(cypher_transform_context));
    if (!ctx) {
        return NULL;
    }
    
    ctx->db = db;
    
    /* Initialize SQL buffer */
    ctx->sql_buffer = malloc(INITIAL_SQL_BUFFER_SIZE);
    if (!ctx->sql_buffer) {
        free(ctx);
        return NULL;
    }
    ctx->sql_buffer[0] = '\0';
    ctx->sql_capacity = INITIAL_SQL_BUFFER_SIZE;
    ctx->sql_size = 0;

    /* Initialize unified variable context (includes path variable tracking) */
    ctx->var_ctx = transform_var_ctx_create();
    if (!ctx->var_ctx) {
        free(ctx->sql_buffer);
        free(ctx);
        return NULL;
    }

    ctx->query_type = QUERY_TYPE_UNKNOWN;
    ctx->has_error = false;
    ctx->global_alias_counter = 0;
    ctx->error_message = NULL;
    ctx->in_comparison = false;

    /* Initialize parameter tracking */
    ctx->param_names = NULL;
    ctx->param_count = 0;
    ctx->param_capacity = 0;

    /* Initialize CTE counter */
    ctx->cte_count = 0;

    /* Initialize unified SQL builder */
    ctx->unified_builder = sql_builder_create();
    if (!ctx->unified_builder) {
        transform_var_ctx_free(ctx->var_ctx);
        free(ctx->sql_buffer);
        free(ctx);
        return NULL;
    }

    CYPHER_DEBUG("Created transform context %p", (void*)ctx);

    return ctx;
}

void cypher_transform_free_context(cypher_transform_context *ctx)
{
    if (!ctx) {
        return;
    }
    
    CYPHER_DEBUG("Freeing transform context %p", (void*)ctx);

    /* Free unified variable context (includes path variables) */
    transform_var_ctx_free(ctx->var_ctx);

    /* Free parameter names */
    for (int i = 0; i < ctx->param_count; i++) {
        free(ctx->param_names[i]);
    }
    free(ctx->param_names);

    /* Free unified SQL builder */
    sql_builder_free(ctx->unified_builder);

    /* Free buffers */
    free(ctx->sql_buffer);
    free(ctx->error_message);
    
    free(ctx);
}

/* SQL generation helpers */

void append_sql(cypher_transform_context *ctx, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    
    /* Calculate required size */
    va_list args_copy;
    va_copy(args_copy, args);
    int required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    /* Grow buffer if needed */
    size_t new_size = ctx->sql_size + required + 1;
    if (new_size > ctx->sql_capacity) {
        size_t new_capacity = ctx->sql_capacity * 2;
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }
        
        char *new_buffer = realloc(ctx->sql_buffer, new_capacity);
        if (!new_buffer) {
            ctx->has_error = true;
            ctx->error_message = strdup("Out of memory during SQL generation");
            va_end(args);
            return;
        }
        
        ctx->sql_buffer = new_buffer;
        ctx->sql_capacity = new_capacity;
    }
    
    /* Append to buffer */
    int written = vsnprintf(ctx->sql_buffer + ctx->sql_size, 
                           ctx->sql_capacity - ctx->sql_size, 
                           format, args);
    ctx->sql_size += written;
    
    va_end(args);
    
    CYPHER_DEBUG("SQL buffer now: %s", ctx->sql_buffer);
}

void append_identifier(cypher_transform_context *ctx, const char *name)
{
    /* SQLite uses double quotes for identifiers */
    append_sql(ctx, "\"%s\"", name);
}

void append_string_literal(cypher_transform_context *ctx, const char *value)
{
    /* SQLite uses single quotes for strings - escape embedded quotes */
    char *escaped = escape_sql_string(value);
    if (escaped) {
        append_sql(ctx, "'%s'", escaped);
        free(escaped);
    } else {
        /* Fallback if allocation fails */
        append_sql(ctx, "'%s'", value ? value : "");
    }
}

/* Parameter tracking */

int register_parameter(cypher_transform_context *ctx, const char *name)
{
    /* Check if parameter already registered */
    for (int i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->param_names[i], name) == 0) {
            return i;  /* Already registered, return existing index */
        }
    }

    /* Grow capacity if needed */
    if (ctx->param_count >= ctx->param_capacity) {
        int new_capacity = ctx->param_capacity == 0 ? 8 : ctx->param_capacity * 2;
        char **new_names = realloc(ctx->param_names, new_capacity * sizeof(char*));
        if (!new_names) {
            ctx->has_error = true;
            ctx->error_message = strdup("Out of memory registering parameter");
            return -1;
        }
        ctx->param_names = new_names;
        ctx->param_capacity = new_capacity;
    }

    /* Register new parameter */
    ctx->param_names[ctx->param_count] = strdup(name);
    if (!ctx->param_names[ctx->param_count]) {
        ctx->has_error = true;
        ctx->error_message = strdup("Out of memory registering parameter");
        return -1;
    }

    return ctx->param_count++;
}

/* SQL finalization - assembles unified_builder content into sql_buffer */

int finalize_sql_generation(cypher_transform_context *ctx)
{
    if (!ctx || !ctx->unified_builder) {
        return 0;
    }

    /* Assemble unified builder content into sql_buffer
     * For UNION queries (in_union=true), we append to accumulate branches
     * For regular queries, we reset the buffer first
     */
    char *assembled = sql_builder_to_string(ctx->unified_builder);
    if (assembled) {
        if (!ctx->in_union) {
            ctx->sql_size = 0;
            ctx->sql_buffer[0] = '\0';
        }
        append_sql(ctx, "%s", assembled);
        free(assembled);

        /* NOTE: CTEs are NOT cleared here - they are handled by
         * prepend_cte_to_sql() which runs at the end of transformation. */
    }
    return 0;
}

/*
 * Prepend CTEs from unified builder to the final SQL.
 * All CTEs should now go through sql_cte() on the unified_builder.
 */
void prepend_cte_to_sql(cypher_transform_context *ctx)
{
    if (!ctx) return;

    /* Check for CTEs from unified builder */
    if (!ctx->unified_builder || dbuf_is_empty(&ctx->unified_builder->cte)) {
        return;
    }

    const char *cte_str = dbuf_get(&ctx->unified_builder->cte);
    size_t cte_len = dbuf_len(&ctx->unified_builder->cte);

    CYPHER_DEBUG("Prepending CTEs to SQL (%zu bytes)", cte_len);

    /* Calculate new size needed: CTE + space + SQL + null */
    size_t new_size = cte_len + 1 + ctx->sql_size + 1;

    /* Allocate new buffer */
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
        ctx->has_error = true;
        ctx->error_message = strdup("Memory allocation failed during CTE prepend");
        return;
    }

    /* Copy CTE, space, and SQL */
    memcpy(new_buffer, cte_str, cte_len);
    new_buffer[cte_len] = ' ';
    memcpy(new_buffer + cte_len + 1, ctx->sql_buffer, ctx->sql_size + 1);

    /* Replace old buffer */
    free(ctx->sql_buffer);
    ctx->sql_buffer = new_buffer;
    ctx->sql_size = cte_len + 1 + ctx->sql_size;
    ctx->sql_capacity = new_size;

    CYPHER_DEBUG("New SQL after CTE prepend: %s", ctx->sql_buffer);
}

/* Register a path variable (uses unified transform_var system) */
int register_path_variable(cypher_transform_context *ctx, const char *name, cypher_path *path)
{
    /* Map AST path_type to var_path_type */
    var_path_type ptype;
    switch (path->type) {
        case PATH_TYPE_SHORTEST:
            ptype = VAR_PATH_SHORTEST;
            break;
        case PATH_TYPE_ALL_SHORTEST:
            ptype = VAR_PATH_ALL_SHORTEST;
            break;
        default:
            ptype = VAR_PATH_NORMAL;
            break;
    }

    /* Register in unified variable tracking system */
    return transform_var_register_path(ctx->var_ctx, name, NULL, path->elements, ptype);
}

/* Generate next unique alias */
char* get_next_default_alias(cypher_transform_context *ctx)
{
    char *alias = malloc(64);
    if (!alias) {
        return NULL;
    }
    snprintf(alias, 64, "_gql_default_alias_%d", ctx->global_alias_counter++);
    return alias;
}

/* Forward declarations for UNION support */
static int transform_single_query_sql(cypher_transform_context *ctx, cypher_query *query);
static int transform_union_sql(cypher_transform_context *ctx, cypher_union *union_node);

/* Main transform dispatcher */

cypher_query_result* cypher_transform_query(cypher_transform_context *ctx, cypher_query *query)
{
    CYPHER_DEBUG("Starting query transformation");

    if (!ctx || !query) {
        return NULL;
    }

    /* Reset context for new query */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';
    ctx->has_error = false;
    free(ctx->error_message);
    ctx->error_message = NULL;
    ctx->global_alias_counter = 0;

    /* Check if this is a UNION query */
    ast_node *root = (ast_node*)query;
    if (root->type == AST_NODE_UNION) {
        CYPHER_DEBUG("Processing UNION query");
        if (transform_union_sql(ctx, (cypher_union*)root) < 0) {
            goto error;
        }
        prepend_cte_to_sql(ctx);

        /* Create result structure and prepare statement */
        cypher_query_result *result = calloc(1, sizeof(cypher_query_result));
        if (!result) {
            goto error;
        }

        CYPHER_DEBUG("Generated SQL (UNION): %s", ctx->sql_buffer);
        int rc = sqlite3_prepare_v2(ctx->db, ctx->sql_buffer, -1, &result->stmt, NULL);
        if (rc != SQLITE_OK) {
            result->has_error = true;
            result->error_message = strdup(sqlite3_errmsg(ctx->db));
            return result;
        }
        return result;
    }

    /* Check if any MATCH clause is optional - if so, use SQL builder from start */
    bool has_optional_match = false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH) {
            cypher_match *match = (cypher_match*)clause;
            CYPHER_DEBUG("Found MATCH clause %d, optional = %s", i, match->optional ? "true" : "false");
            if (match->optional) {
                has_optional_match = true;
                break;
            }
        }
    }
    CYPHER_DEBUG("Query analysis complete: has_optional_match = %s", has_optional_match ? "true" : "false");
    (void)has_optional_match; /* Analysis used for debugging - unified_builder always active */

    /* Process each clause in order */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        
        /* Mark variables from previous clause as inherited */
        if (i > 0) {
            transform_var_mark_inherited(ctx->var_ctx);
        }
        
        CYPHER_DEBUG("Processing clause type %s", ast_node_type_name(clause->type));
        
        switch (clause->type) {
            case AST_NODE_MATCH:
                if (transform_match_clause(ctx, (cypher_match*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_CREATE:
                if (transform_create_clause(ctx, (cypher_create*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_SET:
                if (transform_set_clause(ctx, (cypher_set*)clause) < 0) {
                    goto error;
                }
                break;
                
            case AST_NODE_DELETE:
                if (transform_delete_clause(ctx, (cypher_delete*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_REMOVE:
                if (transform_remove_clause(ctx, (cypher_remove*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_RETURN:
                /* RETURN clause now handles unified builder directly */
                /* It will add SELECT columns, ORDER BY, LIMIT to the builder */
                /* and finalize if needed */
                CYPHER_DEBUG("Processing RETURN clause (unified_builder=%s)",
                            ctx->unified_builder ? "active" : "inactive");

                if (transform_return_clause(ctx, (cypher_return*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_WITH:
                /* WITH clause extracts builder state directly - no finalize needed */
                if (transform_with_clause(ctx, (cypher_with*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_UNWIND:
                /* UNWIND clause extracts builder state directly - no finalize needed */
                if (transform_unwind_clause(ctx, (cypher_unwind*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_FOREACH:
                if (transform_foreach_clause(ctx, (cypher_foreach*)clause) < 0) {
                    goto error;
                }
                break;

            case AST_NODE_LOAD_CSV:
                if (transform_load_csv_clause(ctx, (cypher_load_csv*)clause) < 0) {
                    goto error;
                }
                break;

            default:
                ctx->has_error = true;
                ctx->error_message = strdup("Unsupported clause type");
                goto error;
        }
    }

    /* Create result structure */
    cypher_query_result *result = calloc(1, sizeof(cypher_query_result));
    if (!result) {
        goto error;
    }

    /* Prepend CTE prefix if we have variable-length relationships */
    prepend_cte_to_sql(ctx);

    /* Prepare the SQL statement */
    CYPHER_DEBUG("Generated SQL: %s", ctx->sql_buffer);
    
    int rc = sqlite3_prepare_v2(ctx->db, ctx->sql_buffer, -1, &result->stmt, NULL);
    if (rc != SQLITE_OK) {
        result->has_error = true;
        result->error_message = strdup(sqlite3_errmsg(ctx->db));
        return result;
    }
    
    return result;
    
error:
    CYPHER_DEBUG("Transform error: %s", ctx->error_message ? ctx->error_message : "Unknown error");
    cypher_query_result *error_result = calloc(1, sizeof(cypher_query_result));
    if (error_result) {
        error_result->has_error = true;
        error_result->error_message = strdup(ctx->error_message ? ctx->error_message : "Transform failed");
    }
    return error_result;
}

/* Transform a UNION query to SQL */
static int transform_union_sql(cypher_transform_context *ctx, cypher_union *union_node)
{
    CYPHER_DEBUG("Transforming UNION query (all=%s)", union_node->all ? "true" : "false");

    /* Mark that we're in a UNION context so finalize_sql_generation appends instead of resets */
    ctx->in_union = true;

    /* Transform left side */
    if (union_node->left->type == AST_NODE_UNION) {
        if (transform_union_sql(ctx, (cypher_union*)union_node->left) < 0) {
            return -1;
        }
    } else if (union_node->left->type == AST_NODE_QUERY) {
        if (transform_single_query_sql(ctx, (cypher_query*)union_node->left) < 0) {
            return -1;
        }
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid left side of UNION");
        return -1;
    }

    /* Add UNION or UNION ALL */
    if (union_node->all) {
        append_sql(ctx, " UNION ALL ");
    } else {
        append_sql(ctx, " UNION ");
    }

    /*
     * Reset state for right side of UNION:
     * - Create fresh unified_builder so second query starts fresh
     * - Reset variable context so variables don't leak between branches
     */
    if (ctx->unified_builder) {
        sql_builder_free(ctx->unified_builder);
        ctx->unified_builder = sql_builder_create();
    }
    transform_var_ctx_reset(ctx->var_ctx);

    /* Transform right side - must be a single query */
    if (union_node->right->type == AST_NODE_QUERY) {
        if (transform_single_query_sql(ctx, (cypher_query*)union_node->right) < 0) {
            return -1;
        }
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("Invalid right side of UNION");
        return -1;
    }

    return 0;
}

/* Transform a single query (non-UNION) to SQL */
static int transform_single_query_sql(cypher_transform_context *ctx, cypher_query *query)
{
    CYPHER_DEBUG("Transforming single query to SQL");

    /* Process each clause in order */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];

        if (i > 0) {
            transform_var_mark_inherited(ctx->var_ctx);
        }

        switch (clause->type) {
            case AST_NODE_MATCH:
                if (transform_match_clause(ctx, (cypher_match*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_CREATE:
                if (transform_create_clause(ctx, (cypher_create*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_SET:
                if (transform_set_clause(ctx, (cypher_set*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_DELETE:
                if (transform_delete_clause(ctx, (cypher_delete*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_REMOVE:
                if (transform_remove_clause(ctx, (cypher_remove*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_RETURN:
                /* RETURN clause handles unified builder directly via sql_select() */
                if (transform_return_clause(ctx, (cypher_return*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_WITH:
                /* WITH clause extracts builder state directly - no finalize needed */
                if (transform_with_clause(ctx, (cypher_with*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_UNWIND:
                /* UNWIND clause extracts builder state directly - no finalize needed */
                if (transform_unwind_clause(ctx, (cypher_unwind*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_FOREACH:
                if (transform_foreach_clause(ctx, (cypher_foreach*)clause) < 0) {
                    return -1;
                }
                break;

            case AST_NODE_LOAD_CSV:
                if (transform_load_csv_clause(ctx, (cypher_load_csv*)clause) < 0) {
                    return -1;
                }
                break;

            default:
                ctx->has_error = true;
                ctx->error_message = strdup("Unsupported clause type");
                return -1;
        }
    }

    return 0;
}

/* Generate SQL only (for EXPLAIN) - does not prepare statement */
int cypher_transform_generate_sql(cypher_transform_context *ctx, cypher_query *query)
{
    CYPHER_DEBUG("Starting SQL-only query transformation (EXPLAIN)");

    if (!ctx || !query) {
        return -1;
    }

    /* Reset context for new query */
    ctx->sql_size = 0;
    ctx->sql_buffer[0] = '\0';
    ctx->has_error = false;
    free(ctx->error_message);
    ctx->error_message = NULL;
    ctx->global_alias_counter = 0;

    /* Check if this is a UNION query */
    ast_node *root = (ast_node*)query;
    if (root->type == AST_NODE_UNION) {
        int result = transform_union_sql(ctx, (cypher_union*)root);
        if (result == 0) {
            prepend_cte_to_sql(ctx);
            CYPHER_DEBUG("Generated SQL (EXPLAIN UNION): %s", ctx->sql_buffer);
        }
        return result;
    }

    /* Standard single query transformation */
    if (transform_single_query_sql(ctx, query) < 0) {
        return -1;
    }

    /* Prepend CTE prefix if we have variable-length relationships */
    prepend_cte_to_sql(ctx);

    CYPHER_DEBUG("Generated SQL (EXPLAIN): %s", ctx->sql_buffer);
    return 0;
}

/* Variable-length relationship CTE generation */

/**
 * Generate a recursive CTE for variable-length relationship traversal.
 *
 * For a query like MATCH (a)-[*1..5]->(b), generates:
 *
 * WITH RECURSIVE varlen_cte_N(start_id, end_id, depth, path_ids, visited) AS (
 *     -- Base case: direct edges
 *     SELECT e.source_id, e.target_id, 1,
 *            CAST(e.source_id || ',' || e.target_id AS TEXT),
 *            ',' || e.source_id || ',' || e.target_id || ','
 *     FROM edges e
 *     WHERE e.type = 'TYPE'  -- if type specified
 *
 *     UNION ALL
 *
 *     -- Recursive case: extend paths
 *     SELECT cte.start_id, e.target_id, cte.depth + 1,
 *            cte.path_ids || ',' || e.target_id,
 *            cte.visited || e.target_id || ','
 *     FROM varlen_cte_N cte
 *     JOIN edges e ON e.source_id = cte.end_id
 *     WHERE cte.depth < max_hops
 *       AND cte.visited NOT LIKE '%,' || e.target_id || ',%'  -- cycle prevention
 *       AND e.type = 'TYPE'  -- if type specified
 * )
 */
int generate_varlen_cte(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                       const char *source_alias, const char *target_alias,
                       const char *cte_name)
{
    (void)source_alias; /* Mark as intentionally unused for now */
    (void)target_alias; /* Mark as intentionally unused for now */

    if (!ctx || !rel || !rel->varlen || !cte_name) {
        return -1;
    }

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    int min_hops = range->min_hops > 0 ? range->min_hops : 1;
    int max_hops = range->max_hops > 0 ? range->max_hops : 100; /* Default max for unbounded */

    CYPHER_DEBUG("Generating varlen CTE %s: min=%d, max=%d, type=%s",
                 cte_name, min_hops, max_hops, rel->type ? rel->type : "<any>");

    /* Build CTE query in a local buffer */
    dynamic_buffer cte_query;
    dbuf_init(&cte_query);

    /* Handle relationship direction */
    const char *src_col = "source_id";
    const char *tgt_col = "target_id";
    if (rel->left_arrow && !rel->right_arrow) {
        /* <-[*]- reversed direction */
        src_col = "target_id";
        tgt_col = "source_id";
    }

    /* Base case: direct edges (depth = 1) */
    dbuf_appendf(&cte_query,
        "SELECT e.%s, e.%s, 1, "
        "CAST(e.%s || ',' || e.%s AS TEXT), "
        "','|| e.%s || ',' || e.%s || ','  "
        "FROM edges e",
        src_col, tgt_col,
        src_col, tgt_col,
        src_col, tgt_col);

    /* Add type constraint if specified */
    if (rel->type) {
        dbuf_appendf(&cte_query, " WHERE e.type = '%s'", rel->type);
    } else if (rel->types && rel->types->count > 0) {
        dbuf_append(&cte_query, " WHERE (");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) {
                dbuf_append(&cte_query, " OR ");
            }
            cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
            dbuf_appendf(&cte_query, "e.type = '%s'", type_lit->value.string);
        }
        dbuf_append(&cte_query, ")");
    }

    /* Recursive case */
    dbuf_append(&cte_query, " UNION ALL ");
    dbuf_appendf(&cte_query,
        "SELECT cte.start_id, e.%s, cte.depth + 1, "
        "cte.path_ids || ',' || e.%s, "
        "cte.visited || e.%s || ',' "
        "FROM %s cte "
        "JOIN edges e ON e.%s = cte.end_id "
        "WHERE cte.depth < %d",
        tgt_col, tgt_col, tgt_col,
        cte_name,
        src_col,
        max_hops);

    /* Add cycle detection */
    dbuf_appendf(&cte_query,
        " AND cte.visited NOT LIKE '%%,' || CAST(e.%s AS TEXT) || ',%%'",
        tgt_col);

    /* Add type constraint to recursive case */
    if (rel->type) {
        dbuf_appendf(&cte_query, " AND e.type = '%s'", rel->type);
    } else if (rel->types && rel->types->count > 0) {
        dbuf_append(&cte_query, " AND (");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) {
                dbuf_append(&cte_query, " OR ");
            }
            cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
            dbuf_appendf(&cte_query, "e.type = '%s'", type_lit->value.string);
        }
        dbuf_append(&cte_query, ")");
    }

    /* Build CTE name with column definitions */
    char cte_full_name[256];
    snprintf(cte_full_name, sizeof(cte_full_name),
             "%s(start_id, end_id, depth, path_ids, visited)", cte_name);

    /* Add CTE to unified builder - recursive CTE */
    sql_cte(ctx->unified_builder, cte_full_name, dbuf_get(&cte_query), true);
    dbuf_free(&cte_query);

    ctx->cte_count++;

    CYPHER_DEBUG("Generated varlen CTE via unified builder: %s", cte_full_name);

    return 0;
}

/* Result management */

void cypher_free_result(cypher_query_result *result)
{
    if (!result) {
        return;
    }
    
    if (result->stmt) {
        sqlite3_finalize(result->stmt);
    }
    
    for (int i = 0; i < result->column_count; i++) {
        free(result->column_names[i]);
    }
    free(result->column_names);
    
    free(result->error_message);
    free(result);
}

bool cypher_result_next(cypher_query_result *result)
{
    if (!result || !result->stmt) {
        return false;
    }
    
    int rc = sqlite3_step(result->stmt);
    return rc == SQLITE_ROW;
}

const char* cypher_result_get_string(cypher_query_result *result, int column)
{
    if (!result || !result->stmt) {
        return NULL;
    }
    
    return (const char*)sqlite3_column_text(result->stmt, column);
}

int cypher_result_get_int(cypher_query_result *result, int column)
{
    if (!result || !result->stmt) {
        return 0;
    }
    
    return sqlite3_column_int(result->stmt, column);
}