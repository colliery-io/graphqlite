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

#include "runtime/udf_register.h"

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

    /* Make sure helper UDFs (_gql_bool, _gql_normalize_date, _gql_in, ...)
     * are registered on this connection — the transform layer validates
     * by preparing SQL that references them. sqlite3_create_function is
     * idempotent (a repeat call just replaces the binding), so this is
     * safe to call even when the SQLite extension already registered them. */
    if (db) {
        graphqlite_register_helper_udfs(db);
    }

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

    /* Initialize counters */
    ctx->cte_count = 0;
    ctx->with_cte_counter = 0;
    ctx->unwind_cte_counter = 0;
    ctx->reduce_counter = 0;
    ctx->prop_join_counter = 0;
    ctx->anon_node_counter = 0;
    ctx->anon_node_base = 0;

    /* Initialize pending property JOINs buffer */
    ctx->pending_prop_joins = NULL;
    ctx->pending_prop_joins_len = 0;
    ctx->pending_prop_joins_cap = 0;

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
    free(ctx->pending_prop_joins);

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

/* append_identifier removed 2026-05-20 — it had no remaining callers. */

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

void append_var_table(cypher_transform_context *ctx, const char *var_name, const char *table)
{
    /* Look up the variable's associated graph */
    const char *graph = transform_var_get_graph(ctx->var_ctx, var_name);

    if (graph && graph[0] != '\0') {
        /* Variable has an associated graph - prefix table with graph name */
        append_sql(ctx, "%s.%s", graph, table);
    } else {
        /* No graph - use table name directly */
        append_sql(ctx, "%s", table);
    }
}

const char *get_graph_table(cypher_transform_context *ctx, const char *table)
{
    static char table_buf[256];

    if (ctx->current_graph && ctx->current_graph[0] != '\0') {
        snprintf(table_buf, sizeof(table_buf), "%s.%s", ctx->current_graph, table);
        return table_buf;
    }
    return table;
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

/* Capture transform_expression output into a malloc'd string
 * (I-0039 transitional helper). Swaps ctx->sql_buffer state around
 * the call so legacy append_sql writes happen into a temporary
 * buffer the caller takes ownership of. Restores prior buffer state
 * on both success and failure paths. Returns NULL on transform error.
 *
 * Used by DML transforms (transform_set.c) that need to splice an
 * expression's SQL into a sql_raw() emission without polluting the
 * main sql_buffer. */
extern int transform_expression(cypher_transform_context *ctx, ast_node *expr);
/* I-0043 X2.1: dynamic_buffer-native LITERAL emission. Emits the SQL
 * form of a Cypher literal directly into `out`. The legacy switch
 * case in transform_expression (transform_return.c) still exists for
 * the old append_sql / sql_buffer path; this is the new entry point
 * for callers that hold a dynamic_buffer.
 *
 * Behavior is byte-identical to the legacy LITERAL case:
 *   INTEGER  -> "%lld"
 *   DECIMAL  -> "%.17g" with ".0" appended if whole-number
 *   STRING   -> escaped 'value'  (uses escape_sql_string)
 *   BOOLEAN  -> "0" or "1"
 *   NULL     -> "NULL" */
static int transform_literal_into(const cypher_literal *lit,
                                  dynamic_buffer *out)
{
    if (!lit || !out) return -1;
    switch (lit->literal_type) {
        case LITERAL_INTEGER:
            dbuf_appendf(out, "%lld", (long long)lit->value.integer);
            return 0;
        case LITERAL_DECIMAL: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", lit->value.decimal);
            bool has_dot = false;
            for (char *p = buf; *p; p++) {
                if (*p == '.' || *p == 'e' || *p == 'E' || *p == 'n') {
                    has_dot = true; break;
                }
            }
            if (!has_dot) {
                size_t l = strlen(buf);
                if (l + 2 < sizeof(buf)) { buf[l] = '.'; buf[l+1] = '0'; buf[l+2] = 0; }
            }
            dbuf_append(out, buf);
            return 0;
        }
        case LITERAL_STRING: {
            char *esc = escape_sql_string(lit->value.string);
            dbuf_appendf(out, "'%s'", esc ? esc : (lit->value.string ? lit->value.string : ""));
            free(esc);
            return 0;
        }
        case LITERAL_BOOLEAN:
            dbuf_append(out, lit->value.boolean ? "1" : "0");
            return 0;
        case LITERAL_NULL:
            dbuf_append(out, "NULL");
            return 0;
    }
    return -1;
}

/* I-0043 X1: new expression transform API. As individual AST cases
 * are migrated under X2.x they get dynamic_buffer-native emitters
 * (e.g. transform_literal_into); transform_expression_into routes
 * those directly, falling back to the legacy scratchpad path for
 * AST cases not yet migrated. The legacy body of transform_expression
 * is deleted in X5 along with append_sql / sql_buffer / sql_size /
 * sql_capacity. */
/* I-0043 X2.2: dynamic_buffer-native PARAMETER emission.
 *   $name -> :name (SQLite named parameter)
 *   $     -> ?     (positional placeholder)
 * Side effect: registers the parameter with the context tracker. */
static int transform_parameter_into(cypher_transform_context *ctx,
                                    const cypher_parameter *param,
                                    dynamic_buffer *out)
{
    if (!ctx || !param || !out) return -1;
    if (param->name) {
        register_parameter(ctx, param->name);
        dbuf_appendf(out, ":%s", param->name);
    } else {
        dbuf_append(out, "?");
    }
    return 0;
}

int transform_expression_into(cypher_transform_context *ctx,
                              ast_node *expr,
                              dynamic_buffer *out)
{
    if (!ctx || !expr || !out) return -1;
    /* Native dbuf emitters — no scratchpad swap. */
    if (expr->type == AST_NODE_LITERAL) {
        return transform_literal_into((const cypher_literal *)expr, out);
    }
    if (expr->type == AST_NODE_PARAMETER) {
        return transform_parameter_into(ctx, (const cypher_parameter *)expr, out);
    }
    /* Other cases still route through the legacy scratchpad path until
     * X2.x migration covers them. */
    char *s = cypher_transform_capture_expression(ctx, expr);
    if (!s) return -1;
    dbuf_append(out, s);
    free(s);
    return 0;
}

char *transform_expression_str(cypher_transform_context *ctx, ast_node *expr)
{
    /* For now this is just cypher_transform_capture_expression with a
     * non-NULL guarantee on success. Kept as a separate symbol so the
     * X4 caller migration can grep for it independently of the older
     * capture helper. */
    return cypher_transform_capture_expression(ctx, expr);
}

char *cypher_transform_capture_expression(cypher_transform_context *ctx, ast_node *expr)
{
    if (!ctx || !expr) return NULL;

    /* Save current sql_buffer state. */
    size_t saved_size = ctx->sql_size;
    char *saved_buf = NULL;
    if (saved_size > 0 && ctx->sql_buffer) {
        saved_buf = malloc(saved_size + 1);
        if (!saved_buf) return NULL;
        memcpy(saved_buf, ctx->sql_buffer, saved_size);
        saved_buf[saved_size] = '\0';
    }

    /* Reset sql_buffer so transform_expression's append_sql writes
     * land at the start of a fresh buffer. */
    ctx->sql_size = 0;
    if (ctx->sql_buffer) ctx->sql_buffer[0] = '\0';

    int rc = transform_expression(ctx, expr);

    /* Capture (or null on failure) and restore. */
    char *captured = NULL;
    if (rc == 0 && ctx->sql_buffer && ctx->sql_size > 0) {
        captured = malloc(ctx->sql_size + 1);
        if (captured) {
            memcpy(captured, ctx->sql_buffer, ctx->sql_size);
            captured[ctx->sql_size] = '\0';
        }
    }

    /* Restore prior sql_buffer state. */
    ctx->sql_size = 0;
    if (ctx->sql_buffer) ctx->sql_buffer[0] = '\0';
    if (saved_buf) {
        append_sql(ctx, "%s", saved_buf);
        free(saved_buf);
    }

    return (rc == 0) ? (captured ? captured : strdup("")) : NULL;
}

/* SQL finalization - assembles unified_builder content into sql_buffer */

int finalize_sql_generation(cypher_transform_context *ctx)
{
    if (!ctx || !ctx->unified_builder) {
        return 0;
    }

    /* I-0042 E1: sql_builder_to_string is idempotent (returns NULL on
     * second call). The existing transform pipeline calls this function
     * multiple times for the same builder (e.g. once at the end of
     * transform_return_clause, again from the executor before
     * prepare/exec), and each call expects fresh SQL reflecting the
     * current builder state. Explicitly unfinalize before serializing
     * so the existing behavior is preserved. New callers that want
     * true single-emit can call sql_builder_to_string directly. */
    sql_builder_unfinalize(ctx->unified_builder);

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
    if (!ctx->unified_builder) return;

    sql_builder *b = ctx->unified_builder;
    bool has_pre = b->pre_cte_count > 0;
    bool has_user = !dbuf_is_empty(&b->cte);
    if (!has_pre && !has_user) return;

    /* Build the prepend prefix: a single WITH clause carrying pre_cte
     * fragments first, then the user-CTE fragments (which arrive with
     * their own leading "WITH"/"WITH RECURSIVE"). We strip the
     * embedded WITH from the user buffer and emit a unified header. */
    dynamic_buffer prefix; dbuf_init(&prefix);

    bool any_recursive = b->pre_cte_recursive;
    /* Detect "WITH RECURSIVE" prefix in the user CTE buffer. */
    const char *user_cte = has_user ? dbuf_get(&b->cte) : "";
    const char *user_body = user_cte;
    if (has_user) {
        if (strncmp(user_cte, "WITH RECURSIVE ", 15) == 0) {
            any_recursive = true;
            user_body = user_cte + 15;
        } else if (strncmp(user_cte, "WITH ", 5) == 0) {
            user_body = user_cte + 5;
        }
    }

    dbuf_append(&prefix, any_recursive ? "WITH RECURSIVE " : "WITH ");
    if (has_pre) {
        dbuf_append(&prefix, dbuf_get(&b->pre_cte));
    }
    if (has_user) {
        if (has_pre) dbuf_append(&prefix, ", ");
        dbuf_append(&prefix, user_body);
    }
    dbuf_append_char(&prefix, ' ');

    const char *prefix_str = dbuf_get(&prefix);
    size_t prefix_len = dbuf_len(&prefix);

    CYPHER_DEBUG("Prepending CTEs (pre_cte=%d, user_cte=%zu) to SQL",
                 b->pre_cte_count, has_user ? dbuf_len(&b->cte) : 0);

    size_t new_size = prefix_len + ctx->sql_size + 1;
    char *new_buffer = malloc(new_size);
    if (!new_buffer) {
        ctx->has_error = true;
        ctx->error_message = strdup("Memory allocation failed during CTE prepend");
        dbuf_free(&prefix);
        return;
    }
    memcpy(new_buffer, prefix_str, prefix_len);
    memcpy(new_buffer + prefix_len, ctx->sql_buffer, ctx->sql_size + 1);

    free(ctx->sql_buffer);
    ctx->sql_buffer = new_buffer;
    ctx->sql_size = prefix_len + ctx->sql_size;
    ctx->sql_capacity = new_size;
    /* T-0310: remember exactly where the CTE prefix ended; needed by
     * the DML split path to give pre_exec_dml the same CTE prefix
     * so CTE-bound variable refs (e.g. _with_0.n.id) resolve. */
    ctx->cte_prefix_len = prefix_len;

    dbuf_free(&prefix);

    CYPHER_DEBUG("New SQL after CTE prepend: %s", ctx->sql_buffer);
}

/* Register a path variable (uses unified transform_var system) */
int register_path_variable(cypher_transform_context *ctx, const char *name, cypher_path *path)
{
    /* Reject conflicts: if `name` is already bound as a node/edge or a
     * scalar projection, the path variable can't shadow it. */
    transform_var *existing = transform_var_lookup(ctx->var_ctx, name);
    if (existing) {
        const char *kind_str = "value";
        switch (existing->kind) {
            case VAR_KIND_NODE: kind_str = "node"; break;
            case VAR_KIND_EDGE: kind_str = "relationship"; break;
            case VAR_KIND_PATH: kind_str = "path"; break;
            case VAR_KIND_PROJECTED: kind_str = "value"; break;
            case VAR_KIND_AGGREGATED: kind_str = "aggregate"; break;
        }
        ctx->has_error = true;
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Variable `%s` is already bound as a %s and cannot be used as a path (SyntaxError: VariableAlreadyBound)",
                 name, kind_str);
        ctx->error_message = strdup(msg);
        return -1;
    }

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

            case AST_NODE_CALL_SUBQUERY:
                /* CALL subquery is handled by the executor, not the transform.
                 * The executor splits the query at CALL boundaries and handles
                 * inner query transform/execution per outer row.
                 * If we reach here, it means no specialized executor pattern
                 * matched — report as not yet implemented. */
                ctx->has_error = true;
                ctx->error_message = strdup("CALL {} subquery requires executor-level handling");
                goto error;

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

    /* T-0311/T-0312: finalize at end of cypher_transform_query's
     * clause loop. Fires for any builder state — SELECT shape OR
     * write-only DML (raw_output). sql_builder_to_string handles
     * both shapes; the legacy raw_output drain shim is no longer
     * needed (T-0312/E3). */
    if (ctx->unified_builder &&
        (ctx->unified_builder->select_count > 0 ||
         !dbuf_is_empty(&ctx->unified_builder->from) ||
         !dbuf_is_empty(&ctx->unified_builder->raw_output))) {
        if (finalize_sql_generation(ctx) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to finalize SQL generation");
            goto error;
        }
    }

    /* Create result structure */
    cypher_query_result *result = calloc(1, sizeof(cypher_query_result));
    if (!result) {
        goto error;
    }

    /* T-0310 / T-0312: split DML out of raw_output for mixed
     * DML+SELECT queries. Pure-DML queries are now handled by
     * sql_builder_to_string emitting raw_output directly (E3 removed
     * the legacy drain shim).
     *
     * Cases for mixed (raw_output non-empty AND sql_buffer has SELECT):
     *   - INSERT OR REPLACE in DML → split; pre_exec_dml to executor.
     *   - Other DML shapes → leave in raw_output; finalize already
     *     appended a compound SELECT;DML form that prepare_v2 will
     *     silently truncate.
     */
    char *raw_dml = NULL;
    bool mixed_dml = false;
    if (ctx->unified_builder &&
        !dbuf_is_empty(&ctx->unified_builder->raw_output) &&
        ctx->sql_size > 0) {
        {
            const char *raw = dbuf_get(&ctx->unified_builder->raw_output);
            const char *peek = raw;
            while (*peek == ' ' || *peek == '\t' || *peek == '\n' ||
                   *peek == '\r' || *peek == ';') peek++;

            /* Only split when the DML is self-contained:
             *   - transform_set emits `INSERT OR REPLACE INTO ... SELECT
             *     <alias>.id ... FROM nodes AS <alias>` — the FROM
             *     is part of the INSERT subquery, so this is portable
             *     across the prepare/exec split.
             * Other DML shapes (transform_remove's naked DELETE,
             * transform_delete after WITH) reference MATCH aliases
             * without a FROM, which only resolves inside the compound
             * SELECT;DML form. Skip those — the legacy compound
             * appended after the SELECT (truncated by prepare_v2's
             * one-statement limit) preserves the prior behavior.
             */
            /* Safe-to-split DML shapes: transform_set's compound that
             * starts with cross-table DELETEs and ends with INSERT OR
             * REPLACE INTO. We detect this by the presence of
             * "INSERT OR REPLACE" anywhere in the buffer — those are
             * the only DML statements that emit self-contained shape
             * (subquery-based DELETEs + INSERT OR REPLACE...SELECT
             * FROM <MATCH>). Plain `INSERT INTO` (CREATE per-row) and
             * naked `DELETE FROM` (REMOVE/DELETE) still need the
             * legacy compound + dropped semantics. */
            bool self_contained = peek &&
                strstr(peek, "INSERT OR REPLACE") != NULL;
            if (self_contained) {
                raw_dml = strdup(peek);
                mixed_dml = true;
                dbuf_clear(&ctx->unified_builder->raw_output);
            }
            /* else: leave raw_output in builder. The compound form
             * appended at finalize remains in sql_buffer; prepare_v2
             * takes only the SELECT half, DML silently dropped. */
        }
    }

    /* Prepend CTE prefix if we have variable-length relationships.
     * After this call, ctx->cte_prefix_len holds the length of
     * the CTE prefix at the start of sql_buffer (0 if none added). */
    prepend_cte_to_sql(ctx);

    /* T-0310: build pre_exec_dml = CTE prefix + DML.
     * The CTE definitions sit in sql_buffer[0 .. cte_prefix_len); we
     * copy them in front of the DML so SQLite can resolve CTE-bound
     * variable references that appear in the DML. */
    if (mixed_dml && raw_dml) {
        size_t prefix_len = ctx->cte_prefix_len;
        if (prefix_len > 0 && prefix_len <= ctx->sql_size) {
            size_t dml_len = strlen(raw_dml);
            char *prefixed = malloc(prefix_len + dml_len + 1);
            if (prefixed) {
                memcpy(prefixed, ctx->sql_buffer, prefix_len);
                memcpy(prefixed + prefix_len, raw_dml, dml_len + 1);
                free(raw_dml);
                result->pre_exec_dml = prefixed;
            } else {
                result->pre_exec_dml = raw_dml;
            }
        } else {
            result->pre_exec_dml = raw_dml;
        }
    }

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
     *
     * Preserve the left branch's CTE buffers (cte and pre_cte) across
     * the reset — both branches share the same WITH clause in the
     * assembled SQL, and freeing the builder would lose branch-1's
     * CTE definitions. Without this, UNION-of-UNWINDs fails with
     * "no such table: _unwind_0" because the second branch's
     * prepend_cte_to_sql only sees its own CTEs.
     */
    char *saved_cte = NULL, *saved_pre_cte = NULL;
    int saved_cte_count = 0, saved_pre_count = 0;
    bool saved_pre_recursive = false;
    if (ctx->unified_builder) {
        if (!dbuf_is_empty(&ctx->unified_builder->cte)) {
            saved_cte = strdup(dbuf_get(&ctx->unified_builder->cte));
            saved_cte_count = ctx->unified_builder->cte_count;
        }
        if (!dbuf_is_empty(&ctx->unified_builder->pre_cte)) {
            saved_pre_cte = strdup(dbuf_get(&ctx->unified_builder->pre_cte));
            saved_pre_count = ctx->unified_builder->pre_cte_count;
            saved_pre_recursive = ctx->unified_builder->pre_cte_recursive;
        }
        sql_builder_free(ctx->unified_builder);
        ctx->unified_builder = sql_builder_create();
        if (saved_cte) {
            dbuf_append(&ctx->unified_builder->cte, saved_cte);
            ctx->unified_builder->cte_count = saved_cte_count;
            free(saved_cte);
        }
        if (saved_pre_cte) {
            dbuf_append(&ctx->unified_builder->pre_cte, saved_pre_cte);
            ctx->unified_builder->pre_cte_count = saved_pre_count;
            ctx->unified_builder->pre_cte_recursive = saved_pre_recursive;
            free(saved_pre_cte);
        }
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

    /* T-0311 (E2): each branch's transform_single_query_sql calls
     * finalize at its own end (in_union=true makes it append rather
     * than reset, preserving the left branch + UNION + right). No
     * combined finalize needed here. */

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

            case AST_NODE_CALL_SUBQUERY:
                ctx->has_error = true;
                ctx->error_message = strdup("CALL {} subquery requires executor-level handling");
                return -1;

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

    /* T-0311/T-0312: finalize at end of clause loop. Fires for any
     * builder state — SELECT shape OR write-only DML (raw_output).
     * finalize_sql_generation is `in_union`-aware: appends to
     * sql_buffer when in_union=true (so left-branch SQL is preserved
     * across `UNION` separator) and resets+writes when false.
     * sql_builder_to_string (T-0312) emits raw_output-only when no
     * SELECT/FROM is present, removing the need for the legacy drain
     * shim. */
    if (ctx->unified_builder &&
        (ctx->unified_builder->select_count > 0 ||
         !dbuf_is_empty(&ctx->unified_builder->from) ||
         !dbuf_is_empty(&ctx->unified_builder->raw_output))) {
        if (finalize_sql_generation(ctx) < 0) {
            ctx->has_error = true;
            ctx->error_message = strdup("Failed to finalize SQL generation");
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

    /* T-0312 (E3): raw_output drain shim removed. transform_single_query_sql
     * already calls finalize at clause-loop end which emits both
     * SELECT and write-only DML shapes via sql_builder_to_string. */

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
    /* min_hops == -1 means unspecified → default to 1. Explicit 0 must be
     * preserved so the zero-hop case (`(a)-[*0]->(b)` ⇒ a = b) works. */
    int min_hops = range->min_hops >= 0 ? range->min_hops : 1;
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

    /* Zero-hop base case (only when explicitly requested via *0..N):
     * each node maps to itself with depth=0. */
    if (min_hops == 0) {
        dbuf_appendf(&cte_query,
            "SELECT n.id, n.id, 0, CAST(n.id AS TEXT), ',' || n.id || ',' "
            "FROM nodes n UNION ALL ");
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
        { char *esc = escape_sql_string(rel->type);
          dbuf_appendf(&cte_query, " WHERE e.type = '%s'", esc ? esc : rel->type);
          free(esc); }
    } else if (rel->types && rel->types->count > 0) {
        dbuf_append(&cte_query, " WHERE (");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) {
                dbuf_append(&cte_query, " OR ");
            }
            cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
            { char *esc = escape_sql_string(type_lit->value.string);
              dbuf_appendf(&cte_query, "e.type = '%s'", esc ? esc : type_lit->value.string);
              free(esc); }
        }
        dbuf_append(&cte_query, ")");
    }

    /* Recursive case — only recurse from depth >= 1 rows. The depth=0
     * base case is for self-bind (zero-hop); recursing from it would
     * duplicate edges already emitted by the depth=1 base case. */
    dbuf_append(&cte_query, " UNION ALL ");
    dbuf_appendf(&cte_query,
        "SELECT cte.start_id, e.%s, cte.depth + 1, "
        "cte.path_ids || ',' || e.%s, "
        "cte.visited || e.%s || ',' "
        "FROM %s cte "
        "JOIN edges e ON e.%s = cte.end_id "
        "WHERE cte.depth >= 1 AND cte.depth < %d",
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
        { char *esc = escape_sql_string(rel->type);
          dbuf_appendf(&cte_query, " AND e.type = '%s'", esc ? esc : rel->type);
          free(esc); }
    } else if (rel->types && rel->types->count > 0) {
        dbuf_append(&cte_query, " AND (");
        for (int t = 0; t < rel->types->count; t++) {
            if (t > 0) {
                dbuf_append(&cte_query, " OR ");
            }
            cypher_literal *type_lit = (cypher_literal*)rel->types->items[t];
            { char *esc = escape_sql_string(type_lit->value.string);
              dbuf_appendf(&cte_query, "e.type = '%s'", esc ? esc : type_lit->value.string);
              free(esc); }
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

    free(result->pre_exec_dml);

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