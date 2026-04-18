/*
 * query_dispatch.c
 *    Table-driven query pattern dispatch for Cypher execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/query_patterns.h"
#include "executor/executor_internal.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"

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
static int handle_generic_transform(cypher_executor *executor, cypher_query *query,
                                    cypher_result *result, clause_flags flags);
static int handle_match_set(cypher_executor *executor, cypher_query *query,
                            cypher_result *result, clause_flags flags);
static int handle_match_delete(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_match_remove(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);
static int handle_match_merge(cypher_executor *executor, cypher_query *query,
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
static int handle_merge_with_pipeline(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags);
static int handle_call_subquery(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags);
static int handle_create_return(cypher_executor *executor, cypher_query *query,
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
        .name = "UNWIND+CREATE",
        .required = CLAUSE_UNWIND | CLAUSE_CREATE,
        .forbidden = CLAUSE_RETURN | CLAUSE_MATCH,
        .handler = handle_unwind_create,
        .priority = 100
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
        .forbidden = CLAUSE_NONE,
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
    static char buffer[256];
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

    /* Execute the pattern handler */
    return pattern->handler(executor, query, result, flags);
}

/*
 * Generic transform handler - uses full transform pipeline
 * This is the fallback for queries without a specialized handler
 */
static int handle_generic_transform(cypher_executor *executor, cypher_query *query,
                                    cypher_result *result, clause_flags flags)
{
    (void)flags;  /* Unused in generic handler */

    CYPHER_DEBUG("Using generic transform pipeline");

    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
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
    cypher_free_result(transform_result);
    cypher_transform_free_context(ctx);
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
                rc = execute_match_return_query(executor, match, ret, result);
            }
        }
    }
    return rc;
}

/* Check if a RETURN clause contains only COUNT aggregates and synthesize
 * the result from delete counts instead of re-querying the empty graph. */
static bool synthesize_delete_return(cypher_return *ret, cypher_result *result)
{
    if (!ret || !ret->items || ret->items->count == 0) return false;

    int total_deleted = result->nodes_deleted + result->relationships_deleted;
    if (total_deleted == 0) return false;

    /* Check that every RETURN item is a COUNT function call */
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        if (!item || !item->expr || item->expr->type != AST_NODE_FUNCTION_CALL) return false;
        cypher_function_call *func = (cypher_function_call*)item->expr;
        if (!func->function_name || strcasecmp(func->function_name, "count") != 0) return false;
    }

    /* All items are COUNT — synthesize a single-row result */
    result->column_count = ret->items->count;
    result->column_names = malloc(result->column_count * sizeof(char*));
    result->data = malloc(sizeof(char**));
    result->data[0] = calloc(result->column_count, sizeof(char*));
    result->data_types = malloc(sizeof(int*));
    result->data_types[0] = calloc(result->column_count, sizeof(int));
    result->row_count = 1;

    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *item = (cypher_return_item*)ret->items->items[i];
        /* Use alias if provided, otherwise generate "count" */
        if (item->alias) {
            result->column_names[i] = strdup(item->alias);
        } else {
            result->column_names[i] = strdup("count");
        }

        /* All COUNT columns get the total delete count */
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", total_deleted);
        result->data[0][i] = strdup(buf);
        result->data_types[0][i] = SQLITE_INTEGER;
    }

    result->success = true;
    return true;
}

static int handle_match_delete(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    cypher_match *match = find_match_clause(query);
    cypher_delete *del = find_delete_clause(query);

    CYPHER_DEBUG("Executing MATCH+DELETE via pattern dispatch");
    int rc = execute_match_delete_query(executor, match, del, result);
    if (rc >= 0) {
        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret) {
                /* Try to synthesize COUNT results from delete counts
                 * instead of re-querying the now-empty graph */
                if (!synthesize_delete_return(ret, result)) {
                    rc = execute_match_return_query(executor, match, ret, result);
                }
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
    int rc = execute_match_remove_query(executor, match, remove, result);
    if (rc >= 0) {
        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret) {
                rc = execute_match_return_query(executor, match, ret, result);
            }
        }
    }
    return rc;
}

static int handle_match_merge(cypher_executor *executor, cypher_query *query,
                              cypher_result *result, clause_flags flags)
{
    cypher_match *match = find_match_clause(query);
    cypher_merge *merge = find_merge_clause(query);
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing MATCH+MERGE via pattern dispatch");

    /* Capture the MATCH+MERGE var_map when a trailing SET needs it. */
    variable_map *mm_vars = NULL;
    int rc = execute_match_merge_query_with_varmap(executor, match, merge, result, set ? &mm_vars : NULL);
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
            rc = execute_match_return_query(executor, match, ret, result);
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
    int rc = execute_multi_match_create_query_with_varmap(executor, query, create, result,
                                                          set ? &mc_vars : NULL);
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

static int handle_match_create_return(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_match *match = find_match_clause(query);
    cypher_create *create = find_create_clause(query);
    cypher_return *ret = find_return_clause(query);

    CYPHER_DEBUG("Executing MATCH+CREATE+RETURN via pattern dispatch");
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
    cypher_create *create = find_create_clause(query);
    cypher_set *set = find_set_clause(query);

    CYPHER_DEBUG("Executing CREATE via pattern dispatch");

    /* If no trailing SET, keep the fast path. */
    if (!set) {
        int rc = execute_create_clause(executor, create, result);
        if (rc >= 0) result->success = true;
        return rc;
    }

    /* CREATE + SET: execute CREATE via the varmap variant, thread bindings
     * into execute_set_operations. Keeps the dispatch table flat instead of
     * adding a dedicated CREATE+SET entry. */
    variable_map *create_vars = NULL;
    int rc = execute_create_clause_with_varmap(executor, create, result, &create_vars);
    if (rc < 0) {
        if (create_vars) free_variable_map(create_vars);
        return rc;
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
        int rc = execute_merge_clause(executor, merge, result);
        if (rc >= 0) result->success = true;
        return rc;
    }

    /* MERGE + trailing SET: thread MERGE's var_map into execute_set_operations.
     * ON CREATE / ON MATCH SET already run inside execute_merge_clause. */
    variable_map *merge_vars = NULL;
    int rc = execute_merge_clause_with_varmap(executor, merge, result, &merge_vars);
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

/*
 * Execute a RETURN clause using known node IDs from a variable_map.
 * Sets up a transform context with synthetic FROM/WHERE targeting
 * the specific node IDs, then uses the standard transform pipeline
 * to generate SELECT projections and build results.
 */
static int merge_with_execute_return(cypher_executor *executor,
                                     cypher_return *ret,
                                     variable_map *var_map,
                                     cypher_result *result)
{
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "MERGE+WITH RETURN: failed to create transform context");
        return -1;
    }

    ctx->query_type = QUERY_TYPE_READ;

    /* Register each variable from var_map in the transform context
     * and set up FROM/WHERE to select by known entity IDs */
    bool first_var = true;
    for (int i = 0; i < var_map->count; i++) {
        variable_mapping *m = &var_map->mappings[i];

        char alias[64];
        snprintf(alias, sizeof(alias), "_gql_var_%d", i);
        char where_cond[128];
        snprintf(where_cond, sizeof(where_cond), "%s.id = %d", alias, m->entity_id);

        const char *table = NULL;
        if (m->type == VAR_MAP_TYPE_NODE) {
            transform_var_register_node(ctx->var_ctx, m->variable, alias, NULL);
            table = get_graph_table(ctx, "nodes");
        } else if (m->type == VAR_MAP_TYPE_EDGE) {
            transform_var_register_edge(ctx->var_ctx, m->variable, alias, NULL);
            table = get_graph_table(ctx, "edges");
        } else {
            continue;
        }
        transform_var_set_bound(ctx->var_ctx, m->variable, true);

        if (first_var) {
            sql_from(ctx->unified_builder, table, alias);
            first_var = false;
        } else {
            sql_join(ctx->unified_builder, SQL_JOIN_CROSS, table, alias, NULL);
        }
        sql_where(ctx->unified_builder, where_cond);
    }

    if (first_var) {
        cypher_transform_free_context(ctx);
        set_result_error(result, "MERGE+WITH RETURN: no node or edge variables to return");
        return -1;
    }

    /* Transform the RETURN clause (generates SELECT, calls finalize_sql_generation) */
    if (transform_return_clause(ctx, ret) < 0) {
        const char *msg = ctx->error_message ? ctx->error_message
                                             : "MERGE+WITH RETURN: failed to transform RETURN clause";
        set_result_error(result, msg);
        cypher_transform_free_context(ctx);
        return -1;
    }

    prepend_cte_to_sql(ctx);
    CYPHER_DEBUG("MERGE+WITH RETURN SQL: %s", ctx->sql_buffer);

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error[512];
        snprintf(error, sizeof(error), "MERGE+WITH RETURN: SQL prepare failed: %s",
                 sqlite3_errmsg(executor->db));
        set_result_error(result, error);
        cypher_transform_free_context(ctx);
        return -1;
    }

    if (executor->params_json) {
        if (bind_params_from_json(stmt, executor->params_json) < 0) {
            set_result_error(result, "MERGE+WITH RETURN: failed to bind parameters");
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }
    }

    if (build_query_results(executor, stmt, ret, result, ctx) < 0) {
        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);
        return -1;
    }

    result->success = true;
    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);
    return 0;
}

/* Pipeline handler for MERGE + WITH + subsequent clauses.
 * Splits execution at the WITH boundary:
 * 1. Execute pre-WITH clauses (MERGE + SET) as a standalone MERGE
 * 2. Re-resolve MERGE'd node IDs by pattern
 * 3. Dispatch post-WITH clauses as a new sub-query */
static int handle_merge_with_pipeline(cypher_executor *executor, cypher_query *query,
                                      cypher_result *result, clause_flags flags)
{
    (void)flags;
    CYPHER_DEBUG("Executing MERGE+WITH pipeline");

    if (!query || !query->clauses) {
        set_result_error(result, "Invalid query in MERGE+WITH pipeline");
        return -1;
    }

    /* Find the WITH clause position to split */
    int with_pos = -1;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_WITH) {
            with_pos = i;
            break;
        }
    }

    if (with_pos < 0) {
        set_result_error(result, "MERGE+WITH pipeline: no WITH clause found");
        return -1;
    }

    /* --- Phase 1: Execute pre-WITH clauses (MERGE + SET) --- */
    cypher_merge *merge = NULL;
    for (int i = 0; i < with_pos; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MERGE) {
            merge = (cypher_merge*)query->clauses->items[i];
            break;
        }
    }

    if (!merge) {
        /* No MERGE before WITH — this is a MATCH+WITH+...+MERGE pattern.
         * Execute all MATCH clauses to resolve variables, then execute
         * the post-WITH MERGE with those bindings. */
        CYPHER_DEBUG("MERGE+WITH pipeline: MERGE is after WITH, using match-then-merge strategy");

        /* Find the post-WITH MERGE */
        cypher_merge *post_merge = NULL;
        for (int i = with_pos + 1; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type == AST_NODE_MERGE) {
                post_merge = (cypher_merge*)query->clauses->items[i];
                break;
            }
        }
        if (!post_merge) {
            set_result_error(result, "MERGE+WITH pipeline: no MERGE clause found");
            return -1;
        }

        /* Execute all MATCH clauses (pre and post-WITH) to resolve variables */
        variable_map *resolved_vars = create_variable_map();
        if (!resolved_vars) {
            set_result_error(result, "Failed to create variable map");
            return -1;
        }

        for (int i = 0; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type != AST_NODE_MATCH) continue;
            cypher_match *m = (cypher_match*)query->clauses->items[i];

            cypher_transform_context *mctx = cypher_transform_create_context(executor->db);
            if (!mctx) continue;

            if (transform_match_clause(mctx, m) == 0) {
                /* Build SELECT alias.id AS varname_id FROM ... WHERE ... */
                int vcount = transform_var_count(mctx->var_ctx);
                const char *from_str = sql_builder_get_from(mctx->unified_builder);
                const char *joins_str = sql_builder_get_joins(mctx->unified_builder);
                const char *where_str = sql_builder_get_where(mctx->unified_builder);

                char id_sql[4096];
                size_t pos = 0;
                pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, "SELECT ");
                bool first_col = true;
                for (int vi = 0; vi < vcount; vi++) {
                    transform_var *tv = transform_var_at(mctx->var_ctx, vi);
                    if (tv && tv->kind == VAR_KIND_NODE) {
                        if (!first_col) pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, ", ");
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos,
                                        "%s.id AS \"%s_id\"", tv->table_alias, tv->name);
                        first_col = false;
                    }
                }
                if (!first_col) {
                    if (from_str && from_str[0])
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " FROM %s", from_str);
                    if (joins_str && joins_str[0])
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " %s", joins_str);
                    if (where_str && where_str[0])
                        pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " WHERE %s", where_str);

                    sqlite3_stmt *match_stmt;
                    if (sqlite3_prepare_v2(executor->db, id_sql, -1, &match_stmt, NULL) == SQLITE_OK) {
                        if (sqlite3_step(match_stmt) == SQLITE_ROW) {
                            int mcols = sqlite3_column_count(match_stmt);
                            for (int mc = 0; mc < mcols; mc++) {
                                const char *cname = sqlite3_column_name(match_stmt, mc);
                                if (cname && sqlite3_column_type(match_stmt, mc) == SQLITE_INTEGER) {
                                    char var_name[128];
                                    strncpy(var_name, cname, sizeof(var_name) - 1);
                                    var_name[sizeof(var_name) - 1] = '\0';
                                    char *suffix = strstr(var_name, "_id");
                                    if (suffix) {
                                        *suffix = '\0';
                                        set_variable_node_id(resolved_vars, var_name,
                                            sqlite3_column_int(match_stmt, mc));
                                        CYPHER_DEBUG("WITH+MERGE: resolved '%s' = node %d",
                                                     var_name, sqlite3_column_int(match_stmt, mc));
                                    }
                                }
                            }
                        }
                        sqlite3_finalize(match_stmt);
                    }
                }
            }
            cypher_transform_free_context(mctx);
        }

        /* Execute MERGE with resolved variable bindings */
        int rc = execute_merge_clause_with_vars(executor, post_merge, result, resolved_vars);

        /* Execute any post-WITH SET clauses */
        for (int i = with_pos + 1; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type == AST_NODE_SET) {
                execute_set_operations(executor, (cypher_set*)query->clauses->items[i],
                                       resolved_vars, result);
            }
        }

        free_variable_map(resolved_vars);
        if (rc < 0) return -1;
        result->success = true;
        return 0;
    }

    /* Execute the MERGE clause (handles ON CREATE SET internally) */
    int rc = execute_merge_clause(executor, merge, result);
    if (rc < 0) return -1;

    /* Execute any standalone SET clauses between MERGE and WITH */
    for (int i = 0; i < with_pos; i++) {
        if (query->clauses->items[i]->type == AST_NODE_SET) {
            cypher_set *set_clause = (cypher_set*)query->clauses->items[i];

            /* Build variable map from MERGE pattern by re-resolving node IDs */
            variable_map *var_map = create_variable_map();
            if (!var_map) {
                set_result_error(result, "Failed to create variable map for SET");
                return -1;
            }

            for (int p = 0; p < merge->pattern->count; p++) {
                ast_node *pat = merge->pattern->items[p];
                if (pat->type != AST_NODE_PATH) continue;
                cypher_path *path = (cypher_path*)pat;
                for (int j = 0; j < path->elements->count; j++) {
                    ast_node *elem = path->elements->items[j];
                    if (elem->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *np = (cypher_node_pattern*)elem;
                        if (np->variable) {
                            int node_id = find_node_by_pattern(executor, np);
                            if (node_id >= 0) {
                                set_variable_node_id(var_map, np->variable, node_id);
                            }
                        }
                    }
                }
            }

            rc = execute_set_operations(executor, set_clause, var_map, result);
            free_variable_map(var_map);
            if (rc < 0) return -1;
        }
    }

    /* --- Phase 2: Execute post-WITH clauses --- */

    /* Build a variable map with all MERGE'd node IDs for the post-WITH phase */
    variable_map *post_var_map = create_variable_map();
    if (!post_var_map) {
        set_result_error(result, "Failed to create post-WITH variable map");
        return -1;
    }

    for (int p = 0; p < merge->pattern->count; p++) {
        ast_node *pat = merge->pattern->items[p];
        if (pat->type != AST_NODE_PATH) continue;
        cypher_path *path = (cypher_path*)pat;
        for (int j = 0; j < path->elements->count; j++) {
            ast_node *elem = path->elements->items[j];
            if (elem->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *np = (cypher_node_pattern*)elem;
                if (np->variable) {
                    int node_id = find_node_by_pattern(executor, np);
                    if (node_id >= 0) {
                        set_variable_node_id(post_var_map, np->variable, node_id);
                        CYPHER_DEBUG("MERGE+WITH pipeline: carrying variable '%s' = node %d", np->variable, node_id);
                    }
                }
            } else if (elem->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern*)elem;
                if (rp->variable && j > 0 && j + 1 < path->elements->count) {
                    ast_node *src_elem = path->elements->items[j - 1];
                    ast_node *tgt_elem = path->elements->items[j + 1];
                    if (src_elem->type == AST_NODE_NODE_PATTERN &&
                        tgt_elem->type == AST_NODE_NODE_PATTERN) {
                        cypher_node_pattern *src_np = (cypher_node_pattern*)src_elem;
                        cypher_node_pattern *tgt_np = (cypher_node_pattern*)tgt_elem;
                        int src_id = src_np->variable ? get_variable_node_id(post_var_map, src_np->variable) : -1;
                        if (src_id < 0) src_id = find_node_by_pattern(executor, src_np);
                        int tgt_id = tgt_np->variable ? get_variable_node_id(post_var_map, tgt_np->variable) : -1;
                        if (tgt_id < 0) tgt_id = find_node_by_pattern(executor, tgt_np);
                        if (src_id >= 0 && tgt_id >= 0) {
                            int source_id = src_id, dest_id = tgt_id;
                            if (rp->left_arrow && !rp->right_arrow) {
                                source_id = tgt_id;
                                dest_id = src_id;
                            }
                            const char *rel_type = rp->type ? rp->type : "RELATED";
                            int edge_id = find_edge_by_pattern(executor, source_id, dest_id, rel_type, rp);
                            if (edge_id >= 0) {
                                set_variable_edge_id(post_var_map, rp->variable, edge_id);
                                CYPHER_DEBUG("MERGE+WITH pipeline: carrying edge variable '%s' = edge %d", rp->variable, edge_id);
                            }
                        }
                    }
                }
            }
        }
    }

    /* Find post-WITH clauses */
    cypher_match *post_match = NULL;
    cypher_merge *post_merge = NULL;
    cypher_return *post_return = NULL;
    bool has_post_set = false;
    for (int i = with_pos + 1; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH && !post_match) post_match = (cypher_match*)clause;
        if (clause->type == AST_NODE_MERGE && !post_merge) post_merge = (cypher_merge*)clause;
        if (clause->type == AST_NODE_RETURN && !post_return) post_return = (cypher_return*)clause;
        if (clause->type == AST_NODE_SET) has_post_set = true;
    }

    if (post_match && post_merge) {
        /* Execute MATCH to find additional variables, then MERGE with combined var_map */
        /* Transform MATCH to SQL and execute to get matched node IDs */
        cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
        if (!ctx) {
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to create transform context for post-WITH");
            return -1;
        }

        if (transform_match_clause(ctx, post_match) < 0) {
            cypher_transform_free_context(ctx);
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to transform post-WITH MATCH clause");
            return -1;
        }

        if (finalize_sql_generation(ctx) < 0) {
            cypher_transform_free_context(ctx);
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to finalize post-WITH SQL");
            return -1;
        }

        /* Build SELECT with node variable IDs */
        char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
        if (select_pos) {
            char *after_star = select_pos + strlen("SELECT *");
            char *temp = strdup(after_star);
            ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
            ctx->sql_buffer[ctx->sql_size] = '\0';

            bool first = true;
            int var_count = transform_var_count(ctx->var_ctx);
            for (int vi = 0; vi < var_count; vi++) {
                transform_var *var = transform_var_at(ctx->var_ctx, vi);
                if (var && var->kind == VAR_KIND_NODE) {
                    if (!first) append_sql(ctx, ", ");
                    append_sql(ctx, "%s.id AS \"%s_id\"", var->table_alias, var->name);
                    first = false;
                }
            }
            append_sql(ctx, " %s", temp);
            free(temp);
        }

        CYPHER_DEBUG("MERGE+WITH pipeline: post-WITH MATCH SQL: %s", ctx->sql_buffer);

        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            cypher_transform_free_context(ctx);
            free_variable_map(post_var_map);
            set_result_error(result, "Failed to prepare post-WITH MATCH SQL");
            return -1;
        }

        if (executor->params_json) {
            if (bind_params_from_json(stmt, executor->params_json) < 0) {
                set_result_error(result, "MERGE+WITH pipeline: failed to bind parameters");
                sqlite3_finalize(stmt);
                cypher_transform_free_context(ctx);
                free_variable_map(post_var_map);
                return -1;
            }
        }

        /* Execute MATCH and add found variables to our map */
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int col_count = sqlite3_column_count(stmt);
            for (int c = 0; c < col_count; c++) {
                const char *col_name = sqlite3_column_name(stmt, c);
                if (col_name) {
                    /* Column names are "varname_id" */
                    char var_name[128];
                    strncpy(var_name, col_name, sizeof(var_name) - 1);
                    var_name[sizeof(var_name) - 1] = '\0';
                    char *suffix = strstr(var_name, "_id");
                    if (suffix) {
                        *suffix = '\0';
                        int node_id = sqlite3_column_int(stmt, c);
                        /* Only add if not already in map (MERGE variables take priority) */
                        if (get_variable_node_id(post_var_map, var_name) < 0) {
                            set_variable_node_id(post_var_map, var_name, node_id);
                            CYPHER_DEBUG("MERGE+WITH pipeline: MATCH found '%s' = node %d", var_name, node_id);
                        }
                    }
                }
            }
        }

        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);

        /* Now execute the post-WITH MERGE with the combined variable map */
        rc = execute_merge_with_variables(executor, post_merge, post_var_map, result);
    } else {
        /* Handle post-WITH SET and/or RETURN */

        /* Execute all post-WITH SET clauses */
        for (int i = with_pos + 1; i < query->clauses->count; i++) {
            if (query->clauses->items[i]->type == AST_NODE_SET) {
                cypher_set *set_clause = (cypher_set*)query->clauses->items[i];
                rc = execute_set_operations(executor, set_clause, post_var_map, result);
                if (rc < 0) {
                    free_variable_map(post_var_map);
                    return -1;
                }
            }
        }

        /* Execute post-WITH RETURN if present */
        if (post_return) {
            rc = merge_with_execute_return(executor, post_return, post_var_map, result);
            if (rc < 0) {
                free_variable_map(post_var_map);
                return -1;
            }
        } else if (has_post_set) {
            /* SET without RETURN: mark success */
            result->success = true;
            rc = 0;
        } else {
            set_result_error(result, "MERGE+WITH pipeline: unsupported post-WITH clause combination");
            rc = -1;
        }
    }

    free_variable_map(post_var_map);

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
static int handle_unwind_create(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
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

    /* Handle list literal UNWIND */
    if (unwind->expr->type != AST_NODE_LIST) {
        set_result_error(result, "UNWIND+CREATE requires a list literal or parameter");
        return -1;
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

            if (execute_merge_clause(executor, merge, result) < 0) {
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

            if (execute_merge_clause(executor, merge, result) < 0) {
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
static int handle_create_return(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_create *create = find_create_clause(query);
    cypher_return *ret = find_return_clause(query);

    CYPHER_DEBUG("Executing CREATE+RETURN via pattern dispatch");

    /* Execute CREATE and keep the variable map */
    variable_map *var_map = NULL;
    int rc = execute_create_clause_with_varmap(executor, create, result, &var_map);
    if (rc < 0) {
        if (var_map) free_variable_map(var_map);
        return -1;
    }

    if (!var_map || var_map->count == 0 || !ret || !ret->items) {
        result->success = true;
        if (var_map) free_variable_map(var_map);
        return 0;
    }

    /* Build a SQL query to fetch the RETURN data from created nodes.
     * For each return item like p.name, generate:
     *   SELECT value FROM node_props_text WHERE node_id = ? AND key_id = (
     *     SELECT id FROM property_keys WHERE key = 'name')
     * We build a single row with all requested columns. */
    int col_count = ret->items->count;
    result->column_count = col_count;
    result->column_names = malloc(col_count * sizeof(char*));
    result->row_count = 1;
    result->data = malloc(sizeof(char**));
    result->data[0] = malloc(col_count * sizeof(char*));

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
                if (node_id >= 0) {
                    /* Query each property type table for the value */
                    const char *type_tables[] = {
                        "node_props_text", "node_props_int",
                        "node_props_real", "node_props_bool", NULL
                    };
                    for (int t = 0; type_tables[t]; t++) {
                        char sql[512];
                        snprintf(sql, sizeof(sql),
                            "SELECT CAST(value AS TEXT) FROM %s "
                            "WHERE node_id = %d AND key_id = "
                            "(SELECT id FROM property_keys WHERE key = '%s')",
                            type_tables[t], node_id, prop_name);

                        sqlite3_stmt *stmt;
                        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                            if (sqlite3_step(stmt) == SQLITE_ROW) {
                                const char *val = (const char*)sqlite3_column_text(stmt, 0);
                                if (val) {
                                    result->data[0][i] = strdup(val);
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
        }
    }

    result->success = true;
    free_variable_map(var_map);
    return 0;
}

/*
 * CALL {} subquery handler
 *
 * Splits the query at the CALL clause, executes the outer part to get rows,
 * then dispatches the inner subquery for each outer row.
 */
static int handle_call_subquery(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    (void)flags;
    CYPHER_DEBUG("Executing CALL {} subquery");

    if (!query || !query->clauses) {
        set_result_error(result, "Invalid query in CALL subquery handler");
        return -1;
    }

    /* Find the CALL clause position */
    int call_pos = -1;
    cypher_call_subquery *call_node = NULL;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_CALL_SUBQUERY) {
            call_pos = i;
            call_node = (cypher_call_subquery*)query->clauses->items[i];
            break;
        }
    }

    if (call_pos < 0 || !call_node) {
        set_result_error(result, "CALL subquery handler: no CALL clause found");
        return -1;
    }

    /* Get the inner query from the CALL node */
    if (!call_node->branches || call_node->branches->count == 0) {
        set_result_error(result, "CALL subquery has no inner query");
        return -1;
    }

    ast_node *inner_ast = call_node->branches->items[0];

    /*
     * Case 1: Standalone CALL with no outer MATCH
     * e.g., CALL { MATCH (n) RETURN n }
     */
    if (call_pos == 0 && query->clauses->count == 1) {
        CYPHER_DEBUG("Standalone CALL — dispatching inner query directly");
        cypher_result *inner_result = cypher_executor_execute_ast(executor, inner_ast);
        if (!inner_result) {
            set_result_error(result, "Failed to execute CALL subquery");
            return -1;
        }

        /* Copy inner result to outer result */
        result->success = inner_result->success;
        if (inner_result->error_message) {
            result->error_message = strdup(inner_result->error_message);
        }
        result->row_count = inner_result->row_count;
        result->column_count = inner_result->column_count;
        result->nodes_created = inner_result->nodes_created;
        result->nodes_deleted = inner_result->nodes_deleted;
        result->relationships_created = inner_result->relationships_created;
        result->relationships_deleted = inner_result->relationships_deleted;
        result->properties_set = inner_result->properties_set;

        /* Transfer data ownership */
        result->column_names = inner_result->column_names;
        inner_result->column_names = NULL;
        result->data = inner_result->data;
        inner_result->data = NULL;
        result->data_types = inner_result->data_types;
        inner_result->data_types = NULL;
        result->agtype_data = inner_result->agtype_data;
        inner_result->agtype_data = NULL;
        result->use_agtype = inner_result->use_agtype;

        cypher_result_free(inner_result);
        return result->success ? 0 : -1;
    }

    /*
     * Case 2: Outer MATCH + CALL (with or without post-CALL RETURN)
     * e.g., MATCH (a) CALL { WITH a SET a.x = 1 }
     * e.g., MATCH (a) CALL { WITH a MATCH (b) RETURN b } RETURN a, b
     */

    /* Check if there are pre-CALL MATCH clauses */
    bool has_outer_match = false;
    for (int i = 0; i < call_pos; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MATCH) {
            has_outer_match = true;
            break;
        }
    }

    if (!has_outer_match) {
        /* No outer MATCH — just execute the inner query directly
         * This handles CALL { ... } RETURN ... (no outer context) */
        CYPHER_DEBUG("CALL without outer MATCH — executing inner directly");
        cypher_result *inner_result = cypher_executor_execute_ast(executor, inner_ast);
        if (!inner_result || !inner_result->success) {
            const char *err = (inner_result && inner_result->error_message) ?
                              inner_result->error_message : "Failed to execute CALL subquery";
            set_result_error(result, err);
            if (inner_result) cypher_result_free(inner_result);
            return -1;
        }
        result->success = true;
        result->nodes_created += inner_result->nodes_created;
        result->nodes_deleted += inner_result->nodes_deleted;
        result->relationships_created += inner_result->relationships_created;
        result->relationships_deleted += inner_result->relationships_deleted;
        result->properties_set += inner_result->properties_set;
        cypher_result_free(inner_result);
        return 0;
    }

    /*
     * Has outer MATCH — transform pre-CALL clauses to get outer rows,
     * then execute inner subquery per row.
     */
    CYPHER_DEBUG("CALL with outer MATCH at position %d", call_pos);

    /* Build a temporary query from pre-CALL clauses to get outer row data */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context for outer query");
        return -1;
    }

    /* Transform pre-CALL MATCH clauses */
    for (int i = 0; i < call_pos; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH) {
            if (transform_match_clause(ctx, (cypher_match*)clause) < 0) {
                set_result_error(result, ctx->error_message ?
                                ctx->error_message : "Failed to transform outer MATCH");
                cypher_transform_free_context(ctx);
                return -1;
            }
        }
    }

    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "Failed to finalize outer query SQL");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Replace SELECT * with SELECT of node variable IDs */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
        char *after_star = select_pos + strlen("SELECT *");
        char *rest = strdup(after_star);
        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';

        bool first = true;
        int var_count = transform_var_count(ctx->var_ctx);
        for (int vi = 0; vi < var_count; vi++) {
            transform_var *var = transform_var_at(ctx->var_ctx, vi);
            if (var && (var->kind == VAR_KIND_NODE || var->kind == VAR_KIND_EDGE)) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS \"%s_id\"", var->table_alias, var->name);
                first = false;
            }
        }
        if (first) {
            /* No node/edge variables — just select 1 as placeholder */
            append_sql(ctx, "1");
        }
        append_sql(ctx, " %s", rest);
        free(rest);
    }

    prepend_cte_to_sql(ctx);

    CYPHER_DEBUG("CALL outer query SQL: %s", ctx->sql_buffer);

    /* Prepare and execute outer query */
    sqlite3_stmt *outer_stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &outer_stmt, NULL);
    if (rc != SQLITE_OK) {
        set_result_error(result, "Failed to prepare outer query for CALL subquery");
        cypher_transform_free_context(ctx);
        return -1;
    }

    if (executor->params_json) {
        bind_params_from_json(outer_stmt, executor->params_json);
    }

    /* Iterate outer rows and execute inner subquery per row */
    int total_inner_rows = 0;
    result->success = true;

    /* Get the inner query and find its clauses */
    cypher_query *inner_query = NULL;
    if (inner_ast->type == AST_NODE_QUERY || inner_ast->type == AST_NODE_SINGLE_QUERY) {
        inner_query = (cypher_query*)inner_ast;
    }

    /* Check for inner RETURN clause and post-CALL RETURN to enable result accumulation */
    cypher_return *inner_return_clause = NULL;
    bool has_post_call_return = false;
    if (inner_query && inner_query->clauses) {
        for (int ci = 0; ci < inner_query->clauses->count; ci++) {
            if (inner_query->clauses->items[ci]->type == AST_NODE_RETURN) {
                inner_return_clause = (cypher_return*)inner_query->clauses->items[ci];
                break;
            }
        }
    }
    for (int i = call_pos + 1; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_RETURN) {
            has_post_call_return = true;
            break;
        }
    }

    /* Accumulator for combined outer+inner results */
    bool accumulating = (inner_return_clause != NULL && has_post_call_return);
    int accum_capacity = 16;
    int accum_count = 0;
    char ***accum_rows = accumulating ? calloc(accum_capacity, sizeof(char**)) : NULL;
    int accum_col_count = 0;
    char **accum_col_names = NULL;

    while (sqlite3_step(outer_stmt) == SQLITE_ROW) {
        CYPHER_DEBUG("CALL: processing outer row");

        /* Build variable map from outer row columns (format: varname_id) */
        variable_map *var_map = create_variable_map();
        if (!var_map) {
            set_result_error(result, "Failed to create variable map for CALL");
            sqlite3_finalize(outer_stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }

        int col_count = sqlite3_column_count(outer_stmt);
        for (int c = 0; c < col_count; c++) {
            const char *col_name = sqlite3_column_name(outer_stmt, c);
            if (col_name && sqlite3_column_type(outer_stmt, c) == SQLITE_INTEGER) {
                char var_name[128];
                strncpy(var_name, col_name, sizeof(var_name) - 1);
                var_name[sizeof(var_name) - 1] = '\0';
                char *suffix = strstr(var_name, "_id");
                if (suffix) {
                    *suffix = '\0';
                    int node_id = sqlite3_column_int(outer_stmt, c);
                    set_variable_node_id(var_map, var_name, node_id);
                    CYPHER_DEBUG("CALL: bound outer variable '%s' = node %d", var_name, node_id);
                }
            }
        }

        /* Build a scoped variable map: only include variables listed in the
         * leading WITH clause (scope isolation per openCypher spec).
         * If there is no leading WITH, no outer variables are accessible. */
        variable_map *scoped_map = create_variable_map();
        if (!scoped_map) {
            free_variable_map(var_map);
            set_result_error(result, "Failed to create scoped variable map");
            sqlite3_finalize(outer_stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }

        if (inner_query && inner_query->clauses && inner_query->clauses->count > 0 &&
            inner_query->clauses->items[0]->type == AST_NODE_WITH) {
            cypher_with *with = (cypher_with*)inner_query->clauses->items[0];
            if (with->items) {
                for (int wi = 0; wi < with->items->count; wi++) {
                    cypher_return_item *item = (cypher_return_item*)with->items->items[wi];
                    /* Simple identifier import: WITH a */
                    if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
                        const char *name = ((cypher_identifier*)item->expr)->name;
                        int node_id = get_variable_node_id(var_map, name);
                        if (node_id >= 0) {
                            const char *alias = item->alias ? item->alias : name;
                            set_variable_node_id(scoped_map, alias, node_id);
                            CYPHER_DEBUG("CALL WITH: imported '%s' as '%s' = node %d",
                                         name, alias, node_id);
                        }
                    }
                    /* WITH expressions (e.g., WITH a.name AS n) are not node IDs —
                     * they would need value-level binding which is a future enhancement.
                     * For now, property expressions are skipped with a debug note. */
                    else if (item->expr) {
                        CYPHER_DEBUG("CALL WITH: skipping non-identifier expression (type %s)",
                                     ast_node_type_name(item->expr->type));
                    }
                }
            }
        }

        /* Execute inner subquery clauses with scoped variable bindings.
         * Skip the leading WITH clause (already processed above). */
        if (inner_query && inner_query->clauses) {
            for (int ci = 0; ci < inner_query->clauses->count; ci++) {
                ast_node *inner_clause = inner_query->clauses->items[ci];

                if (inner_clause->type == AST_NODE_WITH) {
                    /* Already processed above for scope building */
                    continue;
                }

                if (inner_clause->type == AST_NODE_SET) {
                    rc = execute_set_operations(executor, (cypher_set*)inner_clause,
                                                scoped_map, result);
                    if (rc < 0) {
                        free_variable_map(scoped_map);
                        free_variable_map(var_map);
                        sqlite3_finalize(outer_stmt);
                        cypher_transform_free_context(ctx);
                        return -1;
                    }
                } else if (inner_clause->type == AST_NODE_MERGE) {
                    rc = execute_merge_clause_with_vars(executor,
                            (cypher_merge*)inner_clause, result, scoped_map);
                    if (rc < 0) {
                        free_variable_map(scoped_map);
                        free_variable_map(var_map);
                        sqlite3_finalize(outer_stmt);
                        cypher_transform_free_context(ctx);
                        return -1;
                    }
                } else if (inner_clause->type == AST_NODE_CREATE) {
                    rc = execute_create_clause(executor, (cypher_create*)inner_clause, result);
                    if (rc < 0) {
                        free_variable_map(scoped_map);
                        free_variable_map(var_map);
                        sqlite3_finalize(outer_stmt);
                        cypher_transform_free_context(ctx);
                        return -1;
                    }
                } else if (inner_clause->type == AST_NODE_MATCH) {
                    /* Execute inner MATCH to resolve variables into scoped_map.
                     * Iterate ALL rows and execute subsequent clauses for each. */
                    cypher_match *inner_match = (cypher_match*)inner_clause;
                    cypher_transform_context *match_ctx = cypher_transform_create_context(executor->db);
                    if (match_ctx) {
                        if (transform_match_clause(match_ctx, inner_match) == 0) {
                            sql_builder *sb = match_ctx->unified_builder;
                            int vcount = transform_var_count(match_ctx->var_ctx);
                            const char *from_str = sql_builder_get_from(sb);
                            const char *joins_str = sql_builder_get_joins(sb);
                            const char *where_str = sql_builder_get_where(sb);

                            char id_sql[4096];
                            size_t pos = 0;
                            pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, "SELECT ");
                            bool first_col = true;
                            for (int vi = 0; vi < vcount; vi++) {
                                transform_var *tv = transform_var_at(match_ctx->var_ctx, vi);
                                if (tv && tv->kind == VAR_KIND_NODE) {
                                    if (!first_col) pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, ", ");
                                    pos += snprintf(id_sql + pos, sizeof(id_sql) - pos,
                                                    "%s.id AS \"%s_id\"", tv->table_alias, tv->name);
                                    first_col = false;
                                }
                            }
                            if (first_col) {
                                cypher_transform_free_context(match_ctx);
                                continue;
                            }
                            if (from_str && from_str[0]) {
                                pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " FROM %s", from_str);
                            }
                            if (joins_str && joins_str[0]) {
                                pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " %s", joins_str);
                            }
                            if (where_str && where_str[0]) {
                                pos += snprintf(id_sql + pos, sizeof(id_sql) - pos, " WHERE %s", where_str);
                            }

                            CYPHER_DEBUG("CALL inner MATCH SQL: %s", id_sql);
                            sqlite3_stmt *match_stmt;
                            if (sqlite3_prepare_v2(executor->db, id_sql, -1, &match_stmt, NULL) == SQLITE_OK) {
                                if (executor->params_json) {
                                    bind_params_from_json(match_stmt, executor->params_json);
                                }
                                while (sqlite3_step(match_stmt) == SQLITE_ROW) {
                                    int mcols = sqlite3_column_count(match_stmt);
                                    for (int mc = 0; mc < mcols; mc++) {
                                        const char *cname = sqlite3_column_name(match_stmt, mc);
                                        if (cname && sqlite3_column_type(match_stmt, mc) == SQLITE_INTEGER) {
                                            int node_id = sqlite3_column_int(match_stmt, mc);
                                            char var_name[128];
                                            strncpy(var_name, cname, sizeof(var_name) - 1);
                                            var_name[sizeof(var_name) - 1] = '\0';
                                            char *suffix = strstr(var_name, "_id");
                                            if (suffix) {
                                                *suffix = '\0';
                                                set_variable_node_id(scoped_map, var_name, node_id);
                                                CYPHER_DEBUG("CALL MATCH: resolved '%s' = node %d",
                                                             var_name, node_id);
                                            }
                                        }
                                    }
                                    /* Execute all post-MATCH clauses for this row */
                                    for (int cj = ci + 1; cj < inner_query->clauses->count; cj++) {
                                        ast_node *post_clause = inner_query->clauses->items[cj];
                                        if (post_clause->type == AST_NODE_SET) {
                                            rc = execute_set_operations(executor, (cypher_set*)post_clause,
                                                                       scoped_map, result);
                                        } else if (post_clause->type == AST_NODE_MERGE) {
                                            rc = execute_merge_clause_with_vars(executor,
                                                    (cypher_merge*)post_clause, result, scoped_map);
                                        } else if (post_clause->type == AST_NODE_CREATE) {
                                            rc = execute_create_clause(executor, (cypher_create*)post_clause, result);
                                        }
                                        if (rc < 0) break;
                                    }
                                    if (rc < 0) break;
                                }
                                sqlite3_finalize(match_stmt);
                            }
                        }
                        cypher_transform_free_context(match_ctx);
                    }
                    /* All post-MATCH clauses handled inside the while loop */
                    break;
                } else if (inner_clause->type == AST_NODE_RETURN) {
                    /* RETURN in inner query — handled after the loop
                     * via post-CALL return path */
                    continue;
                } else {
                    CYPHER_DEBUG("CALL: skipping unsupported inner clause type %s",
                                ast_node_type_name(inner_clause->type));
                }
            }
        } else if (inner_ast->type == AST_NODE_UNION) {
            /* Inner query is a UNION — execute each branch with scoped bindings.
             * Walk the UNION tree: left may be another UNION or a query,
             * right is always a single query. */
            ast_node *union_branches[32]; /* max 32 branches */
            int branch_count = 0;

            /* Flatten UNION tree into branch array */
            ast_node *cur = inner_ast;
            while (cur && cur->type == AST_NODE_UNION && branch_count < 31) {
                cypher_union *u = (cypher_union*)cur;
                union_branches[branch_count++] = u->right;
                cur = u->left;
            }
            if (cur && branch_count < 32) {
                union_branches[branch_count++] = cur;
            }

            /* Execute branches in reverse order (left-to-right) */
            for (int bi = branch_count - 1; bi >= 0; bi--) {
                ast_node *branch = union_branches[bi];
                if (branch->type == AST_NODE_QUERY || branch->type == AST_NODE_SINGLE_QUERY) {
                    cypher_query *bq = (cypher_query*)branch;
                    /* Build scoped map for this branch's WITH */
                    variable_map *branch_scope = create_variable_map();
                    if (branch_scope && bq->clauses && bq->clauses->count > 0 &&
                        bq->clauses->items[0]->type == AST_NODE_WITH) {
                        cypher_with *with = (cypher_with*)bq->clauses->items[0];
                        if (with->items) {
                            for (int wi = 0; wi < with->items->count; wi++) {
                                cypher_return_item *item = (cypher_return_item*)with->items->items[wi];
                                if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
                                    const char *name = ((cypher_identifier*)item->expr)->name;
                                    int node_id = get_variable_node_id(var_map, name);
                                    if (node_id >= 0) {
                                        const char *alias = item->alias ? item->alias : name;
                                        set_variable_node_id(branch_scope, alias, node_id);
                                    }
                                }
                            }
                        }
                    }

                    /* Execute branch clauses (skip WITH) */
                    for (int ci = 0; ci < bq->clauses->count; ci++) {
                        ast_node *bc = bq->clauses->items[ci];
                        if (bc->type == AST_NODE_WITH) continue;
                        if (bc->type == AST_NODE_SET) {
                            execute_set_operations(executor, (cypher_set*)bc, branch_scope, result);
                        } else if (bc->type == AST_NODE_MERGE) {
                            execute_merge_clause(executor, (cypher_merge*)bc, result);
                        } else if (bc->type == AST_NODE_CREATE) {
                            execute_create_clause(executor, (cypher_create*)bc, result);
                        }
                    }
                    free_variable_map(branch_scope);
                }
            }
        } else {
            /* Unknown inner type — try direct dispatch */
            cypher_result *inner_result = cypher_executor_execute_ast(executor, inner_ast);
            if (inner_result) {
                result->nodes_created += inner_result->nodes_created;
                result->properties_set += inner_result->properties_set;
                total_inner_rows += inner_result->row_count;
                cypher_result_free(inner_result);
            }
        }

        /* Execute inner RETURN and accumulate results if needed */
        if (accumulating && inner_return_clause && inner_return_clause->items) {
            cypher_return *post_ret = NULL;
            for (int i = call_pos + 1; i < query->clauses->count; i++) {
                if (query->clauses->items[i]->type == AST_NODE_RETURN) {
                    post_ret = (cypher_return*)query->clauses->items[i];
                    break;
                }
            }

            if (post_ret && post_ret->items) {
                /* Evaluate inner RETURN items by building a single SQL query
                 * that resolves all expressions against the scoped variables. */
                char inner_sql[4096];
                size_t ipos = 0;
                ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos, "SELECT ");

                cypher_transform_context *ret_ctx = cypher_transform_create_context(executor->db);
                if (ret_ctx) {
                    /* Register scoped_map variables with unique aliases */
                    for (int si = 0; si < scoped_map->count; si++) {
                        variable_mapping *m = &scoped_map->mappings[si];
                        char var_alias[64];
                        snprintf(var_alias, sizeof(var_alias), "_cv_%d", si);
                        if (m->type == VAR_MAP_TYPE_NODE) {
                            transform_var_register_node(ret_ctx->var_ctx, m->variable, var_alias, NULL);
                        } else {
                            transform_var_register_edge(ret_ctx->var_ctx, m->variable, var_alias, NULL);
                        }
                        transform_var_set_bound(ret_ctx->var_ctx, m->variable, true);
                    }

                    /* Build SELECT expressions for inner RETURN items */
                    bool inner_ok = true;
                    for (int ri = 0; ri < inner_return_clause->items->count; ri++) {
                        cypher_return_item *ret_item = (cypher_return_item*)inner_return_clause->items->items[ri];
                        if (ri > 0) ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos, ", ");

                        char *saved_buf = ret_ctx->sql_buffer;
                        int saved_size = ret_ctx->sql_size;
                        int saved_cap = ret_ctx->sql_capacity;
                        char temp_buf[2048] = {0};
                        ret_ctx->sql_buffer = temp_buf;
                        ret_ctx->sql_size = 0;
                        ret_ctx->sql_capacity = sizeof(temp_buf);

                        if (transform_expression(ret_ctx, ret_item->expr) == 0 && ret_ctx->sql_size > 0) {
                            const char *col_name = ret_item->alias ? ret_item->alias : temp_buf;
                            ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos, "%s AS \"%s\"",
                                             temp_buf, col_name);
                        } else {
                            inner_ok = false;
                        }

                        ret_ctx->sql_buffer = saved_buf;
                        ret_ctx->sql_size = saved_size;
                        ret_ctx->sql_capacity = saved_cap;
                    }

                    /* Build FROM/WHERE to pin variables to exact entity IDs */
                    if (inner_ok && scoped_map->count > 0) {
                        for (int si = 0; si < scoped_map->count; si++) {
                            variable_mapping *m = &scoped_map->mappings[si];
                            char var_alias[64];
                            snprintf(var_alias, sizeof(var_alias), "_cv_%d", si);
                            const char *table = m->type == VAR_MAP_TYPE_NODE ? "nodes" : "edges";
                            if (si == 0) {
                                ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos,
                                                 " FROM %s AS %s WHERE %s.id = %d",
                                                 table, var_alias, var_alias, m->entity_id);
                            } else {
                                ipos += snprintf(inner_sql + ipos, sizeof(inner_sql) - ipos,
                                                 " JOIN %s AS %s ON %s.id = %d",
                                                 table, var_alias, var_alias, m->entity_id);
                            }
                        }
                    }

                    cypher_transform_free_context(ret_ctx);

                    /* Store inner RETURN name→value pairs */
                    int inner_col_count = 0;
                    char **inner_col_names_local = NULL;
                    char **inner_col_values = NULL;

                    if (inner_ok) {
                        CYPHER_DEBUG("CALL inner RETURN SQL: %s", inner_sql);
                        sqlite3_stmt *inner_stmt;
                        if (sqlite3_prepare_v2(executor->db, inner_sql, -1, &inner_stmt, NULL) == SQLITE_OK) {
                            if (executor->params_json) {
                                bind_params_from_json(inner_stmt, executor->params_json);
                            }
                            if (sqlite3_step(inner_stmt) == SQLITE_ROW) {
                                inner_col_count = sqlite3_column_count(inner_stmt);
                                inner_col_values = calloc(inner_col_count, sizeof(char*));
                                inner_col_names_local = calloc(inner_col_count, sizeof(char*));
                                for (int ic = 0; ic < inner_col_count; ic++) {
                                    const char *cn = sqlite3_column_name(inner_stmt, ic);
                                    const char *cv = (const char*)sqlite3_column_text(inner_stmt, ic);
                                    inner_col_names_local[ic] = cn ? strdup(cn) : strdup("");
                                    inner_col_values[ic] = cv ? strdup(cv) : NULL;
                                }
                            }
                            sqlite3_finalize(inner_stmt);
                        }
                    }

                    /* Build combined row for post-CALL RETURN */
                    if (inner_col_values) {
                        int total_cols = post_ret->items->count;

                        if (accum_col_names == NULL) {
                            accum_col_count = total_cols;
                            accum_col_names = calloc(total_cols, sizeof(char*));
                            for (int pi = 0; pi < total_cols; pi++) {
                                cypher_return_item *item = (cypher_return_item*)post_ret->items->items[pi];
                                if (item->alias) {
                                    accum_col_names[pi] = strdup(item->alias);
                                } else if (item->expr->type == AST_NODE_IDENTIFIER) {
                                    accum_col_names[pi] = strdup(((cypher_identifier*)item->expr)->name);
                                } else if (item->expr->type == AST_NODE_PROPERTY) {
                                    cypher_property *p = (cypher_property*)item->expr;
                                    cypher_identifier *base = (cypher_identifier*)p->expr;
                                    char col_buf[256];
                                    snprintf(col_buf, sizeof(col_buf), "%s.%s", base->name, p->property_name);
                                    accum_col_names[pi] = strdup(col_buf);
                                } else {
                                    accum_col_names[pi] = strdup("?column?");
                                }
                            }
                        }

                        char **row = calloc(total_cols, sizeof(char*));
                        for (int pi = 0; pi < total_cols; pi++) {
                            cypher_return_item *item = (cypher_return_item*)post_ret->items->items[pi];
                            bool resolved = false;

                            /* Check inner RETURN columns first */
                            if (item->expr->type == AST_NODE_IDENTIFIER) {
                                const char *ref = ((cypher_identifier*)item->expr)->name;
                                for (int ic = 0; ic < inner_col_count; ic++) {
                                    if (inner_col_names_local[ic] && strcmp(inner_col_names_local[ic], ref) == 0) {
                                        row[pi] = inner_col_values[ic] ? strdup(inner_col_values[ic]) : NULL;
                                        resolved = true;
                                        break;
                                    }
                                }
                            }

                            /* Evaluate outer expressions via SQL */
                            if (!resolved) {
                                cypher_transform_context *eval_ctx = cypher_transform_create_context(executor->db);
                                if (eval_ctx) {
                                    for (int si = 0; si < var_map->count; si++) {
                                        variable_mapping *m = &var_map->mappings[si];
                                        char va[64];
                                        snprintf(va, sizeof(va), "_ov_%d", si);
                                        if (m->type == VAR_MAP_TYPE_NODE)
                                            transform_var_register_node(eval_ctx->var_ctx, m->variable, va, NULL);
                                        else
                                            transform_var_register_edge(eval_ctx->var_ctx, m->variable, va, NULL);
                                        transform_var_set_bound(eval_ctx->var_ctx, m->variable, true);
                                    }

                                    char eval_sql[2048];
                                    size_t epos = 0;
                                    epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos, "SELECT ");

                                    char *sb = eval_ctx->sql_buffer;
                                    int ss = eval_ctx->sql_size;
                                    int sc = eval_ctx->sql_capacity;
                                    char tb[1024] = {0};
                                    eval_ctx->sql_buffer = tb;
                                    eval_ctx->sql_size = 0;
                                    eval_ctx->sql_capacity = sizeof(tb);

                                    if (transform_expression(eval_ctx, item->expr) == 0 && eval_ctx->sql_size > 0) {
                                        epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos, "%s", tb);
                                        eval_ctx->sql_buffer = sb;
                                        eval_ctx->sql_size = ss;
                                        eval_ctx->sql_capacity = sc;

                                        for (int si = 0; si < var_map->count; si++) {
                                            variable_mapping *m = &var_map->mappings[si];
                                            char va[64];
                                            snprintf(va, sizeof(va), "_ov_%d", si);
                                            const char *tbl = m->type == VAR_MAP_TYPE_NODE ? "nodes" : "edges";
                                            if (si == 0)
                                                epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos,
                                                                 " FROM %s AS %s WHERE %s.id = %d", tbl, va, va, m->entity_id);
                                            else
                                                epos += snprintf(eval_sql + epos, sizeof(eval_sql) - epos,
                                                                 " JOIN %s AS %s ON %s.id = %d", tbl, va, va, m->entity_id);
                                        }

                                        sqlite3_stmt *ev;
                                        if (sqlite3_prepare_v2(executor->db, eval_sql, -1, &ev, NULL) == SQLITE_OK) {
                                            if (executor->params_json) {
                                                bind_params_from_json(ev, executor->params_json);
                                            }
                                            if (sqlite3_step(ev) == SQLITE_ROW) {
                                                const char *val = (const char*)sqlite3_column_text(ev, 0);
                                                row[pi] = val ? strdup(val) : NULL;
                                            }
                                            sqlite3_finalize(ev);
                                        }
                                    } else {
                                        eval_ctx->sql_buffer = sb;
                                        eval_ctx->sql_size = ss;
                                        eval_ctx->sql_capacity = sc;
                                    }
                                    cypher_transform_free_context(eval_ctx);
                                }
                            }
                        }

                        if (accum_count >= accum_capacity) {
                            accum_capacity *= 2;
                            accum_rows = realloc(accum_rows, accum_capacity * sizeof(char**));
                        }
                        accum_rows[accum_count++] = row;

                        for (int ic = 0; ic < inner_col_count; ic++) {
                            free(inner_col_names_local[ic]);
                            free(inner_col_values[ic]);
                        }
                        free(inner_col_names_local);
                        free(inner_col_values);
                    }
                }
            }
        }

        free_variable_map(scoped_map);
        free_variable_map(var_map);
    }

    sqlite3_finalize(outer_stmt);
    cypher_transform_free_context(ctx);

    CYPHER_DEBUG("CALL subquery complete: %d inner rows processed", total_inner_rows);

    /* Handle post-CALL RETURN clause if present */
    cypher_return *post_return = NULL;
    for (int i = call_pos + 1; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_RETURN) {
            post_return = (cypher_return*)query->clauses->items[i];
            break;
        }
    }

    /* If we accumulated inner+outer results, use them directly */
    if (accumulating && accum_count > 0 && accum_col_names) {
        result->column_count = accum_col_count;
        result->column_names = accum_col_names;
        result->row_count = accum_count;
        result->data = accum_rows;
        result->success = true;
        return 0;
    }

    /* Clean up accumulator if unused */
    if (accum_rows) {
        for (int i = 0; i < accum_count; i++) {
            if (accum_rows[i]) {
                for (int j = 0; j < accum_col_count; j++) free(accum_rows[i][j]);
                free(accum_rows[i]);
            }
        }
        free(accum_rows);
    }
    if (accum_col_names) {
        for (int i = 0; i < accum_col_count; i++) free(accum_col_names[i]);
        free(accum_col_names);
    }

    if (post_return) {
        /* Post-CALL RETURN — re-execute outer query through the generic transform
         * pipeline which knows how to handle MATCH+RETURN. The CALL side effects
         * have already been applied above. Build a temporary query without CALL. */
        ast_list *outer_clauses = ast_list_create();
        for (int i = 0; i < call_pos; i++) {
            ast_list_append(outer_clauses, query->clauses->items[i]);
        }
        for (int i = call_pos + 1; i < query->clauses->count; i++) {
            ast_list_append(outer_clauses, query->clauses->items[i]);
        }

        /* Temporarily swap clause list */
        ast_list *original_clauses = query->clauses;
        query->clauses = outer_clauses;

        int ret_rc = handle_generic_transform(executor, query, result, flags);

        /* Restore original clause list and free temporary (don't free items — they're borrowed) */
        query->clauses = original_clauses;
        free(outer_clauses->items);
        free(outer_clauses);

        return ret_rc;
    }

    return 0;
}
