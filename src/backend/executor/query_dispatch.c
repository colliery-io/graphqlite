/*
 * query_dispatch.c
 *    Table-driven query pattern dispatch for Cypher execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "executor/query_patterns.h"
#include "gql_thread_local.h"
#include "executor/executor_internal.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"
#include "runtime/gql_error.h"

/*
 * Clause extraction helpers - find specific clause types in a query
 */
static cypher_match *find_match_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH) {
            return (cypher_match *)clause;
        }
    }
    return NULL;
}

static cypher_return *find_return_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_RETURN) {
            return (cypher_return *)clause;
        }
    }
    return NULL;
}

static cypher_create *find_create_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_CREATE) {
            return (cypher_create *)clause;
        }
    }
    return NULL;
}

static cypher_merge *find_merge_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MERGE) {
            return (cypher_merge *)clause;
        }
    }
    return NULL;
}

static cypher_set *find_set_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_SET) {
            return (cypher_set *)clause;
        }
    }
    return NULL;
}

static cypher_delete *find_delete_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_DELETE) {
            return (cypher_delete *)clause;
        }
    }
    return NULL;
}

static cypher_remove *find_remove_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_REMOVE) {
            return (cypher_remove *)clause;
        }
    }
    return NULL;
}

static cypher_unwind *find_unwind_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_UNWIND) {
            return (cypher_unwind *)clause;
        }
    }
    return NULL;
}

static cypher_foreach *find_foreach_clause(cypher_query *query)
{
    if (!query || !query->clauses) return NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_FOREACH) {
            return (cypher_foreach *)clause;
        }
    }
    return NULL;
}

/*
 * Forward declarations for pattern handlers
 */
/* handle_generic_transform: now extern (decl in executor_internal.h) for I-0040 M1 */
static int handle_match_set(cypher_executor *executor, cypher_query *query,
                            cypher_result *result, clause_flags flags);
/* project_return_row_from_var_map, set_return_column_names moved to
 * executor_result_project.c (I-0040 M3) — decls in executor_internal.h */
static int handle_match_delete(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_match_remove(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_match_merge(cypher_executor *executor, cypher_query *query,
                              cypher_result *result, clause_flags flags);
static int handle_rowwise_write(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags);
static int handle_match_create(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_match_create_return(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags);
static int handle_match_return(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_create(cypher_executor *executor, cypher_query *query,
                         cypher_result *result, clause_flags flags);
static int handle_merge(cypher_executor *executor, cypher_query *query,
                        cypher_result *result, clause_flags flags);
static int handle_set(cypher_executor *executor, cypher_query *query,
                      cypher_result *result, clause_flags flags);
static int handle_foreach(cypher_executor *executor, cypher_query *query,
                          cypher_result *result, clause_flags flags);
static int handle_match_only(cypher_executor *executor, cypher_query *query,
                             cypher_result *result, clause_flags flags);
static int handle_unwind_create(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags);
static int handle_unwind_merge(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_return_only(cypher_executor *executor, cypher_query *query,
                              cypher_result *result, clause_flags flags);
/* handle_merge_with_pipeline moved to executor_merge_pipeline.c (I-0040 M2) — decl in executor_internal.h */
/* handle_call_subquery moved to executor_call_subquery.c (I-0040 M1) — decl in executor_internal.h */
static int handle_create_return(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags);
static int handle_unwind_create_return(cypher_executor *executor, cypher_query *query,
                                       cypher_result *result, clause_flags flags);
static int handle_unwind_merge_return(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags);
static int handle_merge_return(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);

/*
 * Pattern registry - ordered by priority (highest first)
 *
 * Patterns are matched by checking:
 *   1. All required clauses are present
 *   2. No forbidden clauses are present
 *   3. Highest priority wins among matches
 *
 * Pattern inventory from cypher_executor.c if-else chain (lines 258-750):
 */
static const query_pattern patterns[] = {
    /*
     * Priority 100: Most specific multi-clause patterns
     */
    {
        .name = "UNWIND+CREATE+RETURN",
        .required = CLAUSE_UNWIND | CLAUSE_CREATE | CLAUSE_RETURN,
        .forbidden = CLAUSE_MATCH | CLAUSE_MERGE,
        .handler = handle_unwind_create_return,
        .priority = 105
    },
    {
        .name = "UNWIND+CREATE",
        .required = CLAUSE_UNWIND | CLAUSE_CREATE,
        .forbidden = CLAUSE_RETURN | CLAUSE_MATCH,
        .handler = handle_unwind_create,
        .priority = 100
    },
    {
        .name = "UNWIND+MERGE+RETURN",
        .required = CLAUSE_UNWIND | CLAUSE_MERGE | CLAUSE_RETURN,
        .forbidden = CLAUSE_MATCH | CLAUSE_CREATE,
        .handler = handle_unwind_merge_return,
        .priority = 105
    },
    {
        .name = "UNWIND+MERGE",
        .required = CLAUSE_UNWIND | CLAUSE_MERGE,
        .forbidden = CLAUSE_RETURN | CLAUSE_MATCH,
        .handler = handle_unwind_merge,
        .priority = 100
    },
    {
        .name = "WITH+MATCH+RETURN",
        .required = CLAUSE_WITH | CLAUSE_MATCH | CLAUSE_RETURN,
        /* T-0317: MERGE has no case in transform_single_query_sql,
         * so a query with MERGE here would error "Unsupported clause
         * type". Forbid CLAUSE_MERGE to route MATCH+WITH+MERGE+...
         * to the dedicated handler (Merge5 [16]/[17]/[18]/[19]). */
        .forbidden = CLAUSE_MERGE,
        .handler = handle_generic_transform,
        .priority = 100
    },
    {
        .name = "MATCH+CREATE+RETURN",
        .required = CLAUSE_MATCH | CLAUSE_CREATE | CLAUSE_RETURN,
        .forbidden = CLAUSE_NONE,
        .handler = handle_match_create_return,
        .priority = 100
    },

    /*
     * Priority 90: MATCH + write operation patterns
     */
    /*
     * GQLITE-T-0371: row-wise write pipeline. A read prefix (MATCH / WITH /
     * UNWIND, or a leading CREATE) followed by DELETE / MERGE / WITH / SET
     * clauses and a RETURN. The prefix is evaluated once through the normal
     * read pipeline, then every row drives the write clauses in order
     * (DELETEs eagerly first, as Cypher requires), with scalar WITH
     * projections available to MERGE property maps.
     */
    {
        .name = "ROWWISE_WRITE (MERGE+DELETE)",
        .required = CLAUSE_MERGE | CLAUSE_DELETE,
        .forbidden = CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV,
        .handler = handle_rowwise_write,
        .priority = 96
    },
    {
        .name = "ROWWISE_WRITE (MERGE+UNWIND)",
        .required = CLAUSE_MERGE | CLAUSE_UNWIND,
        .forbidden = CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV,
        .handler = handle_rowwise_write,
        .priority = 96
    },
    {
        .name = "ROWWISE_WRITE (MATCH+WITH+MERGE)",
        .required = CLAUSE_MATCH | CLAUSE_WITH | CLAUSE_MERGE,
        .forbidden = CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV,
        .handler = handle_rowwise_write,
        .priority = 95
    },
    {
        .name = "MATCH+SET",
        .required = CLAUSE_MATCH | CLAUSE_SET,
        .forbidden = CLAUSE_WITH | CLAUSE_MERGE | CLAUSE_CREATE,
        .handler = handle_match_set,
        .priority = 90
    },
    {
        .name = "MATCH+DELETE",
        .required = CLAUSE_MATCH | CLAUSE_DELETE,
        .forbidden = CLAUSE_NONE,
        .handler = handle_match_delete,
        .priority = 90
    },
    {
        .name = "MATCH+REMOVE",
        .required = CLAUSE_MATCH | CLAUSE_REMOVE,
        .forbidden = CLAUSE_NONE,
        .handler = handle_match_remove,
        .priority = 90
    },
    {
        .name = "MATCH+MERGE",
        .required = CLAUSE_MATCH | CLAUSE_MERGE,
        .forbidden = CLAUSE_WITH,
        .handler = handle_match_merge,
        .priority = 90
    },
    {
        /* T-0317: MATCH+WITH+MERGE — pre-WITH MATCH(es) bind a var_map,
         * WITH renames it, MERGE uses the scoped var_map. Routes via
         * handle_match_merge which has been extended to detect WITH
         * and process it. */
        .name = "MATCH+WITH+MERGE",
        .required = CLAUSE_MATCH | CLAUSE_WITH | CLAUSE_MERGE,
        .forbidden = CLAUSE_NONE,
        .handler = handle_match_merge,
        .priority = 91
    },
    {
        .name = "MATCH+CREATE",
        .required = CLAUSE_MATCH | CLAUSE_CREATE,
        .forbidden = CLAUSE_RETURN,
        .handler = handle_match_create,
        .priority = 90
    },

    /*
     * Priority 80: OPTIONAL MATCH and multi-MATCH patterns
     * These require the transform pipeline for proper LEFT JOIN handling
     */
    {
        .name = "OPTIONAL_MATCH+RETURN",
        .required = CLAUSE_MATCH | CLAUSE_OPTIONAL | CLAUSE_RETURN,
        .forbidden = CLAUSE_CREATE | CLAUSE_SET | CLAUSE_DELETE | CLAUSE_MERGE,
        .handler = handle_generic_transform,
        .priority = 80
    },
    {
        .name = "MULTI_MATCH+RETURN",
        .required = CLAUSE_MATCH | CLAUSE_MULTI_MATCH | CLAUSE_RETURN,
        .forbidden = CLAUSE_CREATE | CLAUSE_SET | CLAUSE_DELETE | CLAUSE_MERGE,
        .handler = handle_generic_transform,
        .priority = 80
    },

    /*
     * Priority 90: CALL {} subquery - highest priority since CALL
     * can combine with any other clause type
     */
    {
        .name = "CALL",
        .required = CLAUSE_CALL,
        .forbidden = CLAUSE_NONE,
        .handler = handle_call_subquery,
        .priority = 90
    },

    /*
     * Priority 70: Simple MATCH+RETURN (single, non-optional)
     */
    {
        .name = "MATCH+RETURN",
        .required = CLAUSE_MATCH | CLAUSE_RETURN,
        .forbidden = CLAUSE_OPTIONAL | CLAUSE_MULTI_MATCH | CLAUSE_CREATE |
                     CLAUSE_SET | CLAUSE_DELETE | CLAUSE_MERGE | CLAUSE_UNWIND,
        .handler = handle_match_return,
        .priority = 70
    },

    /*
     * Priority 60: UNWIND with RETURN (uses transform)
     */
    {
        .name = "UNWIND+RETURN",
        .required = CLAUSE_UNWIND | CLAUSE_RETURN,
        .forbidden = CLAUSE_CREATE,
        .handler = handle_generic_transform,
        .priority = 60
    },

    /*
     * Priority 50: Standalone write clauses
     */
    {
        .name = "CREATE+RETURN",
        .required = CLAUSE_CREATE | CLAUSE_RETURN,
        .forbidden = CLAUSE_MATCH | CLAUSE_UNWIND,
        .handler = handle_create_return,
        .priority = 55
    },
    {
        .name = "CREATE",
        .required = CLAUSE_CREATE,
        .forbidden = CLAUSE_MATCH | CLAUSE_UNWIND | CLAUSE_RETURN,
        .handler = handle_create,
        .priority = 50
    },
    {
        .name = "MERGE+WITH",
        .required = CLAUSE_MERGE | CLAUSE_WITH,
        .forbidden = CLAUSE_NONE,
        .handler = handle_merge_with_pipeline,
        .priority = 55
    },
    {
        .name = "MERGE+RETURN",
        .required = CLAUSE_MERGE | CLAUSE_RETURN,
        .forbidden = CLAUSE_MATCH | CLAUSE_UNWIND | CLAUSE_WITH,
        .handler = handle_merge_return,
        .priority = 55
    },
    {
        .name = "MERGE",
        .required = CLAUSE_MERGE,
        .forbidden = CLAUSE_MATCH,
        .handler = handle_merge,
        .priority = 50
    },
    {
        .name = "SET",
        .required = CLAUSE_SET,
        .forbidden = CLAUSE_MATCH,
        .handler = handle_set,
        .priority = 50
    },
    {
        .name = "FOREACH",
        .required = CLAUSE_FOREACH,
        .forbidden = CLAUSE_NONE,
        .handler = handle_foreach,
        .priority = 50
    },

    /*
     * Priority 40: MATCH without RETURN (edge case)
     */
    {
        .name = "MATCH",
        .required = CLAUSE_MATCH,
        .forbidden = CLAUSE_RETURN | CLAUSE_CREATE | CLAUSE_SET |
                     CLAUSE_DELETE | CLAUSE_MERGE | CLAUSE_REMOVE,
        .handler = handle_match_only,
        .priority = 40
    },

    /*
     * Priority 10: Standalone RETURN (expressions, list comprehensions, graph algorithms)
     */
    {
        .name = "RETURN",
        .required = CLAUSE_RETURN,
        .forbidden = CLAUSE_MATCH | CLAUSE_UNWIND | CLAUSE_WITH,
        .handler = handle_return_only,
        .priority = 10
    },

    /*
     * Priority 0: Generic fallback
     */
    {
        .name = "GENERIC",
        .required = CLAUSE_NONE,
        .forbidden = CLAUSE_NONE,
        .handler = handle_generic_transform,
        .priority = 0
    },

    /* Sentinel - marks end of array */
    { NULL, 0, 0, NULL, 0 }
};

/*
 * Analyze a query to determine which clauses are present.
 */
clause_flags analyze_query_clauses(cypher_query *query)
{
    if (!query || !query->clauses) {
        return CLAUSE_NONE;
    }

    clause_flags flags = CLAUSE_NONE;
    int match_count = 0;

    /* Check for EXPLAIN */
    if (query->explain) {
        flags |= CLAUSE_EXPLAIN;
    }

    /* Scan all clauses */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];

        switch (clause->type) {
            case AST_NODE_MATCH: {
                cypher_match *match = (cypher_match *)clause;
                flags |= CLAUSE_MATCH;
                match_count++;
                if (match->optional) {
                    flags |= CLAUSE_OPTIONAL;
                }
                break;
            }
            case AST_NODE_RETURN:
                flags |= CLAUSE_RETURN;
                break;
            case AST_NODE_CREATE:
                flags |= CLAUSE_CREATE;
                break;
            case AST_NODE_MERGE:
                flags |= CLAUSE_MERGE;
                break;
            case AST_NODE_SET:
                flags |= CLAUSE_SET;
                break;
            case AST_NODE_DELETE:
                flags |= CLAUSE_DELETE;
                break;
            case AST_NODE_REMOVE:
                flags |= CLAUSE_REMOVE;
                break;
            case AST_NODE_WITH:
                flags |= CLAUSE_WITH;
                break;
            case AST_NODE_UNWIND:
                flags |= CLAUSE_UNWIND;
                break;
            case AST_NODE_FOREACH:
                flags |= CLAUSE_FOREACH;
                break;
            case AST_NODE_CALL_SUBQUERY:
                flags |= CLAUSE_CALL;
                break;
            case AST_NODE_LOAD_CSV:
                flags |= CLAUSE_LOAD_CSV;
                break;
            default:
                /* Unknown clause type - ignore */
                break;
        }
    }

    /* Set multi-match flag if more than one MATCH */
    if (match_count > 1) {
        flags |= CLAUSE_MULTI_MATCH;
    }

    return flags;
}

/*
 * Find the best matching pattern for the given clause flags.
 */
const query_pattern *find_matching_pattern(clause_flags present)
{
    const query_pattern *best = NULL;

    for (int i = 0; patterns[i].handler != NULL; i++) {
        const query_pattern *p = &patterns[i];

        /* Check required clauses are present */
        if ((present & p->required) != p->required) {
            continue;
        }

        /* Check forbidden clauses are absent */
        if (present & p->forbidden) {
            continue;
        }

        /* Found a match - check if it's higher priority than current best */
        if (!best || p->priority > best->priority) {
            best = p;
        }
    }

    return best;
}

/*
 * Get the pattern registry (for testing/debugging).
 */
const query_pattern *get_pattern_registry(void)
{
    return patterns;
}

/*
 * Convert clause flags to a human-readable string.
 * Uses a static buffer - not thread-safe, for debugging only.
 */
const char *clause_flags_to_string(clause_flags flags)
{
    static GQL_THREAD_LOCAL char buffer[256];
    buffer[0] = '\0';

    if (flags == CLAUSE_NONE) {
        return "(none)";
    }

    char *p = buffer;
    int remaining = sizeof(buffer);

#define APPEND_FLAG(flag, name) \
    if (flags & flag) { \
        int n = snprintf(p, remaining, "%s%s", (p > buffer ? "|" : ""), name); \
        if (n > 0 && n < remaining) { p += n; remaining -= n; } \
    }

    APPEND_FLAG(CLAUSE_MATCH, "MATCH")
    APPEND_FLAG(CLAUSE_OPTIONAL, "OPTIONAL")
    APPEND_FLAG(CLAUSE_MULTI_MATCH, "MULTI_MATCH")
    APPEND_FLAG(CLAUSE_RETURN, "RETURN")
    APPEND_FLAG(CLAUSE_CREATE, "CREATE")
    APPEND_FLAG(CLAUSE_MERGE, "MERGE")
    APPEND_FLAG(CLAUSE_SET, "SET")
    APPEND_FLAG(CLAUSE_DELETE, "DELETE")
    APPEND_FLAG(CLAUSE_REMOVE, "REMOVE")
    APPEND_FLAG(CLAUSE_WITH, "WITH")
    APPEND_FLAG(CLAUSE_UNWIND, "UNWIND")
    APPEND_FLAG(CLAUSE_FOREACH, "FOREACH")
    APPEND_FLAG(CLAUSE_UNION, "UNION")
    APPEND_FLAG(CLAUSE_CALL, "CALL")
    APPEND_FLAG(CLAUSE_LOAD_CSV, "LOAD_CSV")
    APPEND_FLAG(CLAUSE_EXPLAIN, "EXPLAIN")

#undef APPEND_FLAG

    return buffer;
}

/*
 * Main dispatch function - replaces the if-else chain.
 */
int dispatch_query_pattern(cypher_executor *executor, cypher_query *query,
                           cypher_result *result)
{
    /* Analyze query clauses */
    clause_flags flags = analyze_query_clauses(query);

    CYPHER_DEBUG("Query clauses: %s", clause_flags_to_string(flags));

    /* Find matching pattern */
    const query_pattern *pattern = find_matching_pattern(flags);

    if (!pattern) {
        set_result_error(result, "No matching execution pattern for query");
        return -1;
    }

    CYPHER_DEBUG("Matched pattern: %s (priority %d)", pattern->name, pattern->priority);

    /* Perf review F8: only the pure read handlers may park their statement
     * for the cache; anything else (writes, CALL, algorithms) runs with
     * capture disarmed so a nested read cannot be mis-associated with the
     * outer query's text. */
    if (pattern->handler != handle_match_return && pattern->handler != handle_generic_transform) {
        executor->stmt_capture = false;
    }

    /* Execute the pattern handler */
    return pattern->handler(executor, query, result, flags);
}

/*
 * Generic transform handler - uses full transform pipeline
 * This is the fallback for queries without a specialized handler
 */
int handle_generic_transform(cypher_executor *executor, cypher_query *query,
                             cypher_result *result, clause_flags flags)
{
    (void)flags;  /* Unused in generic handler */

    CYPHER_DEBUG("Using generic transform pipeline");

    cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }

    cypher_query_result *transform_result = cypher_transform_query(ctx, query);
    if (!transform_result) {
        set_result_error(result, "Failed to transform query");
        cypher_transform_free_context(ctx);
        return -1;
    }

    if (transform_result->has_error) {
        set_result_error(result, transform_result->error_message ?
                        transform_result->error_message : "Transform error");
        cypher_free_result(transform_result);
        cypher_transform_free_context(ctx);
        return -1;
    }


    /* T-0310: run pre_exec_dml (compound DML from SET/REMOVE/DELETE
     * that precedes a read) before stepping the prepared SELECT. */
    if (transform_result->pre_exec_dml) {
        char *err_msg = NULL;
        int erc = sqlite3_exec(executor->db, transform_result->pre_exec_dml,
                               NULL, NULL, &err_msg);
        if (erc != SQLITE_OK) {
            char buf[512];
            snprintf(buf, sizeof(buf), "pre-exec DML failed: %s",
                     err_msg ? err_msg : "unknown");
            set_result_error(result, buf);
            if (err_msg) sqlite3_free(err_msg);
            cypher_free_result(transform_result);
            cypher_transform_free_context(ctx);
            return -1;
        }
        if (err_msg) sqlite3_free(err_msg);
    }

    /* T-0370: a read deferred past the DML (it selects from the
     * `_gql_pipe_N` snapshot that DML just created). */
    if (!transform_result->stmt && transform_result->deferred_sql) {
        int prc = sqlite3_prepare_v2(executor->db, transform_result->deferred_sql, -1,
                                     &transform_result->stmt, NULL);
        if (prc != SQLITE_OK) {
            char buf[512];
            snprintf(buf, sizeof(buf), "SQL prepare failed: %s", sqlite3_errmsg(executor->db));
            set_result_error(result, buf);
            cypher_free_result(transform_result);
            cypher_transform_free_context(ctx);
            return -1;
        }
    }

    /* Build results from statement */
    if (transform_result->stmt) {
        /* Bind parameters if provided */
        if (executor->params_json) {
            if (bind_params_from_json(transform_result->stmt, executor->params_json) < 0) {
                set_result_error(result, "Failed to bind query parameters");
                cypher_free_result(transform_result);
                cypher_transform_free_context(ctx);
                return -1;
            }
        }

        cypher_return *ret = find_return_clause(query);

        if (ret) {
            /* Use build_query_results if we have a return clause */
            int rc = build_query_results(executor, transform_result->stmt,
                                         ret, result, ctx);
            if (rc < 0) {
                cypher_free_result(transform_result);
                cypher_transform_free_context(ctx);
                return -1;
            }
        } else {
            /* No return clause - manually collect results from SQL columns */
            result->data = NULL;
            result->row_count = 0;
            result->column_count = sqlite3_column_count(transform_result->stmt);

            /* Get column names from the SQL result */
            if (result->column_count > 0) {
                result->column_names = malloc(result->column_count * sizeof(char*));
                if (result->column_names) {
                    for (int c = 0; c < result->column_count; c++) {
                        const char *name = sqlite3_column_name(transform_result->stmt, c);
                        result->column_names[c] = name ? strdup(name) : NULL;
                    }
                }
            }

            /* Collect results */
            while (sqlite3_step(transform_result->stmt) == SQLITE_ROW) {
                /* Allocate/resize data array */
                result->data = realloc(result->data, (result->row_count + 1) * sizeof(char**));
                result->data[result->row_count] = calloc(result->column_count, sizeof(char*));

                for (int c = 0; c < result->column_count; c++) {
                    const char *val = (const char*)sqlite3_column_text(transform_result->stmt, c);
                    result->data[result->row_count][c] = val ? strdup(val) : NULL;
                }
                result->row_count++;
            }
        }
    }

    result->success = true;
    /* Perf review F8: park a pure read (RETURN, no pre-exec DML) for the
     * statement cache instead of finalizing it. */
    if (executor->stmt_capture && !executor->captured_stmt && transform_result->stmt &&
        !transform_result->pre_exec_dml && find_return_clause(query)) {
        sqlite3_reset(transform_result->stmt);
        executor->captured_stmt = transform_result->stmt;
        executor->captured_ctx = ctx;
        executor->captured_ret = find_return_clause(query);
        transform_result->stmt = NULL;
        ctx = NULL;
    }
    cypher_free_result(transform_result);
    if (ctx) cypher_transform_free_context(ctx);
    return 0;
}

/*
 * Pattern-specific handlers - wrap existing executor functions
 */

static int handle_match_set(cypher_executor *executor, cypher_query *query,
                            cypher_result *result, clause_flags flags)
{
    cypher_match *match = find_match_clause(query);
    cypher_set *set = find_set_clause(query);

    int match_count = 0;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MATCH) match_count++;
    }

    CYPHER_DEBUG("Executing MATCH+SET via pattern dispatch (match_count=%d)", match_count);

    int rc;
    if (match_count > 1) {
        /* Multi-MATCH + SET: union bindings across every MATCH clause
         * (first-row-each semantics, consistent with multi-MATCH+CREATE),
         * then apply SET once. Resolves the GQLITE-T-0198 follow-up. */
        variable_map *ms_vars = create_variable_map();
        if (!ms_vars) {
            set_result_error(result, "Failed to create variable map");
            return -1;
        }
        for (int i = 0; i < query->clauses->count; i++) {
            ast_node *clause = query->clauses->items[i];
            if (!clause || clause->type != AST_NODE_MATCH) continue;
            if (bind_match_clause_into_varmap(executor, (cypher_match*)clause, ms_vars, result) < 0) {
                free_variable_map(ms_vars);
                return -1;
            }
        }
        rc = execute_set_operations(executor, set, ms_vars, result);
        free_variable_map(ms_vars);
    } else {
        rc = execute_match_set_query(executor, match, set, result);
    }

    if (rc >= 0) {
        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret) {
                /* T-0297 / I-0042 E5 light: SET may have modified properties
                 * referenced in the original WHERE (e.g. `WHERE n.name =
                 * 'Andres' SET n.name = 'Michael' RETURN n`). Re-running the
                 * full MATCH+WHERE would now find zero rows. Use a synthetic
                 * match with the same pattern but no WHERE so the re-MATCH
                 * finds the (post-SET) nodes purely by structure.
                 *
                 * This works for the common case of single bound entity
                 * being updated. Multi-row scenarios where the WHERE
                 * was the only filter could over-return, but those are
                 * caught by row-count tests downstream. */
                cypher_match *synth = make_cypher_match(match->pattern,
                                                       NULL,
                                                       match->optional,
                                                       match->from_graph);
                rc = execute_match_return_query(executor,
                    synth ? synth : match, ret, result);
                if (synth) free(synth);
            }
        }
    }
    return rc;
}

/* GQLITE-T-0253: EntityNotFound: DeletedEntityAccess.
 *
 * openCypher: reading a property, the labels or the property map of an
 * entity that the same statement deleted is a runtime error (Return2
 * [15]-[17]). `RETURN n`, `id(n)`, `type(r)` and `count(*)` stay legal.
 * These helpers walk a RETURN expression and report whether it reads such
 * data from a variable named by the DELETE clause. */
static bool delete_names_variable(cypher_delete *del, const char *name)
{
    if (!del || !del->items || !name) return false;
    for (int i = 0; i < del->items->count; i++) {
        cypher_delete_item *it = (cypher_delete_item *)del->items->items[i];
        if (it && it->variable && strcmp(it->variable, name) == 0) return true;
    }
    return false;
}

static bool is_deleted_identifier(ast_node *expr, cypher_delete *del)
{
    return expr && expr->type == AST_NODE_IDENTIFIER &&
           delete_names_variable(del, ((cypher_identifier *)expr)->name);
}

static bool expr_reads_deleted_entity(ast_node *expr, cypher_delete *del);

static bool list_reads_deleted_entity(ast_list *list, cypher_delete *del)
{
    if (!list) return false;
    for (int i = 0; i < list->count; i++) {
        if (expr_reads_deleted_entity(list->items[i], del)) return true;
    }
    return false;
}

static bool expr_reads_deleted_entity(ast_node *expr, cypher_delete *del)
{
    if (!expr) return false;
    switch (expr->type) {
        case AST_NODE_PROPERTY: {
            cypher_property *p = (cypher_property *)expr;
            if (is_deleted_identifier(p->expr, del)) return true;
            return expr_reads_deleted_entity(p->expr, del);
        }
        case AST_NODE_MAP_PROJECTION: {
            cypher_map_projection *mp = (cypher_map_projection *)expr;
            if (is_deleted_identifier(mp->base_expr, del)) return true;
            if (mp->items) {
                for (int i = 0; i < mp->items->count; i++) {
                    cypher_map_projection_item *it = (cypher_map_projection_item *)mp->items->items[i];
                    if (it && expr_reads_deleted_entity(it->expr, del)) return true;
                }
            }
            return false;
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call *)expr;
            if (fc->function_name && fc->args && fc->args->count == 1 &&
                (strcasecmp(fc->function_name, "labels") == 0 ||
                 strcasecmp(fc->function_name, "properties") == 0 ||
                 strcasecmp(fc->function_name, "keys") == 0) &&
                is_deleted_identifier(fc->args->items[0], del)) {
                return true;
            }
            return list_reads_deleted_entity(fc->args, del);
        }
        case AST_NODE_LABEL_EXPR:
            return expr_reads_deleted_entity(((cypher_label_expr *)expr)->expr, del);
        case AST_NODE_NOT_EXPR:
            return expr_reads_deleted_entity(((cypher_not_expr *)expr)->expr, del);
        case AST_NODE_NULL_CHECK:
            return expr_reads_deleted_entity(((cypher_null_check *)expr)->expr, del);
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *b = (cypher_binary_op *)expr;
            return expr_reads_deleted_entity(b->left, del) ||
                   expr_reads_deleted_entity(b->right, del);
        }
        case AST_NODE_LIST:
            return list_reads_deleted_entity(((cypher_list *)expr)->items, del);
        case AST_NODE_MAP: {
            cypher_map *m = (cypher_map *)expr;
            if (!m->pairs) return false;
            for (int i = 0; i < m->pairs->count; i++) {
                cypher_map_pair *pair = (cypher_map_pair *)m->pairs->items[i];
                if (pair && expr_reads_deleted_entity(pair->value, del)) return true;
            }
            return false;
        }
        case AST_NODE_CASE_EXPR: {
            cypher_case_expr *c = (cypher_case_expr *)expr;
            if (expr_reads_deleted_entity(c->operand, del)) return true;
            if (expr_reads_deleted_entity(c->else_expr, del)) return true;
            if (c->when_clauses) {
                for (int i = 0; i < c->when_clauses->count; i++) {
                    cypher_when_clause *w = (cypher_when_clause *)c->when_clauses->items[i];
                    if (w && (expr_reads_deleted_entity(w->condition, del) ||
                              expr_reads_deleted_entity(w->result, del))) return true;
                }
            }
            return false;
        }
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *sub = (cypher_subscript *)expr;
            return expr_reads_deleted_entity(sub->expr, del) ||
                   expr_reads_deleted_entity(sub->index, del) ||
                   expr_reads_deleted_entity(sub->slice_start, del) ||
                   expr_reads_deleted_entity(sub->slice_end, del);
        }
        case AST_NODE_LIST_COMPREHENSION: {
            cypher_list_comprehension *lc = (cypher_list_comprehension *)expr;
            return expr_reads_deleted_entity(lc->list_expr, del) ||
                   expr_reads_deleted_entity(lc->where_expr, del) ||
                   expr_reads_deleted_entity(lc->transform_expr, del);
        }
        case AST_NODE_LIST_PREDICATE: {
            cypher_list_predicate *lp = (cypher_list_predicate *)expr;
            return expr_reads_deleted_entity(lp->list_expr, del) ||
                   expr_reads_deleted_entity(lp->predicate, del);
        }
        case AST_NODE_REDUCE_EXPR: {
            cypher_reduce_expr *r = (cypher_reduce_expr *)expr;
            return expr_reads_deleted_entity(r->initial_value, del) ||
                   expr_reads_deleted_entity(r->list_expr, del) ||
                   expr_reads_deleted_entity(r->expression, del);
        }
        default:
            return false;
    }
}

static bool return_reads_deleted_entity(cypher_return *ret, cypher_delete *del)
{
    if (!ret || !ret->items || !del) return false;
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *it = (cypher_return_item *)ret->items->items[i];
        if (it && expr_reads_deleted_entity(it->expr, del)) return true;
    }
    return false;
}

static int handle_match_delete(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    cypher_match *match = find_match_clause(query);
    cypher_delete *del = find_delete_clause(query);
    cypher_create *cre = find_create_clause(query);

    CYPHER_DEBUG("Executing MATCH+DELETE via pattern dispatch");

    /* T-0339 (part 3): MATCH+DELETE+CREATE — handle_match_delete previously
     * dropped the CREATE clause entirely. Run CREATE first (per matched row,
     * via execute_multi_match_create_query which iterates every row) using
     * the live pre-delete bindings; then proceed with the delete. The new
     * relationships/nodes from CREATE don't depend on the entities being
     * deleted, so this ordering preserves Cypher semantics for the in-scope
     * scenarios (Match5 [26] setup: rewires (a)-[r]->(b) by creating
     * (b)-[:LIKES]->(a) and deleting r). */
    if (cre) {
        if (execute_multi_match_create_query(executor, query, cre, result, NULL, NULL, NULL) < 0) {
            return -1;
        }
    }

    /* T-0321: MATCH+DELETE+RETURN — capture the RETURN projection
     * BEFORE delete when the projection references entity data
     * (e.g. `type(r)` or `r.prop`). After DELETE, the entity is
     * gone and the lookup yields NULL.
     *
     * I-0047 P1: `count(*)` is pre-captured too. It must reflect the
     * number of MATCHED rows, which `execute_match_return_query`
     * computes directly. The legacy `synthesize_delete_return` path
     * derived `count` from the accumulated *delete* count
     * (nodes_deleted + rels_deleted, with no endpoint dedup), which
     * only coincidentally matched the expected aggregate for shapes
     * our MATCH undercounted (e.g. undirected varlen, where 3 directed
     * rows × 2 deleted endpoints == the 6 rows real Cypher matches).
     * Now that undirected varlen traverses both orientations (the
     * MATCH row count is correct), that coincidence would over-count
     * (6 rows × 2 == 12) — so count() goes through the live MATCH.
     *
     * Literal RETURNs still use synthesize_delete_return (its
     * total_deleted row multiplier is left as-is); only count() and
     * entity-data projections pre-capture. */
    cypher_return *ret_clause = (flags & CLAUSE_RETURN) ? find_return_clause(query) : NULL;
    bool needs_pre_capture = false;
    if (ret_clause && ret_clause->items) {
        for (int i = 0; i < ret_clause->items->count; i++) {
            cypher_return_item *it = (cypher_return_item *)ret_clause->items->items[i];
            if (!it || !it->expr) continue;
            ast_node_type t = it->expr->type;
            /* synthesize_delete_return handles bare LITERAL rows.
             * Everything else (count() aggregate, property access,
             * type()/labels()/id() function calls, variable bare
             * references, etc.) needs the live pre-delete MATCH. */
            if (t == AST_NODE_LITERAL) continue;
            needs_pre_capture = true;
            break;
        }
    }

    /* GQLITE-T-0253: RETURN reads property/label data of a variable this
     * statement deletes. If the MATCH binds such an entity, the statement
     * fails as a whole (nothing is deleted, nothing is projected). A null
     * binding (OPTIONAL MATCH miss) is not a deleted entity and falls
     * through to the normal null-propagating projection. */
    if (ret_clause && return_reads_deleted_entity(ret_clause, del)) {
        int bound = delete_targets_bound(executor, match, del);
        if (bound < 0) {
            set_result_error(result, "Failed to execute MATCH for DELETE");
            return -1;
        }
        if (bound > 0) {
            set_result_error_ex(result,
                "EntityNotFound: DeletedEntityAccess: cannot access a property or "
                "label of an entity deleted in the same statement",
                GQL_ERR_EXECUTION, 0, 0);
            return -1;
        }
    }

    bool ran_pre_return = false;
    if (needs_pre_capture) {
        if (execute_match_return_query(executor, match, ret_clause, result) >= 0) {
            ran_pre_return = true;
        }
    }

    int rc = execute_match_delete_query(executor, match, del, result);
    if (rc >= 0) {
        result->success = true;
        if (ret_clause && !ran_pre_return) {
            /* Synthesize COUNT/literal results from the delete
             * counts when possible; else last-resort re-MATCH. */
            if (!synthesize_delete_return(ret_clause, result)) {
                cypher_match *synth = make_cypher_match(match->pattern,
                                                       NULL,
                                                       match->optional,
                                                       match->from_graph);
                rc = execute_match_return_query(executor,
                    synth ? synth : match, ret_clause, result);
                if (synth) free(synth);
            }
        }
    }
    return rc;
}

static int handle_match_remove(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    cypher_match *match = find_match_clause(query);
    cypher_remove *remove = find_remove_clause(query);

    CYPHER_DEBUG("Executing MATCH+REMOVE via pattern dispatch");

    /* T-0315: detect label-removal items. When REMOVE strips labels,
     * the post-REMOVE pattern `(n:N)` finds 0 rows (the label is
     * gone), so a re-MATCH for RETURN returns nothing. The fix is
     * to RUN the RETURN against the PRE-REMOVE state first, then
     * execute the REMOVE. Property-only REMOVE preserves the
     * existing synth-match-without-WHERE path (which already
     * handles the stale WHERE issue). */
    bool has_label_remove = false;
    if (remove && remove->items) {
        for (int i = 0; i < remove->items->count; i++) {
            cypher_remove_item *it = (cypher_remove_item*)remove->items->items[i];
            if (it && it->target && it->target->type == AST_NODE_LABEL_EXPR) {
                has_label_remove = true;
                break;
            }
        }
    }

    int rc = execute_match_remove_query(executor, match, remove, result);
    if (rc >= 0) {
        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret) {
                /* T-0315: when REMOVE strips labels, the synth pattern
                 * `(n:Label)` would find 0 rows because Label is gone.
                 * Mutate-then-restore: walk match->pattern, remove
                 * the to-be-stripped labels from node patterns, run
                 * the synth re-MATCH, then restore the labels so the
                 * AST is unchanged after our handler returns.
                 *
                 * The variable binding is preserved (we re-match by
                 * structure without the removed label); RETURN reads
                 * the CURRENT post-REMOVE state for labels(n) and
                 * untouched properties alike.
                 *
                 * Property REMOVE doesn't need stripping (the WHERE
                 * is the only stale-state concern, handled by
                 * passing NULL where in synth match). */
                typedef struct { ast_list *labels; int idx; ast_node *removed; } stripped_t;
                stripped_t strip_buf[16];
                int strip_count = 0;
                if (has_label_remove && remove && remove->items) {
                    for (int i = 0; i < remove->items->count && strip_count < 16; i++) {
                        cypher_remove_item *it = (cypher_remove_item*)remove->items->items[i];
                        if (!it || !it->target ||
                            it->target->type != AST_NODE_LABEL_EXPR) continue;
                        cypher_label_expr *le = (cypher_label_expr*)it->target;
                        if (!le->expr || le->expr->type != AST_NODE_IDENTIFIER) continue;
                        const char *var = ((cypher_identifier*)le->expr)->name;
                        const char *lbl = le->label_name;
                        if (!var || !lbl) continue;
                        /* Walk pattern paths. */
                        if (!match->pattern) continue;
                        for (int pi = 0; pi < match->pattern->count; pi++) {
                            cypher_path *path = (cypher_path*)match->pattern->items[pi];
                            if (!path || !path->elements) continue;
                            for (int ei = 0; ei < path->elements->count; ei++) {
                                ast_node *el = path->elements->items[ei];
                                if (!el || el->type != AST_NODE_NODE_PATTERN) continue;
                                cypher_node_pattern *np = (cypher_node_pattern*)el;
                                if (!np->variable || strcmp(np->variable, var) != 0) continue;
                                if (!np->labels) continue;
                                /* Find label in np->labels and strip it. */
                                for (int li = 0; li < np->labels->count; li++) {
                                    ast_node *lab_node = np->labels->items[li];
                                    if (!lab_node) continue;
                                    const char *lab_name = NULL;
                                    if (lab_node->type == AST_NODE_LITERAL) {
                                        cypher_literal *lit = (cypher_literal*)lab_node;
                                        if (lit->literal_type == LITERAL_STRING)
                                            lab_name = lit->value.string;
                                    } else if (lab_node->type == AST_NODE_IDENTIFIER) {
                                        lab_name = ((cypher_identifier*)lab_node)->name;
                                    }
                                    if (lab_name && strcmp(lab_name, lbl) == 0 && strip_count < 16) {
                                        strip_buf[strip_count].labels = np->labels;
                                        strip_buf[strip_count].idx = li;
                                        strip_buf[strip_count].removed = lab_node;
                                        strip_count++;
                                        /* Remove from list by shifting. */
                                        for (int sh = li; sh < np->labels->count - 1; sh++) {
                                            np->labels->items[sh] = np->labels->items[sh + 1];
                                        }
                                        np->labels->count--;
                                        li--; /* Re-check this index */
                                    }
                                }
                            }
                        }
                    }
                }

                cypher_match *synth = make_cypher_match(match->pattern,
                                                       NULL,
                                                       match->optional,
                                                       match->from_graph);
                rc = execute_match_return_query(executor,
                    synth ? synth : match, ret, result);
                if (synth) free(synth);

                /* Restore stripped labels in reverse order. */
                for (int i = strip_count - 1; i >= 0; i--) {
                    ast_list *labs = strip_buf[i].labels;
                    int idx = strip_buf[i].idx;
                    /* Insert back at original index. */
                    for (int sh = labs->count; sh > idx; sh--) {
                        labs->items[sh] = labs->items[sh - 1];
                    }
                    labs->items[idx] = strip_buf[i].removed;
                    labs->count++;
                }
            }
        }
    }
    return rc;
}

/* GQLITE-T-0338: generate_node_match (transform_match.c) consumes a node's
 * first inline property by nulling the map pair's key once it has been
 * folded into the FROM/JOIN anchor. handle_match_merge transforms the same
 * pattern AST twice — once to execute the MERGE, once for the RETURN
 * re-match over the combined MATCH+MERGE pattern — so the second pass used
 * to lose the `{id: ..}` filters and an undirected MERGE re-match doubled
 * its rows (Merge5 [12]/[13]). Snapshot every inline property key before
 * the first pass and put the nulled ones back before the second. The keys
 * are never freed by the consumer, so restoring the pointer is safe. */
typedef struct {
    cypher_map_pair **pairs;
    char **keys;
    int count;
    int cap;
} prop_key_snapshot;

static void snapshot_add(prop_key_snapshot *snap, cypher_map_pair *pair)
{
    if (snap->count == snap->cap) {
        int cap = snap->cap ? snap->cap * 2 : 8;
        cypher_map_pair **pp = realloc(snap->pairs, (size_t)cap * sizeof(*pp));
        char **kk = realloc(snap->keys, (size_t)cap * sizeof(*kk));
        if (!pp || !kk) { free(pp); free(kk); return; }
        snap->pairs = pp; snap->keys = kk; snap->cap = cap;
    }
    snap->pairs[snap->count] = pair;
    snap->keys[snap->count] = pair->key;
    snap->count++;
}

static void snapshot_props(prop_key_snapshot *snap, ast_node *props)
{
    if (!props || props->type != AST_NODE_MAP) return;
    cypher_map *m = (cypher_map*)props;
    if (!m->pairs) return;
    for (int i = 0; i < m->pairs->count; i++) {
        cypher_map_pair *pair = (cypher_map_pair*)m->pairs->items[i];
        if (pair && pair->key) snapshot_add(snap, pair);
    }
}

static void snapshot_pattern_prop_keys(ast_list *pattern, prop_key_snapshot *snap)
{
    if (!pattern) return;
    for (int i = 0; i < pattern->count; i++) {
        ast_node *item = pattern->items[i];
        if (!item) continue;
        if (item->type == AST_NODE_NODE_PATTERN) {
            snapshot_props(snap, ((cypher_node_pattern*)item)->properties);
        } else if (item->type == AST_NODE_PATH) {
            cypher_path *path = (cypher_path*)item;
            if (!path->elements) continue;
            for (int j = 0; j < path->elements->count; j++) {
                ast_node *el = path->elements->items[j];
                if (!el) continue;
                if (el->type == AST_NODE_NODE_PATTERN)
                    snapshot_props(snap, ((cypher_node_pattern*)el)->properties);
                else if (el->type == AST_NODE_REL_PATTERN)
                    snapshot_props(snap, ((cypher_rel_pattern*)el)->properties);
            }
        }
    }
}

static void restore_pattern_prop_keys(prop_key_snapshot *snap)
{
    for (int i = 0; i < snap->count; i++) {
        if (snap->pairs[i]->key == NULL) snap->pairs[i]->key = snap->keys[i];
    }
    free(snap->pairs);
    free(snap->keys);
    snap->pairs = NULL; snap->keys = NULL; snap->count = snap->cap = 0;
}

static int handle_match_merge(cypher_executor *executor, cypher_query *query,
                              cypher_result *result, clause_flags flags)
{
    cypher_match *match = find_match_clause(query);
    cypher_merge *merge = find_merge_clause(query);
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing MATCH+MERGE via pattern dispatch");

    /* GQLITE-T-0338: see prop_key_snapshot above. */
    prop_key_snapshot snap = {0};
    snapshot_pattern_prop_keys(match ? match->pattern : NULL, &snap);
    snapshot_pattern_prop_keys(merge ? merge->pattern : NULL, &snap);

    /* T-0317: MATCH+WITH+MERGE — bind each pre-WITH MATCH into a
     * var_map, process WITH item renames (`a AS x` etc.), then run
     * MERGE against the renamed map. This covers Merge5 [16]-[19]
     * (aliasing of existing nodes) which previously fell through to
     * handle_generic_transform → "Unsupported clause type". */
    bool has_with = (flags & CLAUSE_WITH) != 0;
    if (has_with) {
        variable_map *vm = create_variable_map();
        if (!vm) { restore_pattern_prop_keys(&snap); set_result_error(result, "OOM"); return -1; }

        /* Bind every pre-WITH MATCH clause into vm. */
        for (int i = 0; i < query->clauses->count; i++) {
            ast_node *c = query->clauses->items[i];
            if (!c) continue;
            if (c->type == AST_NODE_WITH) break;
            if (c->type != AST_NODE_MATCH) continue;
            if (bind_match_clause_into_varmap(executor, (cypher_match*)c, vm, result) < 0) {
                free_variable_map(vm);
                restore_pattern_prop_keys(&snap);
                return -1;
            }
        }

        /* Apply WITH renames: for each `X AS Y` (or bare X), copy vm
         * entries to a fresh scoped map under the target alias. */
        variable_map *scoped = create_variable_map();
        if (!scoped) { free_variable_map(vm); restore_pattern_prop_keys(&snap); set_result_error(result, "OOM"); return -1; }
        for (int i = 0; i < query->clauses->count; i++) {
            ast_node *c = query->clauses->items[i];
            if (!c || c->type != AST_NODE_WITH) continue;
            cypher_with *w = (cypher_with*)c;
            if (!w->items) break;
            for (int wi = 0; wi < w->items->count; wi++) {
                cypher_return_item *it = (cypher_return_item*)w->items->items[wi];
                if (!it || !it->expr) continue;
                if (it->expr->type != AST_NODE_IDENTIFIER) continue;
                const char *src = ((cypher_identifier*)it->expr)->name;
                const char *dst = it->alias ? it->alias : src;
                int nid = get_variable_node_id(vm, src);
                int eid = is_variable_edge(vm, src) ? get_variable_edge_id(vm, src) : -1;
                if (nid >= 0) set_variable_node_id(scoped, dst, nid);
                else if (eid >= 0) set_variable_edge_id(scoped, dst, eid);
            }
            break;
        }
        free_variable_map(vm);

        /* Also bind POST-WITH MATCH clauses into scoped. After WITH,
         * variables not passed through are out of scope, but NEW
         * MATCH clauses after WITH introduce fresh bindings. Without
         * this, `MATCH (a) WITH a MATCH (b) MERGE (a)-[:R]->(b)`
         * leaves `b` unbound and MERGE collapses the edge into a
         * self-loop on `a`. */
        bool past_with = false;
        for (int i = 0; i < query->clauses->count; i++) {
            ast_node *c = query->clauses->items[i];
            if (!c) continue;
            if (c->type == AST_NODE_WITH) { past_with = true; continue; }
            if (!past_with) continue;
            if (c->type != AST_NODE_MATCH) continue;
            if (bind_match_clause_into_varmap(executor, (cypher_match*)c,
                                              scoped, result) < 0) {
                free_variable_map(scoped);
                restore_pattern_prop_keys(&snap);
                return -1;
            }
        }

        /* Run MERGE against the scoped (WITH-renamed + post-WITH MATCH) var_map. */
        int rc = execute_merge_clause(executor, merge, result, scoped, NULL);
        free_variable_map(scoped);
        restore_pattern_prop_keys(&snap);   /* T-0338: re-match sees the inline props */
        if (rc < 0) return rc;

        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret) {
                /* RETURN sees the post-MERGE state via the combined
                 * pattern. Use the synth-match (combined MATCH+MERGE
                 * pattern) trick already used by the non-WITH branch.
                 * NOTE: the WITH renames may make some return items
                 * reference the renamed aliases (a, b) rather than the
                 * original (n, m). For Merge5 [16] the RETURN reads
                 * a.id, b.id — which we don't currently rewrite.
                 * Acceptance check below will surface gaps. */
                ast_list *combined = ast_list_create();
                if (match->pattern) {
                    for (int i = 0; i < match->pattern->count; i++)
                        ast_list_append(combined, match->pattern->items[i]);
                }
                if (merge->pattern) {
                    for (int i = 0; i < merge->pattern->count; i++)
                        ast_list_append(combined, merge->pattern->items[i]);
                }
                cypher_match *synth = make_cypher_match(combined,
                                                       match->where, false, NULL);
                rc = execute_match_return_query(executor, synth, ret, result);
                if (synth) free(synth);
            }
        }
        return rc;
    }

    /* Capture the MATCH+MERGE var_map when a trailing SET needs it. */
    variable_map *mm_vars = NULL;
    int rc = execute_match_merge_query_with_varmap(executor, match, merge, result, set ? &mm_vars : NULL);
    restore_pattern_prop_keys(&snap);   /* T-0338: re-match sees the inline props */
    if (rc < 0) {
        if (mm_vars) free_variable_map(mm_vars);
        return rc;
    }

    if (set && mm_vars) {
        rc = execute_set_operations(executor, set, mm_vars, result);
        if (rc < 0) {
            free_variable_map(mm_vars);
            return rc;
        }
    }
    if (mm_vars) free_variable_map(mm_vars);

    result->success = true;
    if (flags & CLAUSE_RETURN) {
        cypher_return *ret = find_return_clause(query);
        if (ret) {
            /* For RETURN after MERGE, the merged variables (r, etc.) must
             * be visible. Construct a synthetic MATCH whose pattern is the
             * union of the original MATCH and MERGE patterns, then re-query.
             * The MERGE has already created/matched whatever was needed; the
             * combined MATCH simply observes the resulting state. */
            ast_list *combined_pattern = ast_list_create();
            if (match->pattern) {
                for (int i = 0; i < match->pattern->count; i++) {
                    ast_list_append(combined_pattern, match->pattern->items[i]);
                }
            }
            if (merge->pattern) {
                for (int i = 0; i < merge->pattern->count; i++) {
                    ast_list_append(combined_pattern, merge->pattern->items[i]);
                }
            }
            cypher_match *synth_match = make_cypher_match(combined_pattern,
                                                          match->where, false, NULL);
            rc = execute_match_return_query(executor, synth_match, ret, result);
            free(synth_match);
            free(combined_pattern->items);
            free(combined_pattern);
        }
    }
    return rc;
}

static int handle_match_create(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_create *create = find_create_clause(query);
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing MATCH+CREATE via pattern dispatch");

    /* Always take the multi-MATCH path — it handles 1+ MATCH clauses and
     * optionally returns the post-CREATE var_map so a trailing SET can
     * thread its scope. Single-MATCH queries behave identically to the
     * legacy execute_match_create_query path. */
    variable_map *mc_vars = NULL;
    int rc = execute_multi_match_create_query(executor, query, create, result,
                                                          set ? &mc_vars : NULL,
                                                          NULL, NULL);
    if (rc < 0) {
        if (mc_vars) free_variable_map(mc_vars);
        return rc;
    }
    if (set && mc_vars) {
        rc = execute_set_operations(executor, set, mc_vars, result);
        if (rc < 0) {
            free_variable_map(mc_vars);
            return rc;
        }
    }
    if (mc_vars) free_variable_map(mc_vars);
    if (rc >= 0) result->success = true;
    return rc;
}

/* GitHub #95 helpers: detect RETURN items that reference a variable bound
 * only by a CREATE pattern (not by any MATCH), e.g.
 *   MATCH (x), (y) CREATE (x)-[r:T]->(y) RETURN r
 * The legacy path re-executes MATCH+RETURN, which has no binding for `r`
 * and errored AFTER the CREATE had already committed. */
static bool pattern_list_binds_var(ast_list *pattern, const char *name)
{
    if (!pattern || !name) return false;
    for (int i = 0; i < pattern->count; i++) {
        ast_node *p = pattern->items[i];
        if (!p || p->type != AST_NODE_PATH) continue;
        cypher_path *path = (cypher_path*)p;
        if (path->var_name && strcmp(path->var_name, name) == 0) return true;
        if (!path->elements) continue;
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *el = path->elements->items[j];
            if (!el) continue;
            if (el->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern*)el;
                if (np->variable && strcmp(np->variable, name) == 0) return true;
            } else if (el->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern*)el;
                if (rp->variable && strcmp(rp->variable, name) == 0) return true;
            }
        }
    }
    return false;
}

/* Base variable of a var-map-projectable RETURN expression: a bare
 * identifier (`r`) or a simple property access (`r.prop`). NULL for
 * anything else. */
static const char *projectable_base_var(ast_node *expr)
{
    if (!expr) return NULL;
    if (expr->type == AST_NODE_IDENTIFIER) {
        return ((cypher_identifier*)expr)->name;
    }
    if (expr->type == AST_NODE_PROPERTY) {
        cypher_property *prop = (cypher_property*)expr;
        if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
            return ((cypher_identifier*)prop->expr)->name;
        }
    }
    return NULL;
}

static int handle_match_create_return(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_match *match = find_match_clause(query);
    cypher_create *create = find_create_clause(query);
    cypher_return *ret = find_return_clause(query);

    CYPHER_DEBUG("Executing MATCH+CREATE+RETURN via pattern dispatch");

    /* GitHub #95: if the RETURN references CREATE-only variables, project it
     * from the per-row variable maps instead of re-running MATCH+RETURN
     * (which cannot see them and would error after the write committed).
     * Only taken when every RETURN item is a var-map-projectable shape
     * (bare var, var.prop, or an aggregate over those), so all other
     * queries keep the legacy path unchanged. */
    bool any_create_only = false;
    bool all_projectable = true;
    if (ret && ret->items && !ret->return_all && !ret->order_by && query->clauses) {
        for (int i = 0; i < ret->items->count; i++) {
            cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
            ast_node *expr = item ? item->expr : NULL;
            if (aggregating_call_name(expr)) {
                cypher_function_call *fc = (cypher_function_call*)expr;
                expr = (fc->args && fc->args->count > 0) ? fc->args->items[0] : NULL;
                if (!expr) continue; /* count(*) — no variable reference */
            }
            const char *base = projectable_base_var(expr);
            if (!base) { all_projectable = false; break; }
            bool in_match = false;
            for (int ci = 0; ci < query->clauses->count; ci++) {
                ast_node *c = query->clauses->items[ci];
                if (c && c->type == AST_NODE_MATCH &&
                    pattern_list_binds_var(((cypher_match*)c)->pattern, base)) {
                    in_match = true;
                    break;
                }
            }
            if (!in_match) {
                bool in_create = false;
                for (int ci = 0; ci < query->clauses->count; ci++) {
                    ast_node *c = query->clauses->items[ci];
                    if (c && c->type == AST_NODE_CREATE &&
                        pattern_list_binds_var(((cypher_create*)c)->pattern, base)) {
                        in_create = true;
                        break;
                    }
                }
                if (in_create) {
                    any_create_only = true;
                } else {
                    all_projectable = false;
                    break;
                }
            }
        }
    } else {
        all_projectable = false;
    }

    if (any_create_only && all_projectable) {
        variable_map **maps = NULL;
        int n_maps = 0;
        if (execute_multi_match_create_query(executor, query, create, result,
                                             NULL, &maps, &n_maps) < 0) {
            return -1;
        }

        int col_count = ret->items->count;
        set_return_column_names(ret, result);

        /* SKIP/LIMIT */
        int64_t limit_val = -1, skip_val = 0;
        if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
            cypher_literal *l = (cypher_literal*)ret->limit;
            if (l->literal_type == LITERAL_INTEGER) limit_val = l->value.integer;
        }
        if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
            cypher_literal *l = (cypher_literal*)ret->skip;
            if (l->literal_type == LITERAL_INTEGER) skip_val = l->value.integer;
        }
        int start = 0;
        if (skip_val > 0) start = (skip_val >= n_maps) ? n_maps : (int)skip_val;
        int end = n_maps;
        if (limit_val == 0) end = start;
        else if (limit_val > 0 && start + (int)limit_val < end) end = start + (int)limit_val;
        int produced = end - start;
        if (produced < 0) produced = 0;

        if (return_has_aggregation(ret)) {
            /* Single aggregated row across the (post-skip/limit) maps. */
            result->row_count = 1;
            result->data = malloc(sizeof(char**));
            result->data_types = malloc(sizeof(int*));
            result->data[0] = malloc(col_count * sizeof(char*));
            result->data_types[0] = calloc(col_count, sizeof(int));
            for (int i = 0; i < col_count; i++) {
                cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
                if (aggregating_call_name(it->expr)) {
                    project_aggregate_cell(executor, it, maps + start, produced, result, i);
                } else if (produced > 0) {
                    char **save_data = result->data[0];
                    int *save_types = result->data_types[0];
                    char ***save_data_all = result->data;
                    int **save_types_all = result->data_types;
                    char **tmp = malloc(col_count * sizeof(char*));
                    int *tmp_t = calloc(col_count, sizeof(int));
                    result->data = &tmp;
                    result->data_types = &tmp_t;
                    project_return_row_from_var_map(executor, ret, maps[start], result, 0);
                    result->data = save_data_all;
                    result->data_types = save_types_all;
                    save_data[i] = tmp[i];
                    save_types[i] = tmp_t[i];
                    for (int k = 0; k < col_count; k++) if (k != i && tmp[k]) free(tmp[k]);
                    free(tmp);
                    free(tmp_t);
                } else {
                    result->data[0][i] = NULL;
                }
            }
        } else {
            result->row_count = produced;
            if (produced == 0) {
                result->data = NULL;
                result->data_types = NULL;
            } else {
                result->data = malloc(produced * sizeof(char**));
                result->data_types = malloc(produced * sizeof(int*));
                for (int r = 0; r < produced; r++) {
                    result->data[r] = malloc(col_count * sizeof(char*));
                    result->data_types[r] = calloc(col_count, sizeof(int));
                    project_return_row_from_var_map(executor, ret, maps[start + r], result, r);
                }
            }
        }

        for (int j = 0; j < n_maps; j++) free_variable_map(maps[j]);
        free(maps);
        result->success = true;
        return 0;
    }

    int rc = execute_match_create_return_query(executor, match, create, ret, result);
    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

static int handle_match_return(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_match *match = find_match_clause(query);
    cypher_return *ret = find_return_clause(query);

    CYPHER_DEBUG("Executing MATCH+RETURN via pattern dispatch");
    int rc = execute_match_return_query(executor, match, ret, result);
    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

static int handle_create(cypher_executor *executor, cypher_query *query,
                         cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing CREATE via pattern dispatch");

    /* No trailing SET: run every CREATE clause in document order, sharing
     * one variable_map so a later CREATE can reference variables bound by
     * an earlier one (e.g. `CREATE (a),(b) CREATE (a)-[:X]->(b)`). */
    if (!set) {
        int rc = 0;
        bool any = false;
        variable_map *shared_vars = NULL;
        for (int i = 0; query->clauses && i < query->clauses->count; i++) {
            ast_node *clause = query->clauses->items[i];
            if (clause->type != AST_NODE_CREATE) continue;
            rc = execute_create_clause_with_varmap(executor,
                                                    (cypher_create *)clause,
                                                    result, &shared_vars);
            if (rc < 0) {
                if (shared_vars) free_variable_map(shared_vars);
                return rc;
            }
            any = true;
        }
        if (shared_vars) free_variable_map(shared_vars);
        if (any) result->success = true;
        return rc;
    }

    /* CREATE + SET: execute every CREATE clause (accumulating bindings) via
     * the varmap variant, then thread the merged map into
     * execute_set_operations. */
    variable_map *create_vars = NULL;
    int rc = 0;
    bool any = false;
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type != AST_NODE_CREATE) continue;
        rc = execute_create_clause_with_varmap(executor,
                                                (cypher_create *)clause,
                                                result, &create_vars);
        if (rc < 0) {
            if (create_vars) free_variable_map(create_vars);
            return rc;
        }
        any = true;
    }
    if (!any) {
        if (create_vars) free_variable_map(create_vars);
        return 0;
    }
    rc = execute_set_operations(executor, set, create_vars, result);
    free_variable_map(create_vars);
    if (rc >= 0) result->success = true;
    return rc;
}

static int handle_merge(cypher_executor *executor, cypher_query *query,
                        cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_merge *merge = find_merge_clause(query);
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing MERGE via pattern dispatch");

    if (!set) {
        int rc = execute_merge_clause(executor, merge, result, NULL, NULL);
        if (rc >= 0) result->success = true;
        return rc;
    }

    /* MERGE + trailing SET: thread MERGE's var_map into execute_set_operations.
     * ON CREATE / ON MATCH SET already run inside execute_merge_clause. */
    variable_map *merge_vars = NULL;
    int rc = execute_merge_clause(executor, merge, result, NULL, &merge_vars);
    if (rc < 0) {
        if (merge_vars) free_variable_map(merge_vars);
        return rc;
    }
    rc = execute_set_operations(executor, set, merge_vars, result);
    free_variable_map(merge_vars);
    if (rc >= 0) result->success = true;
    return rc;
}

static int handle_set(cypher_executor *executor, cypher_query *query,
                      cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing SET via pattern dispatch");
    int rc = execute_set_clause(executor, set, result);
    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

static int handle_foreach(cypher_executor *executor, cypher_query *query,
                          cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_foreach *foreach = find_foreach_clause(query);

    CYPHER_DEBUG("Executing FOREACH via pattern dispatch");
    int rc = execute_foreach_clause(executor, foreach, result);
    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

static int handle_match_only(cypher_executor *executor, cypher_query *query,
                             cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_match *match = find_match_clause(query);

    CYPHER_DEBUG("Executing MATCH (no RETURN) via pattern dispatch");
    int rc = execute_match_clause(executor, match, result);
    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

/*
 * UNWIND+CREATE handler - iterates over list and creates nodes
 * Extracted from cypher_executor.c inline code
 */
/* GQLITE-T-0371: UNWIND ... UNWIND ... CREATE needs the cartesian product of
 * the UNWINDs; the dedicated UNWIND+CREATE handlers iterate one list only, so
 * two or more UNWINDs before the CREATE go to the row-wise pipeline. */
static bool unwind_create_needs_pipeline(cypher_query *query)
{
    int n_unwind = 0;
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        if (c->type == AST_NODE_CREATE) break;
        if (c->type == AST_NODE_UNWIND) n_unwind++;
    }
    return n_unwind >= 2;
}

static int handle_unwind_create(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    if (unwind_create_needs_pipeline(query)) return handle_rowwise_write(executor, query, result, flags);
    (void)flags;
    cypher_unwind *unwind = find_unwind_clause(query);
    cypher_create *create = find_create_clause(query);

    CYPHER_DEBUG("Executing UNWIND+CREATE via pattern dispatch");

    /* Find optional SET clause */
    cypher_set *set = find_set_clause(query);

    /* Handle parameterized UNWIND: iterate via json_each */
    if (unwind->expr->type == AST_NODE_PARAMETER) {
        cypher_parameter *param = (cypher_parameter*)unwind->expr;
        if (!executor->params_json) {
            set_result_error(result, "UNWIND $param requires parameters");
            return -1;
        }

        /* Query: SELECT value FROM json_each(json_extract(:params, '$.paramname')) */
        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT value FROM json_each(json_extract(?, '$.%s'))", param->name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            set_result_error(result, "Failed to prepare UNWIND parameter query");
            return -1;
        }
        sqlite3_bind_text(stmt, 1, executor->params_json, -1, SQLITE_STATIC);

        foreach_context *ctx = create_foreach_context();
        if (!ctx) {
            sqlite3_finalize(stmt);
            set_result_error(result, "Failed to create foreach context");
            return -1;
        }
        foreach_context *prev_ctx = g_foreach_ctx;
        g_foreach_ctx = ctx;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int col_type = sqlite3_column_type(stmt, 0);
            if (col_type == SQLITE_TEXT) {
                set_foreach_binding_string(ctx, unwind->alias,
                    (const char*)sqlite3_column_text(stmt, 0));
            } else if (col_type == SQLITE_INTEGER) {
                set_foreach_binding_int(ctx, unwind->alias,
                    sqlite3_column_int64(stmt, 0));
            }

            /* Execute CREATE, capturing the variable map */
            variable_map *create_vars = NULL;
            if (execute_create_clause_with_varmap(executor, create, result, &create_vars) < 0) {
                g_foreach_ctx = prev_ctx;
                free_foreach_context(ctx);
                sqlite3_finalize(stmt);
                return -1;
            }

            /* Execute SET if present, using variable map from CREATE */
            if (set && create_vars) {
                if (execute_set_operations(executor, set, create_vars, result) < 0) {
                    CYPHER_DEBUG("UNWIND+CREATE+SET: SET failed");
                }
                free_variable_map(create_vars);
            } else if (create_vars) {
                free_variable_map(create_vars);
            }
        }

        g_foreach_ctx = prev_ctx;
        free_foreach_context(ctx);
        sqlite3_finalize(stmt);

        result->success = true;
        return 0;
    }

    /* Handle range(start, end[, step]) — expand via SQLite recursive
     * CTE and iterate like the parameter branch. Common UNWIND
     * generator (Return4 [8], Set/Remove/Delete N-row scenarios). */
    if (unwind->expr->type == AST_NODE_FUNCTION_CALL) {
        cypher_function_call *fc = (cypher_function_call *)unwind->expr;
        if (fc->function_name && strcasecmp(fc->function_name, "range") == 0 &&
            fc->args && (fc->args->count == 2 || fc->args->count == 3)) {
            ast_node *a0 = fc->args->items[0];
            ast_node *a1 = fc->args->items[1];
            ast_node *a2 = fc->args->count == 3 ? fc->args->items[2] : NULL;
            if (a0 && a0->type == AST_NODE_LITERAL && a1 && a1->type == AST_NODE_LITERAL &&
                ((cypher_literal *)a0)->literal_type == LITERAL_INTEGER &&
                ((cypher_literal *)a1)->literal_type == LITERAL_INTEGER &&
                (!a2 || (a2->type == AST_NODE_LITERAL &&
                         ((cypher_literal *)a2)->literal_type == LITERAL_INTEGER))) {
                int64_t start = ((cypher_literal *)a0)->value.integer;
                int64_t end   = ((cypher_literal *)a1)->value.integer;
                int64_t step  = a2 ? ((cypher_literal *)a2)->value.integer : 1;
                if (step == 0) step = 1;
                foreach_context *ctx = create_foreach_context();
                if (!ctx) {
                    set_result_error(result, "Failed to create foreach context");
                    return -1;
                }
                foreach_context *prev_ctx = g_foreach_ctx;
                g_foreach_ctx = ctx;
                for (int64_t v = start;
                     (step > 0) ? v <= end : v >= end;
                     v += step) {
                    set_foreach_binding_int(ctx, unwind->alias, v);
                    variable_map *create_vars = NULL;
                    if (execute_create_clause_with_varmap(executor, create, result, &create_vars) < 0) {
                        g_foreach_ctx = prev_ctx;
                        free_foreach_context(ctx);
                        return -1;
                    }
                    if (set && create_vars) {
                        if (execute_set_operations(executor, set, create_vars, result) < 0) {
                            CYPHER_DEBUG("UNWIND+CREATE+SET (range): SET failed");
                        }
                    }
                    if (create_vars) free_variable_map(create_vars);
                }
                g_foreach_ctx = prev_ctx;
                free_foreach_context(ctx);
                result->success = true;
                return 0;
            }
        }
    }

    /* Handle list literal UNWIND. Other expression shapes (function
     * calls like range(), variable references, list concatenation) are
     * outside this fast-path; defer to the generic transform pipeline
     * which has fuller UNWIND support. */
    if (unwind->expr->type != AST_NODE_LIST) {
        return handle_generic_transform(executor, query, result, flags);
    }

    cypher_list *list = (cypher_list *)unwind->expr;
    if (!list->items || list->items->count == 0) {
        /* Empty list - nothing to create */
        result->success = true;
        return 0;
    }

    /* Create foreach context for variable binding */
    foreach_context *ctx = create_foreach_context();
    if (!ctx) {
        set_result_error(result, "Failed to create foreach context");
        return -1;
    }

    /* Save previous context and set ours */
    foreach_context *prev_ctx = g_foreach_ctx;
    g_foreach_ctx = ctx;

    /* Iterate over list items and create nodes */
    for (int i = 0; i < list->items->count; i++) {
        ast_node *item = list->items->items[i];

        /* Bind the loop variable based on item type */
        if (item->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal *)item;
            switch (lit->literal_type) {
                case LITERAL_INTEGER:
                    set_foreach_binding_int(ctx, unwind->alias, lit->value.integer);
                    break;
                case LITERAL_STRING:
                    set_foreach_binding_string(ctx, unwind->alias, lit->value.string);
                    break;
                case LITERAL_DECIMAL:
                    set_foreach_binding_int(ctx, unwind->alias, (int64_t)lit->value.decimal);
                    break;
                default:
                    CYPHER_DEBUG("Unsupported literal type in UNWIND list: %d", lit->literal_type);
                    continue;
            }
        } else {
            CYPHER_DEBUG("Unsupported item type in UNWIND list: %d", item->type);
            continue;
        }

        CYPHER_DEBUG("UNWIND+CREATE iteration %d, variable=%s", i, unwind->alias);

        /* Execute CREATE, capturing the variable map */
        variable_map *create_vars = NULL;
        if (execute_create_clause_with_varmap(executor, create, result, &create_vars) < 0) {
            g_foreach_ctx = prev_ctx;
            free_foreach_context(ctx);
            return -1;
        }

        /* Execute SET if present, using variable map from CREATE */
        if (set && create_vars) {
            if (execute_set_operations(executor, set, create_vars, result) < 0) {
                CYPHER_DEBUG("UNWIND+CREATE+SET: SET failed");
            }
            free_variable_map(create_vars);
        } else if (create_vars) {
            free_variable_map(create_vars);
        }
    }

    /* Restore previous context */
    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);

    result->success = true;
    return 0;
}

/*
 * UNWIND+MERGE handler - iterates over list/parameter and merges nodes per item
 */
static int handle_unwind_merge(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_unwind *unwind = find_unwind_clause(query);
    cypher_merge *merge = NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MERGE) {
            merge = (cypher_merge*)query->clauses->items[i];
            break;
        }
    }

    if (!unwind || !merge) {
        set_result_error(result, "UNWIND+MERGE: missing clause");
        return -1;
    }

    CYPHER_DEBUG("Executing UNWIND+MERGE via pattern dispatch");

    /* Handle parameterized UNWIND */
    if (unwind->expr->type == AST_NODE_PARAMETER) {
        cypher_parameter *param = (cypher_parameter*)unwind->expr;
        if (!executor->params_json) {
            set_result_error(result, "UNWIND $param requires parameters");
            return -1;
        }

        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT value FROM json_each(json_extract(?, '$.%s'))", param->name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            set_result_error(result, "Failed to prepare UNWIND parameter query");
            return -1;
        }
        sqlite3_bind_text(stmt, 1, executor->params_json, -1, SQLITE_STATIC);

        foreach_context *ctx = create_foreach_context();
        if (!ctx) { sqlite3_finalize(stmt); set_result_error(result, "Failed to create foreach context"); return -1; }
        foreach_context *prev_ctx = g_foreach_ctx;
        g_foreach_ctx = ctx;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int col_type = sqlite3_column_type(stmt, 0);
            if (col_type == SQLITE_TEXT) {
                set_foreach_binding_string(ctx, unwind->alias,
                    (const char*)sqlite3_column_text(stmt, 0));
            } else if (col_type == SQLITE_INTEGER) {
                set_foreach_binding_int(ctx, unwind->alias, sqlite3_column_int64(stmt, 0));
            }

            if (execute_merge_clause(executor, merge, result, NULL, NULL) < 0) {
                g_foreach_ctx = prev_ctx;
                free_foreach_context(ctx);
                sqlite3_finalize(stmt);
                return -1;
            }
        }

        g_foreach_ctx = prev_ctx;
        free_foreach_context(ctx);
        sqlite3_finalize(stmt);
        result->success = true;
        return 0;
    }

    /* Handle list literal UNWIND */
    if (unwind->expr->type == AST_NODE_LIST) {
        cypher_list *list = (cypher_list *)unwind->expr;
        if (!list->items || list->items->count == 0) {
            result->success = true;
            return 0;
        }

        foreach_context *ctx = create_foreach_context();
        if (!ctx) { set_result_error(result, "Failed to create foreach context"); return -1; }
        foreach_context *prev_ctx = g_foreach_ctx;
        g_foreach_ctx = ctx;

        for (int i = 0; i < list->items->count; i++) {
            ast_node *item = list->items->items[i];
            if (item->type == AST_NODE_LITERAL) {
                cypher_literal *lit = (cypher_literal *)item;
                switch (lit->literal_type) {
                    case LITERAL_INTEGER: set_foreach_binding_int(ctx, unwind->alias, lit->value.integer); break;
                    case LITERAL_STRING: set_foreach_binding_string(ctx, unwind->alias, lit->value.string); break;
                    default: continue;
                }
            } else {
                continue;
            }

            if (execute_merge_clause(executor, merge, result, NULL, NULL) < 0) {
                g_foreach_ctx = prev_ctx;
                free_foreach_context(ctx);
                return -1;
            }
        }

        g_foreach_ctx = prev_ctx;
        free_foreach_context(ctx);
        result->success = true;
        return 0;
    }

    set_result_error(result, "UNWIND+MERGE requires list literal or parameter");
    return -1;
}

/*
 * Standalone RETURN handler - handles graph algorithms and expressions
 */
static int handle_return_only(cypher_executor *executor, cypher_query *query,
                              cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_return *ret = find_return_clause(query);

    CYPHER_DEBUG("Executing standalone RETURN via pattern dispatch");

    /* Check for graph algorithm functions - execute in C for performance */
    graph_algo_params algo_params = detect_graph_algorithm(ret, executor->params_json);
    if (algo_params.type != GRAPH_ALGO_NONE) {
        graph_algo_result *algo_result = NULL;

        /* A CSR graph loaded before a write is stale: rebuild it in place so
         * gql_load_graph() callers keep seeing current data. */
        if (executor->cached_graph && executor->graph_dirty && executor->cached_graph_slot) {
            CYPHER_DEBUG("Cached graph is stale after a write; reloading");
            csr_graph_free(executor->cached_graph);
            csr_graph *fresh = csr_graph_load(executor->db);
            *executor->cached_graph_slot = fresh;
            executor->cached_graph = fresh;
            executor->graph_dirty = false;
        }

        switch (algo_params.type) {
            case GRAPH_ALGO_PAGERANK:
                CYPHER_DEBUG("Executing C-based PageRank");
                algo_result = execute_pagerank(executor->db, executor->cached_graph,
                                               algo_params.damping,
                                               algo_params.iterations,
                                               algo_params.top_k);
                break;
            case GRAPH_ALGO_LABEL_PROPAGATION:
                CYPHER_DEBUG("Executing C-based Label Propagation");
                algo_result = execute_label_propagation(executor->db, executor->cached_graph,
                                                        algo_params.iterations);
                break;
            case GRAPH_ALGO_DIJKSTRA:
                CYPHER_DEBUG("Executing C-based Dijkstra");
                algo_result = execute_dijkstra(executor->db, executor->cached_graph,
                                               algo_params.source_id,
                                               algo_params.target_id,
                                               algo_params.weight_prop);
                free(algo_params.source_id);
                free(algo_params.target_id);
                free(algo_params.weight_prop);
                break;
            case GRAPH_ALGO_DEGREE_CENTRALITY:
                CYPHER_DEBUG("Executing C-based Degree Centrality");
                algo_result = execute_degree_centrality(executor->db, executor->cached_graph);
                break;
            case GRAPH_ALGO_WCC:
                CYPHER_DEBUG("Executing C-based Weakly Connected Components");
                algo_result = execute_wcc(executor->db, executor->cached_graph);
                break;
            case GRAPH_ALGO_SCC:
                CYPHER_DEBUG("Executing C-based Strongly Connected Components");
                algo_result = execute_scc(executor->db, executor->cached_graph);
                break;
            case GRAPH_ALGO_BETWEENNESS_CENTRALITY:
                CYPHER_DEBUG("Executing C-based Betweenness Centrality");
                algo_result = execute_betweenness_centrality(executor->db, executor->cached_graph);
                break;
            case GRAPH_ALGO_CLOSENESS_CENTRALITY:
                CYPHER_DEBUG("Executing C-based Closeness Centrality");
                algo_result = execute_closeness_centrality(executor->db, executor->cached_graph);
                break;
            case GRAPH_ALGO_LOUVAIN:
                CYPHER_DEBUG("Executing C-based Louvain Community Detection");
                algo_result = execute_louvain(executor->db, executor->cached_graph, algo_params.resolution);
                break;
            case GRAPH_ALGO_TRIANGLE_COUNT:
                CYPHER_DEBUG("Executing C-based Triangle Count");
                algo_result = execute_triangle_count(executor->db, executor->cached_graph);
                break;
            case GRAPH_ALGO_ASTAR:
                CYPHER_DEBUG("Executing C-based A* Shortest Path");
                algo_result = execute_astar(executor->db, executor->cached_graph, algo_params.source_id,
                                            algo_params.target_id, algo_params.weight_prop,
                                            algo_params.lat_prop, algo_params.lon_prop);
                break;
            case GRAPH_ALGO_BFS:
                CYPHER_DEBUG("Executing C-based BFS Traversal");
                algo_result = execute_bfs(executor->db, executor->cached_graph, algo_params.source_id,
                                          algo_params.max_depth);
                break;
            case GRAPH_ALGO_DFS:
                CYPHER_DEBUG("Executing C-based DFS Traversal");
                algo_result = execute_dfs(executor->db, executor->cached_graph, algo_params.source_id,
                                          algo_params.max_depth);
                break;
            case GRAPH_ALGO_NODE_SIMILARITY:
                CYPHER_DEBUG("Executing C-based Node Similarity (Jaccard)");
                algo_result = execute_node_similarity(executor->db, executor->cached_graph,
                                                      algo_params.source_id,
                                                      algo_params.target_id,
                                                      algo_params.threshold,
                                                      algo_params.top_k);
                break;
            case GRAPH_ALGO_KNN:
                CYPHER_DEBUG("Executing C-based K-Nearest Neighbors");
                algo_result = execute_knn(executor->db, executor->cached_graph,
                                          algo_params.source_id,
                                          algo_params.k);
                break;
            case GRAPH_ALGO_EIGENVECTOR_CENTRALITY:
                CYPHER_DEBUG("Executing C-based Eigenvector Centrality");
                algo_result = execute_eigenvector_centrality(executor->db, executor->cached_graph,
                                                              algo_params.iterations);
                break;
            case GRAPH_ALGO_APSP:
                CYPHER_DEBUG("Executing C-based All Pairs Shortest Path");
                algo_result = execute_apsp(executor->db, executor->cached_graph);
                break;
            default:
                break;
        }

        if (algo_result) {
            if (algo_result->success) {
                result->column_count = 1;
                result->row_count = 1;
                result->data = malloc(sizeof(char**));
                result->data[0] = malloc(sizeof(char*));
                result->data[0][0] = strdup(algo_result->json_result);
                result->success = true;
            } else {
                set_result_error(result, algo_result->error_message ?
                                 algo_result->error_message : "Graph algorithm failed");
            }
            graph_algo_result_free(algo_result);
            return result->success ? 0 : -1;
        }
    }

    /* Standard SQL-based execution for non-algorithm queries */
    return handle_generic_transform(executor, query, result, flags);
}

/*
 * CREATE+RETURN handler
 *
 * Executes the CREATE clause, then queries the created nodes to build
 * the RETURN result.
 */
/* GQLITE-T-0371: RETURN after write-only clauses (CREATE / MERGE, no
 * MATCH) used to be projected by hand from the variable map, which cannot
 * evaluate aggregates, path variables (`MERGE p = (..) RETURN p`), nested
 * function calls (`startNode(r).id`) or arithmetic. For those shapes we
 * re-run the normal MATCH+RETURN pipeline over the written patterns,
 * constrained to exactly the bound entity ids (`id(a) = 5 AND id(r) = 7`),
 * so the result is the written row and nothing else. */
static bool return_item_is_hand_projectable(cypher_return_item *item, variable_map *vm)
{
    ast_node *expr = item ? item->expr : NULL;
    if (!expr) return true;
    switch (expr->type) {
        case AST_NODE_LITERAL:
            return true;
        case AST_NODE_IDENTIFIER: {
            const char *n = ((cypher_identifier*)expr)->name;
            return get_variable_node_id(vm, n) >= 0 || get_variable_edge_id(vm, n) >= 0;
        }
        case AST_NODE_PROPERTY: {
            cypher_property *prop = (cypher_property*)expr;
            if (!prop->expr || prop->expr->type != AST_NODE_IDENTIFIER) return false;
            const char *n = ((cypher_identifier*)prop->expr)->name;
            return get_variable_node_id(vm, n) >= 0 || get_variable_edge_id(vm, n) >= 0;
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *fc = (cypher_function_call*)expr;
            if (aggregating_call_name(expr)) return false;
            if (!fc->args || fc->args->count != 1) return false;
            ast_node *arg = fc->args->items[0];
            if (!arg || arg->type != AST_NODE_IDENTIFIER) return false;
            const char *n = ((cypher_identifier*)arg)->name;
            return get_variable_node_id(vm, n) >= 0 || get_variable_edge_id(vm, n) >= 0;
        }
        default:
            return false;
    }
}

static bool return_needs_bound_rematch(cypher_return *ret, variable_map *vm)
{
    if (!ret || !ret->items || !vm) return false;
    for (int i = 0; i < ret->items->count; i++) {
        if (!return_item_is_hand_projectable((cypher_return_item*)ret->items->items[i], vm))
            return true;
    }
    return false;
}

static void append_pattern_items(ast_list *dst, ast_list *src)
{
    if (!src) return;
    for (int i = 0; i < src->count; i++) ast_list_append(dst, src->items[i]);
}

static int execute_return_via_bound_rematch(cypher_executor *executor, cypher_query *query,
                                            variable_map *vm, cypher_return *ret,
                                            cypher_result *result)
{
    ast_list *combined = ast_list_create();
    if (!combined) { set_result_error(result, "OOM"); return -1; }
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        if (c->type == AST_NODE_CREATE) append_pattern_items(combined, ((cypher_create*)c)->pattern);
        else if (c->type == AST_NODE_MERGE) append_pattern_items(combined, ((cypher_merge*)c)->pattern);
        else if (c->type == AST_NODE_MATCH) append_pattern_items(combined, ((cypher_match*)c)->pattern);
    }

    /* id(var) = <bound id> for every binding, ANDed together. */
    ast_node *where = NULL;
    for (int i = 0; i < vm->count; i++) {
        variable_mapping *m = &vm->mappings[i];
        if (!m->variable || m->entity_id < 0) continue;
        ast_list *args = ast_list_create();
        if (!args) continue;
        ast_list_append(args, (ast_node*)make_identifier(m->variable, 0));
        ast_node *fn = (ast_node*)make_function_call("id", args, false, 0);
        ast_node *eq = (ast_node*)make_binary_op(BINARY_OP_EQ, fn,
                                                 (ast_node*)make_integer_literal(m->entity_id, 0), 0);
        where = where ? (ast_node*)make_binary_op(BINARY_OP_AND, where, eq, 0) : eq;
    }

    cypher_match *synth = make_cypher_match(combined, where, false, NULL);
    int rc = -1;
    if (synth) {
        rc = execute_match_return_query(executor, synth, ret, result);
        free(synth);
    } else {
        set_result_error(result, "OOM");
    }
    if (where) ast_node_free(where);
    free(combined->items);
    free(combined);
    return rc;
}

static int handle_create_return(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_return *ret = find_return_clause(query);

    CYPHER_DEBUG("Executing CREATE+RETURN via pattern dispatch");

    /* Execute every CREATE clause in document order, accumulating bindings,
     * before fetching the RETURN data. Previously only the first CREATE ran. */
    variable_map *var_map = NULL;
    int rc = 0;
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_CREATE) {
            rc = execute_create_clause_with_varmap(executor,
                                                    (cypher_create *)clause,
                                                    result, &var_map);
            if (rc < 0) {
                if (var_map) free_variable_map(var_map);
                return -1;
            }
        } else if (clause->type == AST_NODE_MERGE) {
            /* GQLITE-T-0371: a MERGE between CREATE and RETURN used to be
             * skipped entirely (Merge5 [4], Merge9 [3]). Run it against the
             * bindings so far and fold its bindings back in. */
            variable_map *merged = NULL;
            rc = execute_merge_clause(executor, (cypher_merge *)clause, result,
                                      var_map, &merged);
            if (rc < 0) {
                if (var_map) free_variable_map(var_map);
                if (merged) free_variable_map(merged);
                return -1;
            }
            if (merged) {
                if (!var_map) var_map = create_variable_map();
                for (int mi = 0; var_map && mi < merged->count; mi++) {
                    variable_mapping *m = &merged->mappings[mi];
                    if (m->type == VAR_MAP_TYPE_NODE) set_variable_node_id(var_map, m->variable, m->entity_id);
                    else set_variable_edge_id(var_map, m->variable, m->entity_id);
                }
                free_variable_map(merged);
            }
        }
    }

    /* Run any SET clauses between CREATE and RETURN against the created
     * bindings, in document order (CREATE (a {..}) SET a.x = .. RETURN a.x).
     * Previously the CREATE+RETURN handler ignored SET entirely. (Set1 [6]/[7].) */
    for (int i = 0; var_map && query->clauses && i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type != AST_NODE_SET) continue;
        if (execute_set_operations(executor, (cypher_set *)clause, var_map, result) < 0) {
            free_variable_map(var_map);
            return -1;
        }
    }

    if (!var_map || var_map->count == 0 || !ret || !ret->items) {
        result->success = true;
        if (var_map) free_variable_map(var_map);
        return 0;
    }

    /* GQLITE-T-0371: aggregates, path variables, nested calls → re-match. */
    if (return_needs_bound_rematch(ret, var_map)) {
        rc = execute_return_via_bound_rematch(executor, query, var_map, ret, result);
        free_variable_map(var_map);
        if (rc >= 0) result->success = true;
        return rc;
    }

    /* Honor LIMIT 0 / SKIP for CREATE+RETURN: side effects are already
     * committed; LIMIT just clips the result set. */
    int64_t limit_val = -1;
    int64_t skip_val = 0;
    if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal *)ret->limit;
        if (l->literal_type == LITERAL_INTEGER) limit_val = l->value.integer;
    }
    if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal *)ret->skip;
        if (l->literal_type == LITERAL_INTEGER) skip_val = l->value.integer;
    }
    int produced_rows = (skip_val > 0) ? 0 : 1;
    if (limit_val == 0) produced_rows = 0;

    /* Build a SQL query to fetch the RETURN data from created nodes.
     * For each return item like p.name, generate:
     *   SELECT value FROM node_props_text WHERE node_id = ? AND key_id = (
     *     SELECT id FROM property_keys WHERE key = 'name')
     * We build a single row with all requested columns. */
    int col_count = ret->items->count;
    result->column_count = col_count;
    result->column_names = malloc(col_count * sizeof(char*));
    result->row_count = produced_rows;
    if (produced_rows == 0) {
        /* Set column names so the harness sees the schema, then return. */
        for (int i = 0; i < col_count; i++) {
            cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
            result->column_names[i] = strdup(item->alias ? item->alias : "?column?");
        }
        result->data = NULL;
        result->data_types = NULL;
        result->success = true;
        free_variable_map(var_map);
        return 0;
    }
    int orig_row_count = 1;
    (void)orig_row_count;
    result->data = malloc(sizeof(char**));
    result->data[0] = malloc(col_count * sizeof(char*));
    /* GQLITE-T-0227: track per-cell SQLite types so the JSON formatter can
     * emit integers/floats unquoted instead of stringifying everything. */
    result->data_types = malloc(sizeof(int*));
    result->data_types[0] = calloc(col_count, sizeof(int));

    for (int i = 0; i < col_count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        const char *alias = item->alias;
        ast_node *expr = item->expr;

        /* Determine column name */
        if (alias) {
            result->column_names[i] = strdup(alias);
        } else {
            /* Build name from expression */
            result->column_names[i] = strdup("?column?");
        }

        result->data[0][i] = NULL;

        /* Handle property access: p.name */
        if (expr && expr->type == AST_NODE_PROPERTY) {
            cypher_property *prop = (cypher_property*)expr;
            const char *prop_name = prop->property_name;
            const char *var_name = NULL;

            /* Get variable name from the base expression */
            if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
                var_name = ((cypher_identifier*)prop->expr)->name;
            }

            if (var_name && prop_name) {
                /* Build column name like "p.name" if no alias */
                if (!alias) {
                    free(result->column_names[i]);
                    char col_name[256];
                    snprintf(col_name, sizeof(col_name), "%s.%s", var_name, prop_name);
                    result->column_names[i] = strdup(col_name);
                }

                int node_id = get_variable_node_id(var_map, var_name);
                int edge_id = (node_id < 0) ? get_variable_edge_id(var_map, var_name) : -1;
                bool is_edge = (edge_id >= 0);
                int entity_id = is_edge ? edge_id : node_id;
                if (entity_id >= 0) {
                    /* Query each property type table for the value */
                    const char *node_tables[] = {
                        "node_props_text", "node_props_int",
                        "node_props_real", "node_props_bool", "node_props_json", NULL
                    };
                    const char *edge_tables[] = {
                        "edge_props_text", "edge_props_int",
                        "edge_props_real", "edge_props_bool", "edge_props_json", NULL
                    };
                    const char **type_tables = is_edge ? edge_tables : node_tables;
                    const char *id_col = is_edge ? "edge_id" : "node_id";
                    for (int t = 0; type_tables[t]; t++) {
                        char sql[512];
                        snprintf(sql, sizeof(sql),
                            "SELECT value FROM %s "
                            "WHERE %s = %d AND key_id = "
                            "(SELECT id FROM property_keys WHERE key = '%s')",
                            type_tables[t], id_col, entity_id, prop_name);

                        sqlite3_stmt *stmt;
                        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                            if (sqlite3_step(stmt) == SQLITE_ROW) {
                                int sql_type = sqlite3_column_type(stmt, 0);
                                const char *val = (const char*)sqlite3_column_text(stmt, 0);
                                if (val) {
                                    /* {node,edge}_props_bool stores 0/1; expose as
                                     * "true"/"false" so the JSON formatter
                                     * treats it as a string and openCypher's
                                     * boolean literal is preserved. */
                                    if (strcmp(type_tables[t], "node_props_bool") == 0 ||
                                        strcmp(type_tables[t], "edge_props_bool") == 0) {
                                        result->data[0][i] = strdup(atoi(val) ? "true" : "false");
                                        result->data_types[0][i] = SQLITE_TEXT;
                                    } else {
                                        result->data[0][i] = strdup(val);
                                        result->data_types[0][i] = sql_type;
                                    }
                                }
                            }
                            sqlite3_finalize(stmt);
                        }
                        if (result->data[0][i]) break; /* Found it */
                    }
                }
            }
        } else if (expr && expr->type == AST_NODE_IDENTIFIER) {
            /* Return whole node: RETURN p — return node ID for now */
            const char *var_name = ((cypher_identifier*)expr)->name;
            int node_id = get_variable_node_id(var_map, var_name);
            if (node_id >= 0) {
                if (!alias) {
                    free(result->column_names[i]);
                    result->column_names[i] = strdup(var_name);
                }
                char id_str[32];
                snprintf(id_str, sizeof(id_str), "%d", node_id);
                result->data[0][i] = strdup(id_str);
            }
        } else if (expr && expr->type == AST_NODE_FUNCTION_CALL) {
            /* T-0325: handle the common entity functions in
             * CREATE+RETURN — labels(), id(), keys(). Without this,
             * `CREATE (n) RETURN labels(n)` yielded `?column?: null`.
             * Build a synthetic column name from the function call
             * and run a one-shot SQL query to compute the value. */
            cypher_function_call *fc = (cypher_function_call*)expr;
            const char *fname = fc->function_name ? fc->function_name : "";
            const char *var_name = NULL;
            if (fc->args && fc->args->count == 1 &&
                fc->args->items[0] &&
                fc->args->items[0]->type == AST_NODE_IDENTIFIER) {
                var_name = ((cypher_identifier*)fc->args->items[0])->name;
            }
            if (var_name && (strcmp(fname, "labels") == 0 ||
                             strcmp(fname, "id") == 0 ||
                             strcmp(fname, "keys") == 0)) {
                if (!alias) {
                    free(result->column_names[i]);
                    char col_name[256];
                    snprintf(col_name, sizeof(col_name), "%s(%s)", fname, var_name);
                    result->column_names[i] = strdup(col_name);
                }
                int node_id = get_variable_node_id(var_map, var_name);
                int edge_id = (node_id < 0) ? get_variable_edge_id(var_map, var_name) : -1;
                bool is_edge = (edge_id >= 0);
                int entity_id = is_edge ? edge_id : node_id;
                if (entity_id >= 0) {
                    char sql[512];
                    if (strcmp(fname, "labels") == 0 && !is_edge) {
                        /* json_group_array over an empty result set
                         * yields NULL; COALESCE to json('[]') for
                         * the no-label case. */
                        snprintf(sql, sizeof(sql),
                            "SELECT COALESCE((SELECT json_group_array(label) "
                            "FROM node_labels WHERE node_id = %d), json('[]'))",
                            entity_id);
                    } else if (strcmp(fname, "id") == 0) {
                        snprintf(sql, sizeof(sql), "SELECT %d", entity_id);
                    } else if (strcmp(fname, "keys") == 0) {
                        const char *id_col = is_edge ? "edge_id" : "node_id";
                        const char *t = is_edge ? "edge_props" : "node_props";
                        snprintf(sql, sizeof(sql),
                            "SELECT COALESCE((SELECT json_group_array(pk.key) "
                            "FROM (SELECT %s, key_id FROM %s_text WHERE %s = %d "
                            "UNION SELECT %s, key_id FROM %s_int WHERE %s = %d "
                            "UNION SELECT %s, key_id FROM %s_real WHERE %s = %d "
                            "UNION SELECT %s, key_id FROM %s_bool WHERE %s = %d) u "
                            "JOIN property_keys pk ON pk.id = u.key_id), json('[]'))",
                            id_col, t, id_col, entity_id,
                            id_col, t, id_col, entity_id,
                            id_col, t, id_col, entity_id,
                            id_col, t, id_col, entity_id);
                    } else {
                        sql[0] = '\0';
                    }
                    if (sql[0]) {
                        sqlite3_stmt *stmt;
                        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                            if (sqlite3_step(stmt) == SQLITE_ROW) {
                                int sql_type = sqlite3_column_type(stmt, 0);
                                const unsigned char *val = sqlite3_column_text(stmt, 0);
                                if (val) {
                                    result->data[0][i] = strdup((const char*)val);
                                    result->data_types[0][i] = sql_type;
                                }
                            }
                            sqlite3_finalize(stmt);
                        }
                    }
                }
            }
        } else if (expr) {
            /* Any other expression (binary op, subscript like n['na'+'e'],
             * list/map literal, ...) — evaluate against the created bindings.
             * (Graph7 [2]/[3], RETURN a.numbers + [..], etc.) */
            property_type pt; property_value pv; property_value_init(&pv);
            int erc = executor_eval_value(executor, expr, var_map, &pt, &pv);
            if (erc == 0) {
                if (pt == PROP_TYPE_INTEGER) {
                    char b[32]; snprintf(b, sizeof(b), "%lld", (long long)pv.as_int);
                    result->data[0][i] = strdup(b);
                    result->data_types[0][i] = SQLITE_INTEGER;
                } else if (pt == PROP_TYPE_REAL) {
                    char b[64]; snprintf(b, sizeof(b), "%g", pv.as_real);
                    result->data[0][i] = strdup(b);
                    result->data_types[0][i] = SQLITE_FLOAT;
                } else if (pv.as_str) {
                    result->data[0][i] = strdup(pv.as_str);
                    result->data_types[0][i] = SQLITE_TEXT;
                }
            }
            property_value_free(&pv);
        }
    }

    result->success = true;
    free_variable_map(var_map);
    return 0;
}

/*
 * UNWIND+CREATE+RETURN handler
 * Iterates the UNWIND list, executes CREATE per iteration, collects each
 * var_map, then projects one result row per iteration.
 */
static int handle_unwind_create_return(cypher_executor *executor, cypher_query *query,
                                       cypher_result *result, clause_flags flags)
{
    if (unwind_create_needs_pipeline(query)) return handle_rowwise_write(executor, query, result, flags);
    (void)flags;
    cypher_unwind *unwind = find_unwind_clause(query);
    cypher_create *create = find_create_clause(query);
    cypher_return *ret = find_return_clause(query);
    if (!unwind || !create || !ret || !ret->items) {
        set_result_error(result, "UNWIND+CREATE+RETURN: missing clause");
        return -1;
    }

    CYPHER_DEBUG("Executing UNWIND+CREATE+RETURN via pattern dispatch");

    if (unwind->expr->type != AST_NODE_LIST) {
        set_result_error(result, "UNWIND+CREATE+RETURN requires a list literal");
        return -1;
    }
    cypher_list *list = (cypher_list*)unwind->expr;

    set_return_column_names(ret, result);
    int col_count = ret->items->count;

    if (!list->items || list->items->count == 0) {
        result->row_count = 0;
        result->data = NULL;
        result->data_types = NULL;
        result->success = true;
        return 0;
    }

    foreach_context *ctx = create_foreach_context();
    if (!ctx) {
        set_result_error(result, "Failed to create foreach context");
        return -1;
    }
    foreach_context *prev_ctx = g_foreach_ctx;
    g_foreach_ctx = ctx;

    int cap = list->items->count;
    variable_map **maps = calloc(cap, sizeof(variable_map*));
    int n_maps = 0;

    for (int i = 0; i < list->items->count; i++) {
        ast_node *item = list->items->items[i];
        if (item->type != AST_NODE_LITERAL) {
            CYPHER_DEBUG("UNWIND+CREATE+RETURN: skipping non-literal item %d", item->type);
            continue;
        }
        cypher_literal *lit = (cypher_literal*)item;
        switch (lit->literal_type) {
            case LITERAL_INTEGER:
                set_foreach_binding_int(ctx, unwind->alias, lit->value.integer);
                break;
            case LITERAL_STRING:
                set_foreach_binding_string(ctx, unwind->alias, lit->value.string);
                break;
            case LITERAL_DECIMAL:
                set_foreach_binding_int(ctx, unwind->alias, (int64_t)lit->value.decimal);
                break;
            default:
                continue;
        }
        variable_map *vm = NULL;
        if (execute_create_clause_with_varmap(executor, create, result, &vm) < 0) {
            for (int j = 0; j < n_maps; j++) free_variable_map(maps[j]);
            free(maps);
            g_foreach_ctx = prev_ctx;
            free_foreach_context(ctx);
            return -1;
        }
        if (vm) maps[n_maps++] = vm;
    }

    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);

    /* T-0328: honor intermediate WITH-WHERE between CREATE and
     * RETURN. For each `WITH … WHERE pred`, filter `maps` by
     * evaluating `pred` against each map's bindings; drop maps
     * whose predicate is false/null. CREATE side effects already
     * happened — only the projected result set is filtered.
     *
     * Only handles WITH with a WHERE; aggregating WITH (e.g.
     * `WITH count(*) AS c`) is out of A4's scope and tracked
     * under B4. Skip those WITHs silently and let the
     * downstream projection see the unfiltered maps. */
    if (query->clauses) {
        for (int ci = 0; ci < query->clauses->count; ci++) {
            ast_node *cl = query->clauses->items[ci];
            if (cl->type != AST_NODE_WITH) continue;
            cypher_with *w = (cypher_with *)cl;
            if (!w->where) continue;
            /* Aggregating WITH — skip; B4 handles that. */
            bool agg_with = false;
            if (w->items) {
                for (int wi = 0; wi < w->items->count; wi++) {
                    cypher_return_item *it = (cypher_return_item *)w->items->items[wi];
                    if (it && aggregating_call_name(it->expr)) {
                        agg_with = true;
                        break;
                    }
                }
            }
            if (agg_with) continue;
            int kept = 0;
            for (int mi = 0; mi < n_maps; mi++) {
                int p = executor_eval_predicate(executor, w->where, maps[mi]);
                if (p > 0) {
                    if (kept != mi) maps[kept] = maps[mi];
                    kept++;
                } else {
                    free_variable_map(maps[mi]);
                }
            }
            n_maps = kept;
        }
    }

    /* B4: aggregating WITH between CREATE and RETURN, e.g.
     *   UNWIND [..] AS x CREATE (n {num:x}) WITH sum(n.num) AS s RETURN s
     * When the WITH is pure-aggregate (every item is an aggregate, no
     * grouping keys) it collapses the per-iteration maps to one row. Compute
     * each WITH aggregate across `maps`, then satisfy RETURN by resolving its
     * references to the WITH aliases. (Create6 [7]/[14].) */
    {
        cypher_with *agg_w = NULL;
        if (query->clauses) {
            for (int ci = 0; ci < query->clauses->count; ci++) {
                ast_node *cl = query->clauses->items[ci];
                if (cl->type != AST_NODE_WITH) continue;
                cypher_with *w = (cypher_with *)cl;
                bool has_agg = false, has_key = false;
                if (w->items) {
                    for (int wi = 0; wi < w->items->count; wi++) {
                        cypher_return_item *it = (cypher_return_item *)w->items->items[wi];
                        if (it && aggregating_call_name(it->expr)) has_agg = true;
                        else has_key = true;
                    }
                }
                if (has_agg && !has_key) { agg_w = w; break; }
            }
        }
        if (agg_w) {
            int wn = agg_w->items->count;
            char **wvals = calloc(wn, sizeof(char*));
            int *wtypes = calloc(wn, sizeof(int));
            const char **walias = calloc(wn, sizeof(char*));
            for (int wi = 0; wi < wn; wi++) {
                cypher_return_item *it = (cypher_return_item *)agg_w->items->items[wi];
                walias[wi] = it->alias;
                cypher_result tmp;
                memset(&tmp, 0, sizeof(tmp));
                tmp.data = malloc(sizeof(char**));
                tmp.data[0] = calloc(1, sizeof(char*));
                tmp.data_types = malloc(sizeof(int*));
                tmp.data_types[0] = calloc(1, sizeof(int));
                project_aggregate_cell(executor, it, maps, n_maps, &tmp, 0);
                wvals[wi] = tmp.data[0][0];
                wtypes[wi] = tmp.data_types[0][0];
                free(tmp.data[0]); free(tmp.data);
                free(tmp.data_types[0]); free(tmp.data_types);
            }
            result->row_count = 1;
            result->data = malloc(sizeof(char**));
            result->data_types = malloc(sizeof(int*));
            result->data[0] = malloc(col_count * sizeof(char*));
            result->data_types[0] = calloc(col_count, sizeof(int));
            for (int i = 0; i < col_count; i++) {
                cypher_return_item *rit = (cypher_return_item *)ret->items->items[i];
                result->data[0][i] = NULL;
                const char *ref = NULL;
                if (rit->expr && rit->expr->type == AST_NODE_IDENTIFIER)
                    ref = ((cypher_identifier *)rit->expr)->name;
                if (ref) {
                    for (int wi = 0; wi < wn; wi++) {
                        if (walias[wi] && strcmp(walias[wi], ref) == 0) {
                            result->data[0][i] = wvals[wi] ? strdup(wvals[wi]) : NULL;
                            result->data_types[0][i] = wtypes[wi];
                            break;
                        }
                    }
                }
            }
            for (int wi = 0; wi < wn; wi++) free(wvals[wi]);
            free(wvals); free(wtypes); free(walias);
            for (int j = 0; j < n_maps; j++) free_variable_map(maps[j]);
            free(maps);
            result->success = true;
            return 0;
        }
    }

    /* SKIP/LIMIT */
    int64_t limit_val = -1, skip_val = 0;
    if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal*)ret->limit;
        if (l->literal_type == LITERAL_INTEGER) limit_val = l->value.integer;
    }
    if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal*)ret->skip;
        if (l->literal_type == LITERAL_INTEGER) skip_val = l->value.integer;
    }
    int start = 0;
    if (skip_val > 0) start = (skip_val >= n_maps) ? n_maps : (int)skip_val;
    int end = n_maps;
    if (limit_val == 0) end = start;
    else if (limit_val > 0 && start + (int)limit_val < end) end = start + (int)limit_val;
    int produced = end - start;
    if (produced < 0) produced = 0;

    bool agg = return_has_aggregation(ret);
    if (agg) {
        /* Single aggregated row across all (post-skip/limit) maps. */
        result->row_count = 1;
        result->data = malloc(sizeof(char**));
        result->data_types = malloc(sizeof(int*));
        result->data[0] = malloc(col_count * sizeof(char*));
        result->data_types[0] = calloc(col_count, sizeof(int));
        for (int i = 0; i < col_count; i++) {
            cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
            if (aggregating_call_name(it->expr)) {
                project_aggregate_cell(executor, it, maps + start, produced, result, i);
            } else {
                /* Non-aggregated item with aggregation present: use first map. */
                if (produced > 0) {
                    /* Temporarily project one row's worth via helper-style code */
                    char **save_data = result->data[0];
                    int *save_types = result->data_types[0];
                    char ***save_data_all = result->data;
                    int **save_types_all = result->data_types;
                    char **tmp = malloc(col_count * sizeof(char*));
                    int *tmp_t = calloc(col_count, sizeof(int));
                    result->data = &tmp;
                    result->data_types = &tmp_t;
                    project_return_row_from_var_map(executor, ret, maps[start], result, 0);
                    result->data = save_data_all;
                    result->data_types = save_types_all;
                    save_data[i] = tmp[i];
                    save_types[i] = tmp_t[i];
                    /* Free the other tmp cells we didn't use */
                    for (int k = 0; k < col_count; k++) if (k != i && tmp[k]) free(tmp[k]);
                    free(tmp); free(tmp_t);
                } else {
                    result->data[0][i] = NULL;
                }
            }
        }
    } else {
        result->row_count = produced;
        if (produced == 0) {
            result->data = NULL;
            result->data_types = NULL;
        } else {
            result->data = malloc(produced * sizeof(char**));
            result->data_types = malloc(produced * sizeof(int*));
            for (int r = 0; r < produced; r++) {
                result->data[r] = malloc(col_count * sizeof(char*));
                result->data_types[r] = calloc(col_count, sizeof(int));
                project_return_row_from_var_map(executor, ret, maps[start + r], result, r);
            }
        }
    }

    for (int j = 0; j < n_maps; j++) free_variable_map(maps[j]);
    free(maps);
    result->success = true;
    return 0;
}

/*
 * UNWIND+MERGE+RETURN handler — mirrors UNWIND+CREATE+RETURN but with MERGE
 */
static int handle_unwind_merge_return(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_unwind *unwind = find_unwind_clause(query);
    cypher_merge *merge = NULL;
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MERGE) {
            merge = (cypher_merge*)query->clauses->items[i];
            break;
        }
    }
    cypher_return *ret = find_return_clause(query);
    if (!unwind || !merge || !ret || !ret->items) {
        set_result_error(result, "UNWIND+MERGE+RETURN: missing clause");
        return -1;
    }
    set_return_column_names(ret, result);
    int col_count = ret->items->count;

    foreach_context *ctx = create_foreach_context();
    if (!ctx) { set_result_error(result, "Failed to create foreach context"); return -1; }
    foreach_context *prev_ctx = g_foreach_ctx;
    g_foreach_ctx = ctx;

    /* Initial capacity grows as we iterate. */
    int cap = 8;
    variable_map **maps = calloc(cap, sizeof(variable_map*));
    int n_maps = 0;

    cypher_set *set = find_set_clause(query);

    /* Inner per-iteration body: MERGE + optional SET, append to maps[]. */
    int err_out = 0;
    #define UMR_RUN_BODY() do { \
        variable_map *vm = NULL; \
        if (execute_merge_clause(executor, merge, result, NULL, &vm) < 0) { err_out = -1; break; } \
        if (set && vm) { (void)execute_set_operations(executor, set, vm, result); } \
        if (vm) { \
            if (n_maps == cap) { cap *= 2; maps = realloc(maps, cap * sizeof(variable_map*)); } \
            maps[n_maps++] = vm; \
        } \
    } while (0)

    if (unwind->expr->type == AST_NODE_LIST) {
        cypher_list *list = (cypher_list*)unwind->expr;
        if (list->items) {
            for (int i = 0; i < list->items->count; i++) {
                ast_node *item = list->items->items[i];
                if (item->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)item;
                    switch (lit->literal_type) {
                        case LITERAL_INTEGER: set_foreach_binding_int(ctx, unwind->alias, lit->value.integer); break;
                        case LITERAL_STRING: set_foreach_binding_string(ctx, unwind->alias, lit->value.string); break;
                        case LITERAL_DECIMAL: set_foreach_binding_int(ctx, unwind->alias, (int64_t)lit->value.decimal); break;
                        default: continue;
                    }
                } else if (item->type == AST_NODE_MAP || item->type == AST_NODE_LIST) {
                    char *js = serialize_ast_to_json(item);
                    if (!js) continue;
                    set_foreach_binding_string(ctx, unwind->alias, js);
                    free(js);
                } else continue;
                UMR_RUN_BODY();
                if (err_out) break;
            }
        }
    } else if (unwind->expr->type == AST_NODE_PARAMETER) {
        cypher_parameter *param = (cypher_parameter*)unwind->expr;
        if (!executor->params_json) {
            set_result_error(result, "UNWIND $param requires parameters");
            free(maps); g_foreach_ctx = prev_ctx; free_foreach_context(ctx); return -1;
        }
        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT value FROM json_each(json_extract(?, '$.%s'))", param->name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            set_result_error(result, "Failed to prepare UNWIND parameter query");
            free(maps); g_foreach_ctx = prev_ctx; free_foreach_context(ctx); return -1;
        }
        sqlite3_bind_text(stmt, 1, executor->params_json, -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int ct = sqlite3_column_type(stmt, 0);
            if (ct == SQLITE_TEXT) {
                set_foreach_binding_string(ctx, unwind->alias,
                    (const char*)sqlite3_column_text(stmt, 0));
            } else if (ct == SQLITE_INTEGER) {
                set_foreach_binding_int(ctx, unwind->alias, sqlite3_column_int64(stmt, 0));
            } else { continue; }
            UMR_RUN_BODY();
            if (err_out) break;
        }
        sqlite3_finalize(stmt);
    } else {
        set_result_error(result, "UNWIND+MERGE+RETURN requires a list literal or parameter");
        free(maps); g_foreach_ctx = prev_ctx; free_foreach_context(ctx); return -1;
    }
    #undef UMR_RUN_BODY
    if (err_out) {
        for (int j = 0; j < n_maps; j++) free_variable_map(maps[j]);
        free(maps);
        g_foreach_ctx = prev_ctx;
        free_foreach_context(ctx);
        return -1;
    }
    if (n_maps == 0) {
        result->row_count = 0;
        result->data = NULL;
        result->data_types = NULL;
    }

    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);

    int64_t limit_val = -1, skip_val = 0;
    if (ret->limit && ret->limit->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal*)ret->limit;
        if (l->literal_type == LITERAL_INTEGER) limit_val = l->value.integer;
    }
    if (ret->skip && ret->skip->type == AST_NODE_LITERAL) {
        cypher_literal *l = (cypher_literal*)ret->skip;
        if (l->literal_type == LITERAL_INTEGER) skip_val = l->value.integer;
    }
    int start = 0;
    if (skip_val > 0) start = (skip_val >= n_maps) ? n_maps : (int)skip_val;
    int end = n_maps;
    if (limit_val == 0) end = start;
    else if (limit_val > 0 && start + (int)limit_val < end) end = start + (int)limit_val;
    int produced = end - start;
    if (produced < 0) produced = 0;

    bool agg = return_has_aggregation(ret);
    if (agg) {
        result->row_count = 1;
        result->data = malloc(sizeof(char**));
        result->data_types = malloc(sizeof(int*));
        result->data[0] = malloc(col_count * sizeof(char*));
        result->data_types[0] = calloc(col_count, sizeof(int));
        for (int i = 0; i < col_count; i++) {
            cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
            if (aggregating_call_name(it->expr)) {
                project_aggregate_cell(executor, it, maps + start, produced, result, i);
            } else if (produced > 0) {
                char **tmp = malloc(col_count * sizeof(char*));
                int *tmp_t = calloc(col_count, sizeof(int));
                char ***sd = result->data; int **st = result->data_types;
                result->data = &tmp; result->data_types = &tmp_t;
                project_return_row_from_var_map(executor, ret, maps[start], result, 0);
                result->data = sd; result->data_types = st;
                result->data[0][i] = tmp[i];
                result->data_types[0][i] = tmp_t[i];
                for (int k = 0; k < col_count; k++) if (k != i && tmp[k]) free(tmp[k]);
                free(tmp); free(tmp_t);
            } else {
                result->data[0][i] = NULL;
            }
        }
    } else {
        result->row_count = produced;
        if (produced == 0) { result->data = NULL; result->data_types = NULL; }
        else {
            result->data = malloc(produced * sizeof(char**));
            result->data_types = malloc(produced * sizeof(int*));
            for (int r = 0; r < produced; r++) {
                result->data[r] = malloc(col_count * sizeof(char*));
                result->data_types[r] = calloc(col_count, sizeof(int));
                project_return_row_from_var_map(executor, ret, maps[start + r], result, r);
            }
        }
    }

    for (int j = 0; j < n_maps; j++) free_variable_map(maps[j]);
    free(maps);
    result->success = true;
    return 0;
}

/*
 * MERGE+RETURN — execute MERGE, then project a single result row from
 * the resulting var_map. Handles non-aggregating and basic aggregating
 * RETURN items.
 */
static int handle_merge_return(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_merge *merge = find_merge_clause(query);
    cypher_return *ret = find_return_clause(query);
    if (!merge || !ret || !ret->items) {
        set_result_error(result, "MERGE+RETURN: missing clause");
        return -1;
    }

    CYPHER_DEBUG("Executing MERGE+RETURN via pattern dispatch");

    /* GQLITE-T-0371: run EVERY MERGE clause in document order, chaining the
     * bindings (`MERGE (a) MERGE (b) MERGE p = (a)-[:R]->(b) RETURN p`); only
     * the first MERGE used to run. */
    variable_map *vm = NULL;
    for (int ci = 0; query->clauses && ci < query->clauses->count; ci++) {
        ast_node *c = query->clauses->items[ci];
        if (!c || c->type != AST_NODE_MERGE) continue;
        variable_map *step = NULL;
        if (execute_merge_clause(executor, (cypher_merge*)c, result, vm, &step) < 0) {
            if (vm) free_variable_map(vm);
            if (step) free_variable_map(step);
            return -1;
        }
        if (step) {
            if (!vm) { vm = step; }
            else {
                for (int mi = 0; mi < step->count; mi++) {
                    variable_mapping *m = &step->mappings[mi];
                    if (m->type == VAR_MAP_TYPE_NODE) set_variable_node_id(vm, m->variable, m->entity_id);
                    else set_variable_edge_id(vm, m->variable, m->entity_id);
                }
                free_variable_map(step);
            }
        }
    }

    /* GQLITE-T-0371: `MERGE p = (..) RETURN p`, nested calls etc. go through
     * the id-constrained re-match; aggregates keep the per-cell projection. */
    if (vm && !return_has_aggregation(ret) && return_needs_bound_rematch(ret, vm)) {
        int rc = execute_return_via_bound_rematch(executor, query, vm, ret, result);
        free_variable_map(vm);
        if (rc >= 0) result->success = true;
        return rc;
    }

    set_return_column_names(ret, result);
    int col_count = ret->items->count;

    if (!vm) {
        result->row_count = 0;
        result->data = NULL;
        result->data_types = NULL;
        result->success = true;
        return 0;
    }

    bool agg = return_has_aggregation(ret);
    if (agg) {
        result->row_count = 1;
        result->data = malloc(sizeof(char**));
        result->data_types = malloc(sizeof(int*));
        result->data[0] = malloc(col_count * sizeof(char*));
        result->data_types[0] = calloc(col_count, sizeof(int));
        variable_map *maps[1] = { vm };
        for (int i = 0; i < col_count; i++) {
            cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
            if (aggregating_call_name(it->expr)) {
                project_aggregate_cell(executor, it, maps, 1, result, i);
            } else {
                /* Project single row cell using helper */
                char **tmp = malloc(col_count * sizeof(char*));
                int *tmp_t = calloc(col_count, sizeof(int));
                char ***sd = result->data; int **st = result->data_types;
                result->data = &tmp; result->data_types = &tmp_t;
                project_return_row_from_var_map(executor, ret, vm, result, 0);
                result->data = sd; result->data_types = st;
                result->data[0][i] = tmp[i];
                result->data_types[0][i] = tmp_t[i];
                for (int k = 0; k < col_count; k++) if (k != i && tmp[k]) free(tmp[k]);
                free(tmp); free(tmp_t);
            }
        }
    } else {
        result->row_count = 1;
        result->data = malloc(sizeof(char**));
        result->data_types = malloc(sizeof(int*));
        result->data[0] = malloc(col_count * sizeof(char*));
        result->data_types[0] = calloc(col_count, sizeof(int));
        project_return_row_from_var_map(executor, ret, vm, result, 0);
    }

    free_variable_map(vm);
    result->success = true;
    return 0;
}

/* ======================================================================
 * GQLITE-T-0371: row-wise write pipeline
 * ====================================================================== */

typedef struct {
    char *name;
    char *text;     /* SQL text of the value (NULL for SQL NULL) */
    int type;       /* SQLITE_INTEGER / FLOAT / TEXT / NULL */
} rw_scalar;

typedef struct {
    variable_map *vm;
    rw_scalar *scalars;
    int n_scalars;
} rw_row;

static void rw_row_free(rw_row *r)
{
    if (!r) return;
    if (r->vm) free_variable_map(r->vm);
    for (int i = 0; i < r->n_scalars; i++) { free(r->scalars[i].name); free(r->scalars[i].text); }
    free(r->scalars);
    r->vm = NULL; r->scalars = NULL; r->n_scalars = 0;
}

static void rw_row_add_scalar(rw_row *r, const char *name, const char *text, int type)
{
    rw_scalar *grown = realloc(r->scalars, (size_t)(r->n_scalars + 1) * sizeof(rw_scalar));
    if (!grown) return;
    r->scalars = grown;
    r->scalars[r->n_scalars].name = strdup(name);
    r->scalars[r->n_scalars].text = text ? strdup(text) : NULL;
    r->scalars[r->n_scalars].type = type;
    r->n_scalars++;
}

static const rw_scalar *rw_row_scalar(const rw_row *r, const char *name)
{
    for (int i = 0; i < r->n_scalars; i++)
        if (strcmp(r->scalars[i].name, name) == 0) return &r->scalars[i];
    return NULL;
}

static bool rw_is_write_clause(ast_node_type t)
{
    return t == AST_NODE_CREATE || t == AST_NODE_MERGE || t == AST_NODE_DELETE ||
           t == AST_NODE_SET || t == AST_NODE_REMOVE;
}

/* Variable names bound at the end of a read prefix. */
static void rw_collect_pattern_vars(ast_list *pattern, char ***names, int *n)
{
    if (!pattern) return;
    for (int i = 0; i < pattern->count; i++) {
        ast_node *item = pattern->items[i];
        if (!item) continue;
        ast_list *els = NULL;
        if (item->type == AST_NODE_PATH) {
            cypher_path *path = (cypher_path*)item;
            if (path->var_name) { *names = realloc(*names, (size_t)(*n + 1) * sizeof(char*)); (*names)[(*n)++] = strdup(path->var_name); }
            els = path->elements;
        } else if (item->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *np = (cypher_node_pattern*)item;
            if (np->variable) { *names = realloc(*names, (size_t)(*n + 1) * sizeof(char*)); (*names)[(*n)++] = strdup(np->variable); }
            continue;
        }
        if (!els) continue;
        for (int j = 0; j < els->count; j++) {
            ast_node *el = els->items[j];
            const char *v = NULL;
            if (el->type == AST_NODE_NODE_PATTERN) v = ((cypher_node_pattern*)el)->variable;
            else if (el->type == AST_NODE_REL_PATTERN) v = ((cypher_rel_pattern*)el)->variable;
            if (v && strncmp(v, "_gql_", 5) != 0) {
                bool dup = false;
                for (int k = 0; k < *n; k++) if (strcmp((*names)[k], v) == 0) { dup = true; break; }
                if (!dup) { *names = realloc(*names, (size_t)(*n + 1) * sizeof(char*)); (*names)[(*n)++] = strdup(v); }
            }
        }
    }
}

static void rw_free_names(char **names, int n) { for (int i = 0; i < n; i++) free(names[i]); free(names); }

static int rw_scope_after_prefix(ast_list *clauses, int prefix_len, char ***out_names)
{
    char **names = NULL; int n = 0;
    for (int i = 0; i < prefix_len; i++) {
        ast_node *c = clauses->items[i];
        if (!c) continue;
        if (c->type == AST_NODE_MATCH) {
            rw_collect_pattern_vars(((cypher_match*)c)->pattern, &names, &n);
        } else if (c->type == AST_NODE_UNWIND) {
            cypher_unwind *u = (cypher_unwind*)c;
            if (u->alias) { names = realloc(names, (size_t)(n + 1) * sizeof(char*)); names[n++] = strdup(u->alias); }
        } else if (c->type == AST_NODE_WITH) {
            cypher_with *w = (cypher_with*)c;
            if (w->pass_all) continue;
            char **nn = NULL; int m = 0;
            if (w->items) {
                for (int wi = 0; wi < w->items->count; wi++) {
                    cypher_return_item *it = (cypher_return_item*)w->items->items[wi];
                    const char *nm = it->alias ? it->alias :
                        (it->expr && it->expr->type == AST_NODE_IDENTIFIER ? ((cypher_identifier*)it->expr)->name : NULL);
                    if (nm) { nn = realloc(nn, (size_t)(m + 1) * sizeof(char*)); nn[m++] = strdup(nm); }
                }
            }
            rw_free_names(names, n);
            names = nn; n = m;
        }
    }
    *out_names = names;
    return n;
}

/* SQL literal text for a scalar, for splicing into an expression. Owned. */
static char *rw_scalar_sql(const rw_scalar *sc)
{
    if (!sc || !sc->text) return strdup("NULL");
    if (sc->type == SQLITE_INTEGER || sc->type == SQLITE_FLOAT) return strdup(sc->text);
    const char *t = sc->text;
    if ((t[0] == '[' || t[0] == '{')) return sqlite3_mprintf("json(%Q)", t);
    return sqlite3_mprintf("%Q", t);
}

/* Does `expr` reference only scalar bindings of `row` (and literals)? */
static bool rw_expr_scalar_only(ast_node *expr, const rw_row *row)
{
    if (!expr) return true;
    switch (expr->type) {
        case AST_NODE_LITERAL: return true;
        case AST_NODE_PARAMETER: return true;
        case AST_NODE_IDENTIFIER: return rw_row_scalar(row, ((cypher_identifier*)expr)->name) != NULL;
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *b = (cypher_binary_op*)expr;
            return rw_expr_scalar_only(b->left, row) && rw_expr_scalar_only(b->right, row);
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *f = (cypher_function_call*)expr;
            if (aggregating_call_name(expr)) return false;
            if (!f->args) return true;
            for (int i = 0; i < f->args->count; i++) if (!rw_expr_scalar_only(f->args->items[i], row)) return false;
            return true;
        }
        case AST_NODE_LIST: {
            cypher_list *l = (cypher_list*)expr;
            if (!l->items) return true;
            for (int i = 0; i < l->items->count; i++) if (!rw_expr_scalar_only(l->items->items[i], row)) return false;
            return true;
        }
        default: return false;
    }
}

/* Evaluate a scalar-only expression for a row. Returns the SQLite type;
 * *out_text is owned (NULL for SQL NULL). Returns -1 on failure. */
static int rw_eval_scalar(cypher_executor *executor, ast_node *expr, const rw_row *row, char **out_text)
{
    *out_text = NULL;
    cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
    if (!ctx) return -1;
    for (int i = 0; i < row->n_scalars; i++) {
        char *lit = rw_scalar_sql(&row->scalars[i]);
        transform_var_register_projected(ctx->var_ctx, row->scalars[i].name, lit ? lit : "NULL");
        transform_var_set_bound(ctx->var_ctx, row->scalars[i].name, true);
        transform_var_set_scalar_value(ctx->var_ctx, row->scalars[i].name, true);
        sqlite3_free(lit);
    }
    char *sql_expr = cypher_transform_capture_expression(ctx, expr);
    cypher_transform_free_context(ctx);
    if (!sql_expr) return -1;
    char *sql = sqlite3_mprintf("SELECT %s", sql_expr);
    free(sql_expr);
    sqlite3_stmt *st = NULL;
    int type = -1;
    if (sql && sqlite3_prepare_v2(executor->db, sql, -1, &st, NULL) == SQLITE_OK) {
        if (executor->params_json) bind_params_from_json(st, executor->params_json);
        if (sqlite3_step(st) == SQLITE_ROW) {
            type = sqlite3_column_type(st, 0);
            const char *v = (const char*)sqlite3_column_text(st, 0);
            *out_text = v ? strdup(v) : NULL;
        }
        sqlite3_finalize(st);
    }
    sqlite3_free(sql);
    return type;
}

/* Literal AST for a scalar value (JSON arrays become list literals). */
static ast_node *rw_make_literal(cypher_executor *executor, const char *text, int type)
{
    if (!text || type == SQLITE_NULL) return (ast_node*)make_null_literal(0);
    if (type == SQLITE_INTEGER) return (ast_node*)make_integer_literal(strtoll(text, NULL, 10), 0);
    if (type == SQLITE_FLOAT) return (ast_node*)make_decimal_literal(strtod(text, NULL), 0);
    if (text[0] == '[') {
        ast_list *items = ast_list_create();
        sqlite3_stmt *st = NULL;
        if (items && sqlite3_prepare_v2(executor->db,
                "SELECT type, value FROM json_each(?)", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, text, -1, SQLITE_TRANSIENT);
            bool ok = true;
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *jt = (const char*)sqlite3_column_text(st, 0);
                const char *jv = (const char*)sqlite3_column_text(st, 1);
                ast_node *lit = NULL;
                if (!jt) { ok = false; break; }
                if (strcmp(jt, "integer") == 0) lit = (ast_node*)make_integer_literal(strtoll(jv ? jv : "0", NULL, 10), 0);
                else if (strcmp(jt, "real") == 0) lit = (ast_node*)make_decimal_literal(strtod(jv ? jv : "0", NULL), 0);
                else if (strcmp(jt, "true") == 0) lit = (ast_node*)make_boolean_literal(true, 0);
                else if (strcmp(jt, "false") == 0) lit = (ast_node*)make_boolean_literal(false, 0);
                else if (strcmp(jt, "null") == 0) lit = (ast_node*)make_null_literal(0);
                else if (strcmp(jt, "text") == 0) lit = (ast_node*)make_string_literal((char*)(jv ? jv : ""), 0);
                else { ok = false; break; }   /* nested arrays/objects: fall back to text */
                ast_list_append(items, lit);
            }
            sqlite3_finalize(st);
            if (ok) return (ast_node*)make_list(items, 0);
            ast_list_free(items);
        } else if (items) {
            ast_list_free(items);
        }
    }
    return (ast_node*)make_string_literal((char*)text, 0);
}

typedef struct { cypher_map_pair *pair; ast_node *original; ast_node *substitute; } rw_subst;

static void rw_literalize_props(cypher_executor *executor, ast_node *props, const rw_row *row,
                                rw_subst **subs, int *n_subs)
{
    if (!props || props->type != AST_NODE_MAP) return;
    cypher_map *m = (cypher_map*)props;
    if (!m->pairs) return;
    for (int i = 0; i < m->pairs->count; i++) {
        cypher_map_pair *pair = (cypher_map_pair*)m->pairs->items[i];
        if (!pair || !pair->value) continue;
        if (pair->value->type == AST_NODE_LITERAL || pair->value->type == AST_NODE_PARAMETER) continue;
        if (!rw_expr_scalar_only(pair->value, row)) continue;
        char *text = NULL;
        int type = rw_eval_scalar(executor, pair->value, row, &text);
        if (type < 0) { free(text); continue; }
        ast_node *lit = rw_make_literal(executor, text, type);
        free(text);
        if (!lit) continue;
        rw_subst *grown = realloc(*subs, (size_t)(*n_subs + 1) * sizeof(rw_subst));
        if (!grown) { ast_node_free(lit); continue; }
        *subs = grown;
        (*subs)[*n_subs].pair = pair;
        (*subs)[*n_subs].original = pair->value;
        (*subs)[*n_subs].substitute = lit;
        (*n_subs)++;
        pair->value = lit;
    }
}

static void rw_literalize_pattern(cypher_executor *executor, ast_list *pattern, const rw_row *row,
                                  rw_subst **subs, int *n_subs)
{
    if (!pattern) return;
    for (int i = 0; i < pattern->count; i++) {
        ast_node *item = pattern->items[i];
        if (!item) continue;
        if (item->type == AST_NODE_NODE_PATTERN) {
            rw_literalize_props(executor, ((cypher_node_pattern*)item)->properties, row, subs, n_subs);
        } else if (item->type == AST_NODE_PATH) {
            cypher_path *path = (cypher_path*)item;
            if (!path->elements) continue;
            for (int j = 0; j < path->elements->count; j++) {
                ast_node *el = path->elements->items[j];
                if (el->type == AST_NODE_NODE_PATTERN) rw_literalize_props(executor, ((cypher_node_pattern*)el)->properties, row, subs, n_subs);
                else if (el->type == AST_NODE_REL_PATTERN) rw_literalize_props(executor, ((cypher_rel_pattern*)el)->properties, row, subs, n_subs);
            }
        }
    }
}

static void rw_restore_subs(rw_subst *subs, int n_subs)
{
    for (int i = 0; i < n_subs; i++) {
        subs[i].pair->value = subs[i].original;
        ast_node_free(subs[i].substitute);
    }
    free(subs);
}

static void rw_fold_map(variable_map *into, variable_map *from)
{
    if (!into || !from) return;
    for (int i = 0; i < from->count; i++) {
        variable_mapping *m = &from->mappings[i];
        if (m->type == VAR_MAP_TYPE_NODE) set_variable_node_id(into, m->variable, m->entity_id);
        else set_variable_edge_id(into, m->variable, m->entity_id);
    }
}

/* Apply a tail WITH to a row: rename / drop entities and scalars, evaluate
 * scalar-only expressions. */
static int rw_apply_with(cypher_executor *executor, cypher_with *w, rw_row *row)
{
    if (w->pass_all || !w->items) return 0;
    rw_row next = {0};
    next.vm = create_variable_map();
    if (!next.vm) return -1;
    for (int wi = 0; wi < w->items->count; wi++) {
        cypher_return_item *it = (cypher_return_item*)w->items->items[wi];
        if (!it || !it->expr) continue;
        if (it->expr->type == AST_NODE_IDENTIFIER) {
            const char *src = ((cypher_identifier*)it->expr)->name;
            const char *dst = it->alias ? it->alias : src;
            if (is_variable_edge(row->vm, src)) set_variable_edge_id(next.vm, dst, get_variable_edge_id(row->vm, src));
            else if (get_variable_node_id(row->vm, src) >= 0) set_variable_node_id(next.vm, dst, get_variable_node_id(row->vm, src));
            else {
                const rw_scalar *sc = rw_row_scalar(row, src);
                if (sc) rw_row_add_scalar(&next, dst, sc->text, sc->type);
            }
        } else if (it->alias && rw_expr_scalar_only(it->expr, row)) {
            char *text = NULL;
            int type = rw_eval_scalar(executor, it->expr, row, &text);
            if (type >= 0) rw_row_add_scalar(&next, it->alias, text, type);
            free(text);
        }
    }
    bool keep = true;
    if (w->where && rw_expr_scalar_only(w->where, &next)) {
        char *text = NULL;
        int type = rw_eval_scalar(executor, w->where, &next, &text);
        keep = (type >= 0 && text && strcmp(text, "1") == 0);
        free(text);
    }
    rw_row_free(row);
    *row = next;
    return keep ? 0 : 1;   /* 1 = row filtered out */
}

static int handle_rowwise_write(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    ast_list *clauses = query->clauses;
    if (!clauses || clauses->count == 0) return -1;

    /* Shape check: the first clause must be MATCH, UNWIND or CREATE. Anything
     * else goes to the handlers that owned these flag sets before. */
    ast_node *first = clauses->items[0];
    if (!first || !(first->type == AST_NODE_MATCH || first->type == AST_NODE_UNWIND ||
                    first->type == AST_NODE_CREATE)) {
        if (flags & CLAUSE_WITH) return handle_merge_with_pipeline(executor, query, result, flags);
        return handle_merge_return(executor, query, result, flags);
    }

    CYPHER_DEBUG("Executing row-wise write pipeline");

    /* --- 1. Split prefix / tail ------------------------------------------ */
    int start = 0;
    variable_map *created = NULL;
    cypher_match *synth_create_match = NULL;
    ast_node *synth_where = NULL;
    if (first->type == AST_NODE_CREATE) {
        /* Leading CREATE: run it once, then read its bindings back through a
         * synthetic id-constrained MATCH so the rest of the prefix (WITH /
         * UNWIND) sees the created entities. */
        if (execute_create_clause_with_varmap(executor, (cypher_create*)first, result, &created) < 0) {
            if (created) free_variable_map(created);
            return -1;
        }
        if (created) {
            for (int i = 0; i < created->count; i++) {
                variable_mapping *m = &created->mappings[i];
                ast_list *args = ast_list_create();
                ast_list_append(args, (ast_node*)make_identifier(m->variable, 0));
                ast_node *fn = (ast_node*)make_function_call("id", args, false, 0);
                ast_node *eq = (ast_node*)make_binary_op(BINARY_OP_EQ, fn, (ast_node*)make_integer_literal(m->entity_id, 0), 0);
                synth_where = synth_where ? (ast_node*)make_binary_op(BINARY_OP_AND, synth_where, eq, 0) : eq;
            }
        }
        synth_create_match = make_cypher_match(((cypher_create*)first)->pattern, synth_where, false, NULL);
        start = 1;
    }
    int prefix_end = start;
    while (prefix_end < clauses->count) {
        ast_node *c = clauses->items[prefix_end];
        if (!c || rw_is_write_clause(c->type) || c->type == AST_NODE_RETURN) break;
        prefix_end++;
    }

    /* --- 2. Evaluate the prefix once: SELECT every in-scope variable ------ */
    char **scope = NULL;
    int n_scope;
    {
        ast_list *tmp = ast_list_create();
        if (synth_create_match) ast_list_append(tmp, (ast_node*)synth_create_match);
        for (int i = start; i < prefix_end; i++) ast_list_append(tmp, clauses->items[i]);
        n_scope = rw_scope_after_prefix(tmp, tmp->count, &scope);
        free(tmp->items); free(tmp);
    }
    rw_row *rows = NULL; int n_rows = 0;
    bool *scope_is_entity = NULL;
    if (n_scope > 0 || prefix_end > start || synth_create_match) {
        ast_list *items = ast_list_create();
        for (int i = 0; i < n_scope; i++)
            ast_list_append(items, (ast_node*)make_return_item((ast_node*)make_identifier(scope[i], 0), strdup(scope[i])));
        if (n_scope == 0)
            ast_list_append(items, (ast_node*)make_return_item((ast_node*)make_integer_literal(1, 0), strdup("_one")));
        cypher_return *ret = make_cypher_return(false, items, NULL, NULL, NULL);
        ast_list *pre = ast_list_create();
        if (synth_create_match) ast_list_append(pre, (ast_node*)synth_create_match);
        for (int i = start; i < prefix_end; i++) ast_list_append(pre, clauses->items[i]);
        ast_list_append(pre, (ast_node*)ret);
        cypher_query *pq = make_cypher_query(pre, false);

        /* Run the prefix through this executor (its schema and params are
         * live); the read dispatch creates its own transform contexts. */
        cypher_result *pr = cypher_executor_execute_ast(executor, (ast_node*)pq);
        int rc_prefix = 0;
        if (!pr || !pr->success) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Failed to evaluate the read prefix of a write pipeline: %s",
                     pr && pr->error_message ? pr->error_message : "no result");
            set_result_error(result, msg);
            rc_prefix = -1;
        } else {
            scope_is_entity = calloc((size_t)(n_scope > 0 ? n_scope : 1), sizeof(bool));
            rows = calloc((size_t)(pr->row_count > 0 ? pr->row_count : 1), sizeof(rw_row));
            for (int r = 0; r < pr->row_count && rows; r++) {
                rw_row *row = &rows[n_rows];
                row->vm = create_variable_map();
                for (int c = 0; c < n_scope && c < pr->column_count; c++) {
                    const char *cell = pr->data[r][c];
                    int ty = pr->data_types ? pr->data_types[r][c] : SQLITE_TEXT;
                    if (cell && cell[0] == '{' && strstr(cell, "\"id\"")) {
                        /* entity object: {"id":N,...}; edges carry "startNode" */
                        const char *idp = strstr(cell, "\"id\":");
                        long long id = idp ? strtoll(idp + 5, NULL, 10) : -1;
                        if (id >= 0) {
                            if (strstr(cell, "\"startNode\"")) set_variable_edge_id(row->vm, scope[c], (int)id);
                            else set_variable_node_id(row->vm, scope[c], (int)id);
                            scope_is_entity[c] = true;
                            continue;
                        }
                    }
                    rw_row_add_scalar(row, scope[c], cell, cell ? ty : SQLITE_NULL);
                }
                n_rows++;
            }
        }
        if (pr) cypher_result_free(pr);
        /* The clauses are shared with `query`; free only what we made. */
        ast_node_free((ast_node*)ret);
        free(pre->items); free(pre); free(pq);
        if (rc_prefix < 0) {
            rw_free_names(scope, n_scope);
            if (synth_create_match) { ast_node_free(synth_where); free(synth_create_match); }
            if (created) free_variable_map(created);
            free(rows); free(scope_is_entity);
            return -1;
        }
    }
    if (synth_create_match) { ast_node_free(synth_where); free(synth_create_match); }
    if (created) free_variable_map(created);
    rw_free_names(scope, n_scope);
    free(scope_is_entity);

    cypher_return *ret_clause = find_return_clause(query);
    int rc = 0;

    /* --- 3. Eager DELETE pass: every DELETE that precedes the first MERGE
     *        runs for all rows before any MERGE runs for any row. --------- */
    int first_merge = clauses->count;
    for (int i = prefix_end; i < clauses->count; i++)
        if (clauses->items[i] && clauses->items[i]->type == AST_NODE_MERGE) { first_merge = i; break; }
    bool *eager_done = calloc((size_t)clauses->count, sizeof(bool));
    for (int i = prefix_end; i < first_merge; i++) {
        ast_node *c = clauses->items[i];
        if (!c || c->type != AST_NODE_DELETE) continue;
        for (int r = 0; r < n_rows && rc >= 0; r++) {
            rc = execute_delete_operations(executor, (cypher_delete*)c, rows[r].vm, result);
        }
        if (eager_done) eager_done[i] = true;
        if (rc < 0) break;
    }

    /* --- 4. Per-row tail --------------------------------------------------- */
    bool *row_alive = calloc((size_t)(n_rows > 0 ? n_rows : 1), sizeof(bool));
    for (int r = 0; r < n_rows && rc >= 0; r++) {
        row_alive[r] = true;
        for (int i = prefix_end; i < clauses->count && rc >= 0 && row_alive[r]; i++) {
            ast_node *c = clauses->items[i];
            if (!c) continue;
            switch (c->type) {
                case AST_NODE_DELETE:
                    if (eager_done && eager_done[i]) break;
                    rc = execute_delete_operations(executor, (cypher_delete*)c, rows[r].vm, result);
                    break;
                case AST_NODE_MERGE: {
                    cypher_merge *mg = (cypher_merge*)c;
                    rw_subst *subs = NULL; int n_subs = 0;
                    rw_literalize_pattern(executor, mg->pattern, &rows[r], &subs, &n_subs);
                    variable_map *out = NULL;
                    rc = execute_merge_clause(executor, mg, result, rows[r].vm, &out);
                    rw_restore_subs(subs, n_subs);
                    if (out) { rw_fold_map(rows[r].vm, out); free_variable_map(out); }
                    break;
                }
                case AST_NODE_SET:
                    rc = execute_set_operations(executor, (cypher_set*)c, rows[r].vm, result);
                    break;
                case AST_NODE_REMOVE:
                    rc = execute_remove_operations(executor, (cypher_remove*)c, rows[r].vm, result);
                    break;
                case AST_NODE_CREATE: {
                    cypher_create *cr = (cypher_create*)c;
                    rw_subst *subs = NULL; int n_subs = 0;
                    rw_literalize_pattern(executor, cr->pattern, &rows[r], &subs, &n_subs);
                    /* Entities bound by the row (e.g. CREATE (a)-[:R]->(b) after
                     * MATCH) must be visible to CREATE: start from a copy. */
                    variable_map *cm = create_variable_map();
                    if (cm) rw_fold_map(cm, rows[r].vm);
                    rc = execute_create_clause_with_varmap(executor, cr, result, &cm);
                    rw_restore_subs(subs, n_subs);
                    if (cm) { rw_fold_map(rows[r].vm, cm); free_variable_map(cm); }
                    break;
                }
                case AST_NODE_WITH: {
                    int w = rw_apply_with(executor, (cypher_with*)c, &rows[r]);
                    if (w < 0) rc = -1;
                    else if (w == 1) row_alive[r] = false;
                    break;
                }
                case AST_NODE_RETURN:
                    break;
                default:
                    set_result_error(result, "Unsupported clause in a row-wise write pipeline");
                    rc = -1;
            }
        }
    }
    free(eager_done);

    /* --- 5. RETURN --------------------------------------------------------- */
    if (rc >= 0) {
        result->success = true;
        if (ret_clause && ret_clause->items) {
            int live = 0;
            for (int r = 0; r < n_rows; r++) if (row_alive[r]) live++;
            set_return_column_names(ret_clause, result);
            int cols = ret_clause->items->count;
            bool agg = return_has_aggregation(ret_clause);
            int out_rows = agg ? 1 : live;
            result->row_count = out_rows;
            result->data = calloc((size_t)(out_rows > 0 ? out_rows : 1), sizeof(char**));
            result->data_types = calloc((size_t)(out_rows > 0 ? out_rows : 1), sizeof(int*));
            for (int o = 0; o < out_rows; o++) {
                result->data[o] = calloc((size_t)cols, sizeof(char*));
                result->data_types[o] = calloc((size_t)cols, sizeof(int));
            }
            if (agg) {
                variable_map **maps = calloc((size_t)(live > 0 ? live : 1), sizeof(variable_map*));
                int k = 0;
                for (int r = 0; r < n_rows; r++) if (row_alive[r]) maps[k++] = rows[r].vm;
                /* non-aggregate cells come from the first live row */
                int first_live = -1;
                for (int r = 0; r < n_rows; r++) if (row_alive[r]) { first_live = r; break; }
                if (first_live >= 0) project_return_row_from_var_map(executor, ret_clause, rows[first_live].vm, result, 0);
                for (int i = 0; i < cols; i++) {
                    cypher_return_item *it = (cypher_return_item*)ret_clause->items->items[i];
                    if (aggregating_call_name(it->expr)) {
                        free(result->data[0][i]); result->data[0][i] = NULL;
                        project_aggregate_cell(executor, it, maps, live, result, i);
                    } else if (first_live >= 0 && rw_expr_scalar_only(it->expr, &rows[first_live])) {
                        char *text = NULL;
                        int type = rw_eval_scalar(executor, it->expr, &rows[first_live], &text);
                        free(result->data[0][i]);
                        result->data[0][i] = text;
                        result->data_types[0][i] = type >= 0 ? type : SQLITE_NULL;
                    }
                }
                free(maps);
            } else {
                int o = 0;
                for (int r = 0; r < n_rows; r++) {
                    if (!row_alive[r]) continue;
                    project_return_row_from_var_map(executor, ret_clause, rows[r].vm, result, o);
                    for (int i = 0; i < cols; i++) {
                        cypher_return_item *it = (cypher_return_item*)ret_clause->items->items[i];
                        if (it->expr && it->expr->type != AST_NODE_LITERAL && rw_expr_scalar_only(it->expr, &rows[r])) {
                            char *text = NULL;
                            int type = rw_eval_scalar(executor, it->expr, &rows[r], &text);
                            free(result->data[o][i]);
                            result->data[o][i] = text;
                            result->data_types[o][i] = type >= 0 ? type : SQLITE_NULL;
                        }
                    }
                    o++;
                }
            }
        }
    }

    for (int r = 0; r < n_rows; r++) rw_row_free(&rows[r]);
    free(rows);
    free(row_alive);
    return rc;
}

