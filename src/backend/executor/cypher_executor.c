/*
 * Cypher Execution Engine
 * Orchestrates parser, transformer, and schema manager for end-to-end query execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sqlite3.h>

#include "executor/cypher_executor.h"
#include "executor/executor_internal.h"
#include "executor/query_patterns.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"
#include "transform/transform_validate.h"

/* SQLite custom function: REVERSE(string) - reverses a string */
static void sqlite_reverse_func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 1) {
        sqlite3_result_error(context, "reverse() requires exactly 1 argument", -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    const unsigned char *input = sqlite3_value_text(argv[0]);
    if (!input) {
        sqlite3_result_null(context);
        return;
    }
    int len = sqlite3_value_bytes(argv[0]);

    /* Cypher reverse() is type-overloaded: reverses strings AND lists.
     * If the value is a JSON array (starts with '['), reverse the
     * elements via SQLite's json layer rather than treating the value
     * as a flat text string (which would produce ']2,1[' for
     * reverse([1,2])). */
    if (len > 0 && input[0] == '[') {
        sqlite3 *db = sqlite3_context_db_handle(context);
        sqlite3_stmt *stmt = NULL;
        /* Use json_each.rowid to step backwards. Wrap each element
         * with json() when it's a JSON container so nested arrays/
         * maps embed instead of being string-quoted. */
        const char *sql =
            "SELECT json_group_array("
            "  CASE WHEN type IN ('array','object') THEN json(value) ELSE value END"
            ") FROM (SELECT type, value FROM json_each(?1) ORDER BY rowid DESC)";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_value(stmt, 1, argv[0]);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *out = (const char*)sqlite3_column_text(stmt, 0);
                if (out) {
                    sqlite3_result_text(context, out, -1, SQLITE_TRANSIENT);
                } else {
                    sqlite3_result_null(context);
                }
            } else {
                sqlite3_result_null(context);
            }
            sqlite3_finalize(stmt);
            return;
        }
        /* On prepare failure fall through to text reverse — best effort. */
    }

    char *result = sqlite3_malloc(len + 1);
    if (!result) {
        sqlite3_result_error_nomem(context);
        return;
    }

    /* Reverse the string */
    for (int i = 0; i < len; i++) {
        result[i] = input[len - 1 - i];
    }
    result[len] = '\0';

    sqlite3_result_text(context, result, len, sqlite3_free);
}

#include "runtime/udf_register.h"
#include "transform/cypher_transform.h"
#include "parser/cypher_parser.h"
#include "runtime/gql_error.h"

/* Register custom SQLite functions needed for Cypher execution */
static int register_custom_functions(sqlite3 *db)
{
    int rc = sqlite3_create_function(db, "REVERSE", 1, SQLITE_UTF8, NULL,
                                      sqlite_reverse_func, NULL, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    /* SQLITE_BUSY (5) means we are being called from inside an active
     * statement (e.g. cypher() invoked from SQL) and the UDFs are already
     * registered by sqlite3_graphqlite_init. That's the normal case for
     * the loadable-extension path; just keep going. */
    int hrc = graphqlite_register_helper_udfs(db);
    if (hrc != SQLITE_OK && hrc != SQLITE_BUSY) {
        return -1;
    }
    return 0;
}

/* Helper functions moved to executor_helpers.c:
 * - get_label_string
 * - has_labels
 * - bind_params_from_json
 */

/* Performance timing instrumentation - enable with -DGRAPHQLITE_PERF_TIMING */


/* ---- Perf review F8: statement cache --------------------------------- */

static void stmt_cache_entry_free(stmt_cache_entry *e)
{
    if (e->stmt) sqlite3_finalize(e->stmt);
    if (e->ctx) cypher_transform_free_context(e->ctx);
    if (e->ast) cypher_parser_free_result(e->ast);
    free(e->text);
    memset(e, 0, sizeof(*e));
}

/* Remove entry i by swapping the last entry into its slot. */
static void stmt_cache_evict(cypher_executor *ex, int i)
{
    if (i < 0 || i >= ex->stmt_cache_count) return;
    stmt_cache_entry_free(&ex->stmt_cache[i]);
    ex->stmt_cache_count--;
    if (i != ex->stmt_cache_count) {
        ex->stmt_cache[i] = ex->stmt_cache[ex->stmt_cache_count];
        memset(&ex->stmt_cache[ex->stmt_cache_count], 0, sizeof(stmt_cache_entry));
    }
}

static void stmt_cache_clear(cypher_executor *ex)
{
    if (!ex->stmt_cache) return;
    for (int i = 0; i < ex->stmt_cache_count; i++) stmt_cache_entry_free(&ex->stmt_cache[i]);
    ex->stmt_cache_count = 0;
}

static stmt_cache_entry *stmt_cache_find(cypher_executor *ex, const char *text)
{
    if (!ex->stmt_cache_enabled || !ex->stmt_cache || !text) return NULL;
    for (int i = 0; i < ex->stmt_cache_count; i++) {
        stmt_cache_entry *e = &ex->stmt_cache[i];
        if (e->stmt && !e->in_use && strcmp(e->text, text) == 0) return e;
    }
    return NULL;
}

static void stmt_cache_insert(cypher_executor *ex, const char *text, ast_node *ast,
                              struct cypher_transform_context *ctx, sqlite3_stmt *stmt,
                              struct cypher_return *ret)
{
    if (ex->stmt_cache_count == GQL_STMT_CACHE_MAX) {
        int victim = -1;
        unsigned long long oldest = ~0ULL;
        for (int i = 0; i < ex->stmt_cache_count; i++) {
            if (!ex->stmt_cache[i].in_use && ex->stmt_cache[i].last_used < oldest) {
                oldest = ex->stmt_cache[i].last_used;
                victim = i;
            }
        }
        if (victim < 0) { /* everything in use: do not cache */
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            cypher_parser_free_result(ast);
            return;
        }
        stmt_cache_evict(ex, victim);
    }
    stmt_cache_entry *e = &ex->stmt_cache[ex->stmt_cache_count];
    e->text = strdup(text);
    if (!e->text) {
        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);
        cypher_parser_free_result(ast);
        return;
    }
    e->ast = ast;
    e->ctx = ctx;
    e->stmt = stmt;
    e->ret = ret;
    e->last_used = ++ex->stmt_cache_tick;
    e->in_use = false;
    ex->stmt_cache_count++;
}

void cypher_executor_release_statements(cypher_executor *executor)
{
    if (!executor || !executor->stmt_cache) return;
    for (int i = 0; i < executor->stmt_cache_count; i++) {
        if (executor->stmt_cache[i].stmt) {
            sqlite3_finalize(executor->stmt_cache[i].stmt);
            executor->stmt_cache[i].stmt = NULL;   /* entry is now a miss */
        }
    }
    if (executor->captured_stmt) {
        sqlite3_finalize(executor->captured_stmt);
        executor->captured_stmt = NULL;
    }
    /* Perf review F9: the schema manager's write statements too */
    cypher_schema_release_statements(executor->schema_mgr);
}

static int executor_trace_close_cb(unsigned type, void *arg, void *p, void *x)
{
    (void)p; (void)x;
    if (type != SQLITE_TRACE_CLOSE) return 0;
    executor_trace_ctx *tc = (executor_trace_ctx *)arg;
    if (!tc) return 0;
    if (tc->ex) {
        cypher_executor_release_statements(tc->ex);
        tc->ex->trace_ctx = NULL;     /* the executor must not touch tc again */
    }
    free(tc);
    return 0;
}

/* Run a cached read query: rebind, build rows, reset. */
static cypher_result *stmt_cache_execute(cypher_executor *ex, stmt_cache_entry *e)
{
    cypher_result *result = create_empty_result();
    if (!result) return NULL;
    e->in_use = true;
    sqlite3_reset(e->stmt);
    sqlite3_clear_bindings(e->stmt);
    if (ex->params_json && bind_params_from_json(e->stmt, ex->params_json) < 0) {
        set_result_error(result, "Failed to bind query parameters");
        e->in_use = false;
        return result;
    }
    int rc = build_query_results(ex, e->stmt, e->ret, result, e->ctx);
    sqlite3_reset(e->stmt);
    e->in_use = false;
    if (rc < 0) {
        /* Anything that broke a previously working statement (e.g. an
         * attached graph went away) invalidates the entry. */
        for (int i = 0; i < ex->stmt_cache_count; i++) {
            if (&ex->stmt_cache[i] == e) { stmt_cache_evict(ex, i); break; }
        }
        return result;
    }
    result->success = true;
    e->last_used = ++ex->stmt_cache_tick;
    return result;
}

/* Create execution engine */
cypher_executor* cypher_executor_create(sqlite3 *db)
{
    if (!db) {
        return NULL;
    }
    
    cypher_executor *executor = calloc(1, sizeof(cypher_executor));
    if (!executor) {
        return NULL;
    }
    
    executor->db = db;
    executor->schema_initialized = false;
    executor->params_json = NULL;

    /* Register custom SQLite functions */
    if (register_custom_functions(db) < 0) {
        free(executor);
        return NULL;
    }

    /* Perf review F8: statement cache. Live prepared statements make
     * sqlite3_close() (v1) fail with SQLITE_BUSY, so finalize them from the
     * SQLITE_TRACE_CLOSE callback, which SQLite invokes before that check.
     * An application that installs its own trace hook afterwards replaces
     * this one; sqlite3_close_v2() callers are unaffected either way. */
    {
        const char *env = getenv("GQL_STMT_CACHE");
        executor->stmt_cache_enabled = !(env && env[0] == '0');
        executor->stmt_cache = calloc(GQL_STMT_CACHE_MAX, sizeof(stmt_cache_entry));
        if (!executor->stmt_cache) executor->stmt_cache_enabled = false;
        if (executor->stmt_cache_enabled) {
            executor->trace_ctx = calloc(1, sizeof(executor_trace_ctx));
            if (executor->trace_ctx) {
                executor->trace_ctx->ex = executor;
                sqlite3_trace_v2(db, SQLITE_TRACE_CLOSE, executor_trace_close_cb, executor->trace_ctx);
            } else {
                executor->stmt_cache_enabled = false;
            }
        }
    }

    /* Create schema manager */
    executor->schema_mgr = cypher_schema_create_manager(db);
    if (!executor->schema_mgr) {
        free(executor);
        return NULL;
    }
    
    /* Initialize schema */
    if (cypher_schema_initialize(executor->schema_mgr) < 0) {
        cypher_schema_free_manager(executor->schema_mgr);
        free(executor);
        return NULL;
    }
    
    executor->schema_initialized = true;
    
    CYPHER_DEBUG("Created cypher executor with initialized schema");
    
    return executor;
}

void cypher_executor_free(cypher_executor *executor)
{
    if (!executor) {
        return;
    }

    cypher_executor_release_statements(executor);
    stmt_cache_clear(executor);
    free(executor->stmt_cache);
    /* Detach from the close hook; the hook frees trace_ctx when it fires
     * (or it is leaked harmlessly if the connection is never closed). */
    if (executor->trace_ctx) executor->trace_ctx->ex = NULL;
    if (executor->captured_stmt) sqlite3_finalize(executor->captured_stmt);
    if (executor->captured_ctx) cypher_transform_free_context(executor->captured_ctx);

    cypher_schema_free_manager(executor->schema_mgr);
    free(executor);
    
    CYPHER_DEBUG("Freed cypher executor");
}

/* Forward declarations - all are non-static since declared in executor_internal.h */
/* Functions moved to extracted modules are declared there too */



/* Execute AST node */
static void note_write(cypher_executor *executor, const cypher_result *result);

cypher_result* cypher_executor_execute_ast(cypher_executor *executor, ast_node *ast)
{
    if (!executor || !ast) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor or AST");
        }
        return result;
    }
    
    if (!executor->schema_initialized) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Schema not initialized");
        }
        return result;
    }
    
    cypher_result *result = create_empty_result();
    if (!result) {
        return NULL;
    }
    
    CYPHER_DEBUG("Executing AST node type: %d", ast->type);
    
    /* Handle different query types */
    switch (ast->type) {
        case AST_NODE_QUERY:
        case AST_NODE_SINGLE_QUERY:
            /* Query node - cast the AST node to cypher_query and process its clauses */  
            {
                cypher_query *query = (cypher_query*)ast;
                CYPHER_DEBUG("Found query node with %d clauses", query->clauses ? query->clauses->count : 0);

                /* GQLITE-T-0230: compile-time argument-type validation.
                 * Rejects openCypher type violations that the grammar accepts
                 * (e.g. `RETURN NOT 1`, `RETURN 'a' AND true`) before they
                 * reach the transform layer. */
                {
                    char *validate_err = NULL;
                    if (transform_validate_query(query, &validate_err) < 0) {
                        set_result_error(result,
                                         validate_err ? validate_err
                                                       : "Validation failed");
                        if (validate_err) free(validate_err);
                        return result;
                    }
                }

                /* Runtime validation for parameter-driven SKIP/LIMIT
                 * (openCypher rejects negative or non-integer values).
                 * Spec violations on literals are caught at compile time
                 * by transform_validate_query; the parameter path needs
                 * params_json which only exists at execution. */
                if (executor->params_json && query->clauses) {
                    for (int ci = 0; ci < query->clauses->count; ci++) {
                        ast_node *cl = query->clauses->items[ci];
                        if (!cl) continue;
                        ast_node *skip = NULL, *limit = NULL;
                        if (cl->type == AST_NODE_RETURN) {
                            cypher_return *r = (cypher_return *)cl;
                            skip = r->skip; limit = r->limit;
                        } else if (cl->type == AST_NODE_WITH) {
                            cypher_with *w = (cypher_with *)cl;
                            skip = w->skip; limit = w->limit;
                        }
                        const char *err_label = NULL;
                        const char *err_param = NULL;
                        bool err_neg = false, err_nonint = false;
                        ast_node *targets[2] = { skip, limit };
                        const char *labels[2] = { "SKIP", "LIMIT" };
                        for (int t = 0; t < 2 && !err_label; t++) {
                            if (!targets[t] || targets[t]->type != AST_NODE_PARAMETER) continue;
                            cypher_parameter *p = (cypher_parameter *)targets[t];
                            if (!p->name) continue;
                            property_type pt;
                            property_value pv; property_value_init(&pv);
                            int rc = get_param_value(executor->params_json, p->name, &pt, &pv);
                            if (rc == 0) {
                                if (pt == PROP_TYPE_INTEGER) {
                                    if (pv.as_int < 0) {
                                        err_label = labels[t]; err_param = p->name; err_neg = true;
                                    }
                                } else if (pt == PROP_TYPE_REAL) {
                                    err_label = labels[t]; err_param = p->name; err_nonint = true;
                                } else {
                                    err_label = labels[t]; err_param = p->name; err_nonint = true;
                                }
                            }
                            property_value_free(&pv);
                        }
                        if (err_label) {
                            char buf[256];
                            if (err_neg) {
                                snprintf(buf, sizeof(buf),
                                    "SyntaxError: NegativeIntegerArgument: %s parameter `%s` must be non-negative",
                                    err_label, err_param ? err_param : "?");
                            } else {
                                snprintf(buf, sizeof(buf),
                                    "SyntaxError: InvalidArgumentType: %s parameter `%s` must be an integer",
                                    err_label, err_param ? err_param : "?");
                            }
                            set_result_error(result, buf);
                            return result;
                        }
                    }
                }

                if (query->clauses) {
                    /* Handle EXPLAIN - return generated SQL and pattern info */
                    if (query->explain) {
                        CYPHER_DEBUG("EXPLAIN mode - returning generated SQL and pattern info");

                        /* Analyze query to find matched pattern */
                        clause_flags flags = analyze_query_clauses(query);
                        const query_pattern *pattern = find_matching_pattern(flags);
                        const char *pattern_name = pattern ? pattern->name : "NONE";
                        const char *flags_str = clause_flags_to_string(flags);

                        cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
                        if (!ctx) {
                            set_result_error(result, "Failed to create transform context");
                            return result;
                        }

                        /* Transform the query to SQL */
                        int transform_status = cypher_transform_generate_sql(ctx, query);
                        if (transform_status < 0 || ctx->has_error) {
                            set_result_error(result, ctx->error_message ? ctx->error_message : "Transform error");
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        /* Return pattern info + SQL as formatted output */
                        result->column_count = 1;
                        result->row_count = 1;
                        result->data = malloc(sizeof(char**));
                        result->data[0] = malloc(sizeof(char*));

                        /* Format: Pattern: NAME\nClauses: FLAGS\nSQL: query */
                        const char *sql = ctx->sql_buffer ? ctx->sql_buffer : "";
                        size_t len = strlen(pattern_name) + strlen(flags_str) + strlen(sql) + 64;
                        char *explain_output = malloc(len);
                        snprintf(explain_output, len, "Pattern: %s\nClauses: %s\nSQL: %s",
                                 pattern_name, flags_str, sql);
                        result->data[0][0] = explain_output;
                        result->success = true;

                        cypher_transform_free_context(ctx);
                        return result;
                    }

                    /* Table-driven pattern dispatch */
                    if (dispatch_query_pattern(executor, query, result) < 0) {
                        return result; /* Error already set */
                    }
                } else {
                    CYPHER_DEBUG("No clauses found in query");
                }
            }
            break;
            
        case AST_NODE_CREATE:
            if (execute_create_clause(executor, (cypher_create*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;

        case AST_NODE_MERGE:
            if (execute_merge_clause(executor, (cypher_merge*)ast, result, NULL, NULL) < 0) {
                return result; /* Error already set */
            }
            break;

        case AST_NODE_SET:
            if (execute_set_clause(executor, (cypher_set*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;
            
        case AST_NODE_MATCH:
            if (execute_match_clause(executor, (cypher_match*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;
            
        case AST_NODE_UNION:
            /* UNION query - transform and execute via the transform layer */
            {
                CYPHER_DEBUG("Executing UNION query");
                /* Validate UNION shape (column agreement, no UNION/UNION ALL
                 * mixing) before transform. */
                {
                    char *uerr = NULL;
                    if (transform_validate_union((cypher_union *)ast, &uerr) < 0) {
                        set_result_error(result, uerr ? uerr : "UNION validation failed");
                        if (uerr) free(uerr);
                        return result;
                    }
                }
                cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
                if (!ctx) {
                    set_result_error(result, "Failed to create transform context");
                    return result;
                }

                /* The transform layer handles UNION queries directly when passed the union node */
                cypher_query_result *transform_result = cypher_transform_query(ctx, (cypher_query*)ast);
                if (!transform_result) {
                    set_result_error(result, "Failed to transform UNION query");
                    cypher_transform_free_context(ctx);
                    return result;
                }

                if (transform_result->has_error) {
                    set_result_error(result, transform_result->error_message ? transform_result->error_message : "UNION transform error");
                    free(transform_result);
                    cypher_transform_free_context(ctx);
                    return result;
                }

                /* Execute the prepared statement */
                if (transform_result->stmt) {
                    /* Bind parameters if provided */
                    if (executor->params_json) {
                        if (bind_params_from_json(transform_result->stmt, executor->params_json) < 0) {
                            set_result_error(result, "Failed to bind query parameters");
                            free(transform_result);
                            cypher_transform_free_context(ctx);
                            return result;
                        }
                    }

                    result->data = NULL;
                    result->row_count = 0;
                    result->column_count = sqlite3_column_count(transform_result->stmt);

                    /* Get column names from the SQL result */
                    result->column_names = malloc(result->column_count * sizeof(char*));
                    if (result->column_names) {
                        for (int c = 0; c < result->column_count; c++) {
                            const char *name = sqlite3_column_name(transform_result->stmt, c);
                            result->column_names[c] = name ? strdup(name) : NULL;
                        }
                    }

                    /* Collect results with type information */
                    while (sqlite3_step(transform_result->stmt) == SQLITE_ROW) {
                        /* Allocate/resize data and data_types arrays */
                        {
                            char ***new_data = realloc(result->data, (result->row_count + 1) * sizeof(char**));
                            if (!new_data) {
                                set_result_error(result, "Memory allocation failed for result data");
                                sqlite3_finalize(transform_result->stmt);
                                cypher_transform_free_context(ctx);
                                return result;
                            }
                            result->data = new_data;
                            result->data[result->row_count] = calloc(result->column_count, sizeof(char*));

                            int **new_types = realloc(result->data_types, (result->row_count + 1) * sizeof(int*));
                            if (!new_types) {
                                set_result_error(result, "Memory allocation failed for result data types");
                                sqlite3_finalize(transform_result->stmt);
                                cypher_transform_free_context(ctx);
                                return result;
                            }
                            result->data_types = new_types;
                            result->data_types[result->row_count] = calloc(result->column_count, sizeof(int));
                        }

                        for (int c = 0; c < result->column_count; c++) {
                            /* Store the SQLite type. If the UDF that
                             * produced this cell tagged it with the
                             * boolean subtype, override the type with
                             * GQL_COL_TYPE_BOOLEAN so the JSON renderer
                             * emits an unquoted JSON boolean (I-0040 M13). */
                            sqlite3_value *vv = sqlite3_column_value(transform_result->stmt, c);
                            unsigned int sub = vv ? sqlite3_value_subtype(vv) : 0;
                            if (sub == GQL_SUBTYPE_BOOLEAN) {
                                result->data_types[result->row_count][c] = GQL_COL_TYPE_BOOLEAN;
                            } else {
                                result->data_types[result->row_count][c] = sqlite3_column_type(transform_result->stmt, c);
                            }
                            const char *val = (const char*)sqlite3_column_text(transform_result->stmt, c);
                            result->data[result->row_count][c] = val ? strdup(val) : NULL;
                        }
                        result->row_count++;
                    }
                    sqlite3_finalize(transform_result->stmt);
                }

                result->success = true;
                free(transform_result);
                cypher_transform_free_context(ctx);
            }
            break;

        default:
            set_result_error(result, "Unsupported query type");
            return result;
    }
    
    /* If we got here, execution was successful */
    result->success = true;
    
    return result;
}

/* Execute query string */
cypher_result* cypher_executor_execute(cypher_executor *executor, const char *query)
{
    if (!executor || !query) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor or query");
        }
        return result;
    }

    CYPHER_DEBUG("Executing query: %s", query);

    /* Perf review F8: cached read query? */
    {
        stmt_cache_entry *hit = stmt_cache_find(executor, query);
        if (hit) {
            CYPHER_DEBUG("Statement cache hit");
            return stmt_cache_execute(executor, hit);
        }
    }

#ifdef GRAPHQLITE_PERF_TIMING
    struct timespec t_start, t_parse, t_exec, t_cleanup;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif

    /* Parse query to AST with extended error handling */
    CYPHER_DEBUG("Parsing query: '%s'", query);
    cypher_parse_result *parse_result = parse_cypher_query_ext(query);
    if (!parse_result) {
        CYPHER_DEBUG("Parser returned NULL");
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Internal parser error");
        }
        return result;
    }

    /* Check for parse errors */
    if (!parse_result->ast) {
        CYPHER_DEBUG("Parser error: %s", parse_result->error_message ? parse_result->error_message : "Unknown error");
        cypher_result *result = create_empty_result();
        if (result) {
            /* Use the detailed parser error message */
            set_result_error(result, parse_result->error_message ? parse_result->error_message : "Failed to parse query");
        }
        cypher_parse_result_free(parse_result);
        return result;
    }

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_parse);
#endif

    ast_node *ast = parse_result->ast;

    CYPHER_DEBUG("Parser returned AST with type=%d, data=%p", ast->type, ast->data);

    /* Execute AST. Perf review F8: arm statement capture for this one
     * text-path execution; a read handler parks its statement instead of
     * finalizing it, and it becomes a cache entry below. */
    executor->stmt_capture = executor->stmt_cache_enabled;
    executor->captured_ctx = NULL;
    executor->captured_stmt = NULL;
    executor->captured_ret = NULL;
    cypher_result *result = cypher_executor_execute_ast(executor, ast);
    executor->stmt_capture = false;

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_exec);
#endif

    /* Clean up parse result (includes AST) - note: don't free AST separately */
    parse_result->ast = NULL;  /* Prevent double-free since execute_ast may have taken ownership */
    cypher_parse_result_free(parse_result);

    if (executor->captured_stmt && result && result->success) {
        /* The AST now belongs to the cache entry. */
        stmt_cache_insert(executor, query, ast, executor->captured_ctx,
                          executor->captured_stmt, executor->captured_ret);
    } else {
        if (executor->captured_stmt) sqlite3_finalize(executor->captured_stmt);
        if (executor->captured_ctx) cypher_transform_free_context(executor->captured_ctx);
        /* Clean up AST */
        cypher_parser_free_result(ast);
    }
    executor->captured_ctx = NULL;
    executor->captured_stmt = NULL;
    executor->captured_ret = NULL;

    note_write(executor, result);

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_cleanup);
    double parse_ms = (t_parse.tv_sec - t_start.tv_sec) * 1000.0 + (t_parse.tv_nsec - t_start.tv_nsec) / 1000000.0;
    double exec_ms = (t_exec.tv_sec - t_parse.tv_sec) * 1000.0 + (t_exec.tv_nsec - t_parse.tv_nsec) / 1000000.0;
    double cleanup_ms = (t_cleanup.tv_sec - t_exec.tv_sec) * 1000.0 + (t_cleanup.tv_nsec - t_exec.tv_nsec) / 1000000.0;
    CYPHER_DEBUG("TIMING: parse=%.2fms, exec=%.2fms, cleanup=%.2fms", parse_ms, exec_ms, cleanup_ms);
#endif

    return result;
}

/* Mark the connection's CSR graph cache stale after a successful write, so
 * the next algorithm call rebuilds it (perf review: cache did not track
 * writes). */
static void note_write(cypher_executor *executor, const cypher_result *result)
{
    if (!executor || !result || !result->success) return;
    if (result->nodes_created || result->relationships_created ||
        result->nodes_deleted || result->relationships_deleted || result->properties_set) {
        executor->graph_dirty = true;
    }
}

/* Execute Cypher query with parameters */
cypher_result* cypher_executor_execute_params(cypher_executor *executor, const char *query, const char *params_json)
{
    if (!executor) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor");
        }
        return result;
    }

    /* Set params for this execution */
    executor->params_json = params_json;

    /* Execute the query */
    cypher_result *result = cypher_executor_execute(executor, query);

    /* Clear params */
    executor->params_json = NULL;

    return result;
}

/* Execute AST with parameters */
cypher_result* cypher_executor_execute_ast_params(cypher_executor *executor, ast_node *ast, const char *params_json)
{
    if (!executor) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor");
        }
        return result;
    }

    /* Set params for this execution */
    executor->params_json = params_json;

    /* Execute the AST */
    cypher_result *result = cypher_executor_execute_ast(executor, ast);
    note_write(executor, result);

    /* Clear params */
    executor->params_json = NULL;

    return result;
}

/* Free result */
void cypher_result_free(cypher_result *result)
{
    if (!result) {
        return;
    }
    
    free(result->error_message);
    
    if (result->column_names) {
        for (int i = 0; i < result->column_count; i++) {
            free(result->column_names[i]);
        }
        free(result->column_names);
    }
    
    if (result->data) {
        for (int row = 0; row < result->row_count; row++) {
            if (result->data[row]) {
                for (int col = 0; col < result->column_count; col++) {
                    free(result->data[row][col]);
                }
                free(result->data[row]);
            }
        }
        free(result->data);
    }

    if (result->data_types) {
        for (int row = 0; row < result->row_count; row++) {
            free(result->data_types[row]);
        }
        free(result->data_types);
    }

    if (result->agtype_data) {
        for (int row = 0; row < result->row_count; row++) {
            if (result->agtype_data[row]) {
                for (int col = 0; col < result->column_count; col++) {
                    agtype_value_free(result->agtype_data[row][col]);
                }
                free(result->agtype_data[row]);
            }
        }
        free(result->agtype_data);
    }
    
    free(result);
}

/* Print result */
void cypher_result_print(cypher_result *result)
{
    if (!result) {
        printf("NULL result\n");
        return;
    }
    
    if (!result->success) {
        printf("Query failed: %s\n", result->error_message ? result->error_message : "Unknown error");
        return;
    }
    
    /* Print statistics for modification queries */
    if (result->nodes_created > 0 || result->nodes_deleted > 0 || result->relationships_created > 0 || result->relationships_deleted > 0 || result->properties_set > 0) {
        printf("Query executed successfully - nodes created: %d, relationships created: %d, nodes deleted: %d, relationships deleted: %d\n", 
               result->nodes_created, result->relationships_created, result->nodes_deleted, result->relationships_deleted);
    }
    
    /* Print result data */
    if (result->row_count > 0 && result->column_count > 0) {
        /* Print column headers */
        for (int col = 0; col < result->column_count; col++) {
            printf("%-15s", result->column_names[col]);
        }
        printf("\n");
        
        /* Print separator */
        for (int col = 0; col < result->column_count; col++) {
            printf("%-15s", "---------------");
        }
        printf("\n");
        
        /* Print data rows */
        for (int row = 0; row < result->row_count; row++) {
            for (int col = 0; col < result->column_count; col++) {
                printf("%-15s", result->data[row][col]);
            }
            printf("\n");
        }
    }
}

/* Utility functions */
bool cypher_executor_is_ready(cypher_executor *executor)
{
    return executor && executor->schema_initialized;
}

const char* cypher_executor_get_last_error(cypher_executor *executor)
{
    UNUSED_PARAMETER(executor);
    return "Not implemented";
}