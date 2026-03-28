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
static int handle_return_only(cypher_executor *executor, cypher_query *query,
                              cypher_result *result, clause_flags flags);
static int handle_merge_with_pipeline(cypher_executor *executor, cypher_query *query,
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
        .forbidden = CLAUSE_WITH,
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
     * Priority 70: Simple MATCH+RETURN (single, non-optional)
     */
    {
        .name = "MATCH+RETURN",
        .required = CLAUSE_MATCH | CLAUSE_RETURN,
        .forbidden = CLAUSE_OPTIONAL | CLAUSE_MULTI_MATCH | CLAUSE_CREATE |
                     CLAUSE_SET | CLAUSE_DELETE | CLAUSE_MERGE,
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
        .name = "CREATE",
        .required = CLAUSE_CREATE,
        .forbidden = CLAUSE_MATCH | CLAUSE_UNWIND,
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

    CYPHER_DEBUG("Executing MATCH+SET via pattern dispatch");
    int rc = execute_match_set_query(executor, match, set, result);
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

    CYPHER_DEBUG("Executing MATCH+MERGE via pattern dispatch");
    int rc = execute_match_merge_query(executor, match, merge, result);
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

static int handle_match_create(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_match *match = find_match_clause(query);
    cypher_create *create = find_create_clause(query);

    CYPHER_DEBUG("Executing MATCH+CREATE via pattern dispatch");
    int rc = execute_match_create_query(executor, match, create, result);
    if (rc >= 0) {
        result->success = true;
    }
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

    CYPHER_DEBUG("Executing CREATE via pattern dispatch");
    int rc = execute_create_clause(executor, create, result);
    if (rc >= 0) {
        result->success = true;
    }
    return rc;
}

static int handle_merge(cypher_executor *executor, cypher_query *query,
                        cypher_result *result, clause_flags flags)
{
    (void)flags;
    cypher_merge *merge = find_merge_clause(query);

    CYPHER_DEBUG("Executing MERGE via pattern dispatch");
    int rc = execute_merge_clause(executor, merge, result);
    if (rc >= 0) {
        result->success = true;
    }
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
        set_result_error(result, "MERGE+WITH pipeline: no MERGE clause before WITH");
        return -1;
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
            }
        }
    }

    /* Find post-WITH MATCH and MERGE clauses */
    cypher_match *post_match = NULL;
    cypher_merge *post_merge = NULL;
    cypher_return *post_return = NULL;
    for (int i = with_pos + 1; i < query->clauses->count; i++) {
        ast_node *clause = query->clauses->items[i];
        if (clause->type == AST_NODE_MATCH && !post_match) post_match = (cypher_match*)clause;
        if (clause->type == AST_NODE_MERGE && !post_merge) post_merge = (cypher_merge*)clause;
        if (clause->type == AST_NODE_RETURN && !post_return) post_return = (cypher_return*)clause;
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
                    append_sql(ctx, "%s.id AS %s_id", var->table_alias, var->name);
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
            bind_params_from_json(stmt, executor->params_json);
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
    } else if (post_return) {
        /* MERGE + WITH + RETURN: just return the MERGE'd variables */
        result->success = true;
        rc = 0;
    } else {
        set_result_error(result, "MERGE+WITH pipeline: unsupported post-WITH clause combination");
        rc = -1;
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

    /* For now, only handle list literals in UNWIND */
    if (unwind->expr->type != AST_NODE_LIST) {
        set_result_error(result, "UNWIND+CREATE currently only supports list literals");
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

        /* Execute CREATE for this iteration */
        if (execute_create_clause(executor, create, result) < 0) {
            g_foreach_ctx = prev_ctx;
            free_foreach_context(ctx);
            return -1;
        }
    }

    /* Restore previous context */
    g_foreach_ctx = prev_ctx;
    free_foreach_context(ctx);

    result->success = true;
    return 0;
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
