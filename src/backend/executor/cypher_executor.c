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
#include "executor/json_builder.h"
#include "executor/executor_internal.h"
#include "executor/query_patterns.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"
#include "transform/transform_validate.h"
#include "runtime/gql_error.h"

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
    if (!executor) return;
    for (int i = 0; executor->stmt_cache && i < executor->stmt_cache_count; i++) {
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
    for (int i = 0; i < executor->helper_count; i++) {
        if (executor->helper_stmts[i]) sqlite3_finalize(executor->helper_stmts[i]);
        executor->helper_stmts[i] = NULL;
        free(executor->helper_sql[i]);
        executor->helper_sql[i] = NULL;
    }
    executor->helper_count = 0;
}

sqlite3_stmt *cypher_executor_helper_stmt(cypher_executor *executor, const char *sql)
{
    if (!executor || !executor->db || !sql) return NULL;
    for (int i = 0; i < executor->helper_count; i++) {
        if (executor->helper_sql[i] && strcmp(executor->helper_sql[i], sql) == 0) {
            sqlite3_reset(executor->helper_stmts[i]);
            sqlite3_clear_bindings(executor->helper_stmts[i]);
            return executor->helper_stmts[i];
        }
    }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(executor->db, sql, -1, &st, NULL) != SQLITE_OK) {
        sqlite3_finalize(st);
        return NULL;
    }
    int slot = executor->helper_count;
    if (slot == GQL_HELPER_STMT_MAX) {
        /* full: drop the oldest */
        sqlite3_finalize(executor->helper_stmts[0]);
        free(executor->helper_sql[0]);
        memmove(&executor->helper_stmts[0], &executor->helper_stmts[1], (GQL_HELPER_STMT_MAX - 1) * sizeof(sqlite3_stmt*));
        memmove(&executor->helper_sql[0], &executor->helper_sql[1], (GQL_HELPER_STMT_MAX - 1) * sizeof(char*));
        slot = GQL_HELPER_STMT_MAX - 1;
    } else {
        executor->helper_count++;
    }
    executor->helper_stmts[slot] = st;
    executor->helper_sql[slot] = strdup(sql);
    return st;
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
        { char bm[300]; snprintf(bm, sizeof(bm), "Failed to bind query parameters: %s", sqlite3_errmsg(ex->db)); set_result_error(result, bm); }
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
static cypher_result *execute_ast_guarded(cypher_executor *executor, ast_node *ast);

/* A string as a JSON string literal (owned). */
static char *profile_json_string(const char *t)
{
    if (!t) t = "";
    size_t n = strlen(t);
    char *q = malloc(n * 6 + 3), *o = q;
    if (!q) return NULL;
    *o++ = '"';
    for (const unsigned char *c = (const unsigned char *)t; *c; c++) {
        if (*c == '"' || *c == '\\') { *o++ = '\\'; *o++ = (char)*c; }
        else if (*c == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (*c < 0x20) o += sprintf(o, "\\u%04x", *c);
        else *o++ = (char)*c;
    }
    *o++ = '"'; *o = 0;
    return q;
}

/* Milliseconds from the SQLite VFS clock (portable, also on Windows). */
static double profile_now_ms(sqlite3 *db)
{
    (void)db;
    sqlite3_vfs *vfs = sqlite3_vfs_find(NULL);
    sqlite3_int64 t = 0;
    if (vfs && vfs->iVersion >= 2 && vfs->xCurrentTimeInt64) vfs->xCurrentTimeInt64(vfs, &t);
    return (double)t;
}

/* PROFILE <query> (GQLITE-T-0135): run the query, then give one row: the
 * dispatch pattern, the clauses, the SQL of the transform and its SQLite
 * plan (EXPLAIN QUERY PLAN), the row count, the time and the writes. */
static void execute_profile(cypher_executor *executor, cypher_query *query, cypher_result *result)
{
    clause_flags flags = analyze_query_clauses(query);
    const query_pattern *pattern = find_matching_pattern(flags);
    const char *pattern_name = pattern ? pattern->name : "NONE";
    char *flags_str = strdup(clause_flags_to_string(flags));

    /* the SQL and its plan; a write pipeline may run other statements */
    char *sql = NULL;
    json_builder plan;
    jbuf_init(&plan, 256);
    jbuf_start_array(&plan);
    cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
    if (ctx && cypher_transform_generate_sql(ctx, query) >= 0 && !ctx->has_error && ctx->sql_buffer) {
        sql = strdup(ctx->sql_buffer);
        char *eqp = sqlite3_mprintf("EXPLAIN QUERY PLAN %s", sql);
        sqlite3_stmt *st = NULL;
        if (eqp && sqlite3_prepare_v2(executor->db, eqp, -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                char *d = profile_json_string((const char *)sqlite3_column_text(st, 3));
                if (d) { jbuf_add_item(&plan, "%s", d); free(d); }
            }
        }
        sqlite3_finalize(st);
        sqlite3_free(eqp);
    }
    if (ctx) cypher_transform_free_context(ctx);
    jbuf_end_array(&plan);

    /* run it */
    query->profile = false;
    cypher_result *run = create_empty_result();
    double t0 = profile_now_ms(executor->db);
    if (run) dispatch_query_pattern(executor, query, run);
    double t1 = profile_now_ms(executor->db);
    query->profile = true;
    if (!run || !run->success) {
        set_result_error(result, run && run->error_message ? run->error_message : "PROFILE: the query failed");
        if (run) cypher_result_free(run);
        free(sql); free(flags_str); jbuf_free(&plan);
        return;
    }

    static const char *names[] = {"pattern", "clauses", "sql", "plan", "rows", "time_ms",
                                  "nodes_created", "nodes_deleted", "relationships_created",
                                  "relationships_deleted", "properties_set"};
    int nc = (int)(sizeof(names) / sizeof(names[0]));
    result->column_names = calloc((size_t)nc, sizeof(char *));
    result->data = calloc(1, sizeof(char **));
    result->data_types = calloc(1, sizeof(int *));
    if (result->data) result->data[0] = calloc((size_t)nc, sizeof(char *));
    if (result->data_types) result->data_types[0] = calloc((size_t)nc, sizeof(int));
    if (!result->column_names || !result->data || !result->data[0] ||
        !result->data_types || !result->data_types[0]) {
        set_result_error(result, "PROFILE: out of memory");
        cypher_result_free(run);
        free(sql); free(flags_str); jbuf_free(&plan);
        return;
    }
    result->column_count = nc;
    result->row_count = 1;
    for (int i = 0; i < nc; i++) result->column_names[i] = strdup(names[i]);
    char buf[64];
    result->data[0][0] = strdup(pattern_name);           result->data_types[0][0] = GQL_COL_TYPE_STRING;
    result->data[0][1] = flags_str;                       result->data_types[0][1] = GQL_COL_TYPE_STRING;
    result->data[0][2] = sql ? sql : NULL;                result->data_types[0][2] = sql ? GQL_COL_TYPE_STRING : SQLITE_NULL;
    result->data[0][3] = jbuf_take(&plan);                result->data_types[0][3] = GQL_COL_TYPE_JSON;
    snprintf(buf, sizeof(buf), "%d", run->row_count);
    result->data[0][4] = strdup(buf);                     result->data_types[0][4] = SQLITE_INTEGER;
    snprintf(buf, sizeof(buf), "%.3f", t1 - t0);
    result->data[0][5] = strdup(buf);                     result->data_types[0][5] = SQLITE_FLOAT;
    int counts[] = {run->nodes_created, run->nodes_deleted, run->relationships_created,
                    run->relationships_deleted, run->properties_set};
    for (int i = 0; i < 5; i++) {
        snprintf(buf, sizeof(buf), "%d", counts[i]);
        result->data[0][6 + i] = strdup(buf);
        result->data_types[0][6 + i] = SQLITE_INTEGER;
    }
    /* The writes count as writes: note_write reads these, and the cached
     * algorithm graph stayed stale after PROFILE CREATE (GQLITE-T-0444). */
    result->nodes_created = run->nodes_created;
    result->nodes_deleted = run->nodes_deleted;
    result->relationships_created = run->relationships_created;
    result->relationships_deleted = run->relationships_deleted;
    result->properties_set = run->properties_set;
    result->success = true;
    cypher_result_free(run);
}

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
                        set_result_error_ex(result,
                                            validate_err ? validate_err
                                                         : "Validation failed",
                                            GQL_ERR_VALIDATION, 0, 0);
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
                    if (query->profile && !query->explain) {
                        execute_profile(executor, query, result);
                        return result;
                    }
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
                        set_result_error_ex(result, uerr ? uerr : "UNION validation failed",
                                            GQL_ERR_VALIDATION, 0, 0);
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
                            { char bm[300]; snprintf(bm, sizeof(bm), "Failed to bind query parameters: %s", sqlite3_errmsg(executor->db)); set_result_error(result, bm); }
                            free(transform_result);
                            cypher_transform_free_context(ctx);
                            return result;
                        }
                    }

                    result->data = NULL;
                    result->row_count = 0;
                    /* Hidden RETURN tag columns are not result columns
                     * (GQLITE-A-0007). */
                    result->column_count = gql_visible_column_count(transform_result->stmt);

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
                            result->data_types[result->row_count][c] =
                                gql_result_cell_type(transform_result->stmt, c, result->column_count);
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

    /* CREATE / DROP / SHOW INDEX and CONSTRAINT (GQLITE-T-0008) */
    {
        cypher_result *sr = schema_command_try(executor, query);
        if (sr) return sr;
    }

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
            set_result_error_ex(result, "Internal parser error", GQL_ERR_INTERNAL, 0, 0);
        }
        return result;
    }

    /* Check for parse errors */
    if (!parse_result->ast) {
        CYPHER_DEBUG("Parser error: %s", parse_result->error_message ? parse_result->error_message : "Unknown error");
        cypher_result *result = create_empty_result();
        if (result) {
            /* Use the detailed parser error message and carry its location
             * so cypher() / cypher_rows can report line and column. */
            set_result_error_ex(result,
                                parse_result->error_message ? parse_result->error_message : "Failed to parse query",
                                GQL_ERR_PARSE,
                                parse_result->error_line,
                                parse_result->error_column);
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
    /* PROFILE runs the query each time and returns its profile, not rows
     * that the cache could replay (GQLITE-T-0135). */
    if (ast && ast->type == AST_NODE_QUERY && ((cypher_query *)ast)->profile)
        executor->stmt_capture = false;
    executor->captured_ctx = NULL;
    executor->captured_stmt = NULL;
    executor->captured_ret = NULL;
    cypher_result *result = execute_ast_guarded(executor, ast);
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

/* #134: true when the query can write. Reads skip the savepoint. */
static bool ast_may_write(ast_node *ast)
{
    if (!ast) return false;
    if (ast->type == AST_NODE_QUERY || ast->type == AST_NODE_SINGLE_QUERY) {
        cypher_query *q = (cypher_query *)ast;
        if (q->explain) return false;
        clause_flags f = analyze_query_clauses(q);
        return (f & (CLAUSE_CREATE | CLAUSE_MERGE | CLAUSE_SET | CLAUSE_DELETE |
                     CLAUSE_REMOVE | CLAUSE_FOREACH | CLAUSE_CALL)) != 0;
    }
    return ast->type != AST_NODE_MATCH;
}

/* #134: run one top-level query as a unit. A write query runs inside a
 * savepoint; when it fails, its writes are rolled back, so a failed query
 * leaves no node or property behind. Inside an open transaction, new
 * property keys are not cached, because a ROLLBACK can give their ids back. */
static cypher_result *execute_ast_guarded(cypher_executor *executor, ast_node *ast)
{
    if (!executor || !executor->db) return cypher_executor_execute_ast(executor, ast);

    property_key_cache *kc = executor->schema_mgr ? executor->schema_mgr->key_cache : NULL;
    bool saved_bypass = kc ? kc->bypass : false;
    if (kc) kc->bypass = !sqlite3_get_autocommit(executor->db);

    bool guarded = ast_may_write(ast) &&
        sqlite3_exec(executor->db, "SAVEPOINT graphqlite_query", NULL, NULL, NULL) == SQLITE_OK;
    long keys_before = kc ? kc->key_insertions : 0;

    cypher_result *result = cypher_executor_execute_ast(executor, ast);

    /* Constraints hold after each write query; a violation fails the query
     * and the rollback below undoes its writes (GQLITE-T-0008). */
    if (guarded && result && result->success) {
        char *violation = schema_check_constraints(executor);
        if (violation) {
            set_result_error(result, violation);
            free(violation);
        }
    }

    if (guarded) {
        if (!result || !result->success) {
            sqlite3_exec(executor->db, "ROLLBACK TO graphqlite_query", NULL, NULL, NULL);
            if (kc && kc->key_insertions != keys_before) {
                cypher_schema_clear_property_key_cache(executor->schema_mgr);
            }
            if (result) {
                result->nodes_created = 0;
                result->nodes_deleted = 0;
                result->relationships_created = 0;
                result->relationships_deleted = 0;
                result->properties_set = 0;
            }
        }
        if (sqlite3_exec(executor->db, "RELEASE graphqlite_query", NULL, NULL, NULL) != SQLITE_OK) {
            /* Outside a transaction the RELEASE is the commit. When it
             * fails (SQLITE_BUSY), undo the query and close the savepoint,
             * so no later query runs inside it. */
            char msg[512];
            snprintf(msg, sizeof(msg), "Failed to commit the query: %s", sqlite3_errmsg(executor->db));
            sqlite3_exec(executor->db, "ROLLBACK TO graphqlite_query", NULL, NULL, NULL);
            sqlite3_exec(executor->db, "RELEASE graphqlite_query", NULL, NULL, NULL);
            cypher_schema_clear_property_key_cache(executor->schema_mgr);
            if (result && result->success) set_result_error(result, msg);
        }
    }
    if (kc) kc->bypass = saved_bypass;
    return result;
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
    cypher_result *result = execute_ast_guarded(executor, ast);
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