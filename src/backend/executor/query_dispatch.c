/*
 * query_dispatch.c
 *    Table-driven query pattern dispatch for Cypher execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "executor/query_patterns.h"
#include "gql_thread_local.h"
#include "entity_json_sql.h"
#include "executor/executor_internal.h"
#include "transform/sql_builder.h"
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
static bool unwind_before_create(cypher_query *query);
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
        /* GQLITE-T-0401: `WITH 42 AS x CREATE (:V {p: x})` — the CREATE
         * handler skipped the WITH, so x was unbound (and a WITH ... WHERE
         * did not filter). The WITH is a read prefix of the pipeline. */
        .name = "ROWWISE_WRITE (WITH+CREATE)",
        .required = CLAUSE_WITH | CLAUSE_CREATE,
        .forbidden = CLAUSE_MATCH | CLAUSE_UNWIND | CLAUSE_MERGE | CLAUSE_CALL |
                     CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV,
        .handler = handle_rowwise_write,
        .priority = 95
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
/* #136: true when an UNWIND or a WITH comes before the first write clause,
 * and the write part is a MATCH-bound write, or a MERGE followed by SET /
 * REMOVE / DELETE. Every clause after the first write must be one that the
 * row-wise pipeline runs. The generic transform lost $param values that
 * crossed a WITH (`WITH $v AS v MATCH (n) SET n.x = v` stored nothing). */
static bool unwind_feeds_write(cypher_query *query, clause_flags flags)
{
    if (!(flags & (CLAUSE_UNWIND | CLAUSE_WITH))) return false;
    if (flags & (CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN))
        return false;
    if (!query->clauses || query->clauses->count == 0) return false;
    ast_node *first = query->clauses->items[0];
    if (!first || !(first->type == AST_NODE_MATCH || first->type == AST_NODE_UNWIND ||
                    first->type == AST_NODE_WITH)) {
        return false;
    }
    bool unwind_seen = false, with_seen = false, delete_seen = false;
    bool write_seen = false, merge_seen = false, update_after_merge = false, agg_tail = false;
    bool early_write = false, with_after_write = false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        switch (c->type) {
            case AST_NODE_UNWIND:
                if (write_seen) return false;
                unwind_seen = true;
                break;
            case AST_NODE_MATCH:
                if (write_seen) return false;
                break;
            case AST_NODE_WITH:
                if (!write_seen) {
                    with_seen = true;
                } else {
                    with_after_write = true;
                    /* The pipeline aggregates in a tail WITH without
                     * ORDER BY / SKIP / LIMIT / DISTINCT. */
                    cypher_with *w = (cypher_with *)c;
                    bool plain = !w->order_by && !w->skip && !w->limit && !w->distinct;
                    for (int wi = 0; w->items && wi < w->items->count; wi++) {
                        cypher_return_item *it = (cypher_return_item *)w->items->items[wi];
                        if (it && aggregating_call_name(it->expr)) {
                            if (!plain) return false;
                            agg_tail = true;
                        }
                    }
                }
                break;
            case AST_NODE_RETURN:
                break;
            case AST_NODE_MERGE:
                merge_seen = true;
                write_seen = true;
                break;
            case AST_NODE_DELETE:
                delete_seen = true;
                /* fall through */
            case AST_NODE_SET:
            case AST_NODE_REMOVE:
                if (merge_seen) update_after_merge = true;
                write_seen = true;
                break;
            case AST_NODE_CREATE:
                write_seen = true;
                break;
            default:
                return false;
        }
        if (!unwind_seen && !with_seen && write_seen) early_write = true;
    }
    if (!write_seen) return false;
    /* A write straight after MATCH keeps its handler, unless a WITH
     * follows it: the read handlers dropped the write (`MATCH (n) REMOVE
     * n.x WITH n ...`, Remove3 / Set6). */
    if (early_write && !with_after_write) return false;
    (void)delete_seen;   /* the pipeline groups after a DELETE too (Delete6) */
    /* An aggregating WITH after the write: only the pipeline groups the
     * written rows (`UNWIND .. CREATE (n) WITH collect(n) AS ns`). */
    return (flags & CLAUSE_MATCH) || update_after_merge || agg_tail;
}

/* MATCH .. DELETE .. CREATE: the MATCH+DELETE handler ran CREATE first,
 * then matched again for the DELETE, so it deleted what CREATE had just
 * made (`MATCH (a)-[r]->(b) DELETE r CREATE (b)-[:R]->(a)`, Match5 [27]).
 * The row pipeline matches once and deletes before it creates. */
static bool delete_then_create(cypher_query *query, clause_flags flags)
{
    if (!(flags & CLAUSE_MATCH) || !(flags & CLAUSE_DELETE)) return false;
    if (flags & (CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN |
                 CLAUSE_MERGE | CLAUSE_RETURN | CLAUSE_WITH | CLAUSE_UNWIND))
        return false;
    if (!query->clauses || query->clauses->count < 2) return false;
    /* Also `MATCH (n) OPTIONAL MATCH (n)-[r]-() DELETE n, r` (r can be
     * null, Delete2 [2]) and `MATCH () CREATE (n) DELETE n` (Delete4 [3]). */
    bool write_seen = false, delete_seen = false, create_seen = false, optional_seen = false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        if (c->type == AST_NODE_MATCH) {
            if (write_seen) return false;
            if (((cypher_match*)c)->optional) optional_seen = true;
            continue;
        }
        if (c->type == AST_NODE_DELETE) { delete_seen = write_seen = true; continue; }
        if (c->type == AST_NODE_CREATE) { create_seen = write_seen = true; continue; }
        if (c->type == AST_NODE_SET) { write_seen = true; continue; }
        return false;
    }
    return delete_seen && (create_seen || optional_seen);
}

/* DELETE of a path variable or an expression (`MATCH p = ()-->() DETACH
 * DELETE p`, `DELETE list[0]`): the MATCH+DELETE handler deletes only node
 * and relationship variables (Delete3 [1]). The row pipeline deletes each
 * entity the value holds. */
static bool delete_of_value(cypher_query *query, clause_flags flags)
{
    if (!(flags & CLAUSE_DELETE) || !(flags & CLAUSE_MATCH)) return false;
    if (flags & (CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN))
        return false;
    if (!query->clauses || !query->clauses->items[0] ||
        query->clauses->items[0]->type != AST_NODE_MATCH) return false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c || c->type != AST_NODE_DELETE) continue;
        cypher_delete *d = (cypher_delete*)c;
        for (int k = 0; d->items && k < d->items->count; k++) {
            cypher_delete_item *it = (cypher_delete_item*)d->items->items[k];
            if (!it) continue;
            if (it->expr) return true;
            for (int j = 0; it->variable && j < i; j++) {
                ast_node *m = query->clauses->items[j];
                if (!m || m->type != AST_NODE_MATCH) continue;
                ast_list *pat = ((cypher_match*)m)->pattern;
                for (int q = 0; pat && q < pat->count; q++) {
                    ast_node *pi = pat->items[q];
                    if (pi && pi->type == AST_NODE_PATH && ((cypher_path*)pi)->var_name &&
                        strcmp(((cypher_path*)pi)->var_name, it->variable) == 0) return true;
                }
            }
        }
    }
    return false;
}

/* Write shapes the per-pattern handlers lost part of (GQLITE-T-0429, side
 * effects): a MERGE after a WITH or a MATCH (`WITH 42 AS v MERGE (:N {v: v})`
 * stored no property; `MATCH (p) MERGE (:C {name: p.x})` merged once), a
 * query with two or more writes where one is a MERGE (`MERGE (a) MERGE (b)
 * MERGE (a)-[:R]->(b)` made no relationship; `MERGE (t) CREATE ...` lost t's
 * label), and a MATCH after a write (`MATCH () CREATE () WITH * MATCH ()
 * CREATE ()` made one node). The row pipeline runs each row through every
 * clause. */
static bool writes_need_rows(cypher_query *query, clause_flags flags)
{
    if (flags & (CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN))
        return false;
    if (!query->clauses || query->clauses->count < 2) return false;
    int writes = 0, matches = 0;
    bool merge = false, read_before_merge = false, read_after_write = false, read_seen = false;
    bool only_unwind_read = true;   /* every read clause before the writes is an UNWIND */
    bool later_match_where = false; /* a WHERE on a second MATCH before the writes */
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        switch (c->type) {
            case AST_NODE_MATCH: case AST_NODE_WITH: case AST_NODE_UNWIND:
                if (writes > 0 && c->type == AST_NODE_MATCH) read_after_write = true;
                if (writes == 0 && c->type == AST_NODE_MATCH &&
                    matches++ > 0 && ((cypher_match *)c)->where) later_match_where = true;
                if (c->type != AST_NODE_UNWIND) only_unwind_read = false;
                read_seen = true;
                break;
            case AST_NODE_MERGE:
                if (read_seen) read_before_merge = true;
                merge = true;
                writes++;
                break;
            case AST_NODE_CREATE: case AST_NODE_SET: case AST_NODE_DELETE: case AST_NODE_REMOVE:
                writes++;
                break;
            case AST_NODE_RETURN:
                break;
            default:
                return false;
        }
    }
    if (writes == 0) return false;
    /* `UNWIND $rows AS r MERGE (n {id: r.id}) ON CREATE SET ...` keeps its
     * batch handler: it is correct there, and 28x faster than row by row
     * (GQLITE-T-0431). */
    if (writes == 1 && merge && only_unwind_read && !read_after_write) return false;
    /* The MATCH + write handlers bind each MATCH on its own: a second
     * MATCH's WHERE did not see the first one's variables (GQLITE-T-0469). */
    if (later_match_where) return true;
    return (merge && (read_before_merge || writes >= 2)) || read_after_write;
}

/* CALL { } with UNWIND / WITH before it, or with clauses after a leading
 * CALL: the CALL handler took its rows only from a MATCH, and dropped the
 * inner rows of `CALL { ... } RETURN ...` (GQLITE-T-0432). */
static bool rw_is_write_clause(ast_node_type t);

static bool call_in_rows(cypher_query *query, clause_flags flags)
{
    if (!(flags & CLAUSE_CALL)) return false;
    if (flags & (CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN)) return false;
    if (!query->clauses || query->clauses->count < 2) return false;
    int call_pos = -1, calls = 0;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        if (c->type == AST_NODE_CALL_SUBQUERY) { if (call_pos < 0) call_pos = i; calls++; }
    }
    if (call_pos < 0 || calls > 1) return false;
    for (int i = 0; i < call_pos; i++) {
        ast_node_type t = query->clauses->items[i]->type;
        if (t == AST_NODE_MATCH) return false;            /* the CALL handler */
        if (t != AST_NODE_UNWIND && t != AST_NODE_WITH) return false;
    }
    for (int i = call_pos + 1; i < query->clauses->count; i++) {
        ast_node_type t = query->clauses->items[i]->type;
        if (t != AST_NODE_RETURN && t != AST_NODE_WITH && t != AST_NODE_UNWIND &&
            t != AST_NODE_MATCH && !rw_is_write_clause(t)) return false;
    }
    return true;
}

static bool foreach_after_read(cypher_query *query, clause_flags flags)
{
    if (!(flags & CLAUSE_FOREACH)) return false;
    if (flags & (CLAUSE_CALL | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN)) return false;
    if (!query->clauses || query->clauses->count < 2) return false;
    ast_node *first = query->clauses->items[0];
    /* GQLITE-T-0423: a leading CREATE too (`CREATE (a) FOREACH (... |
     * CREATE (a)-[:R]->(:B))` ran the FOREACH without `a`). */
    if (!first || !(first->type == AST_NODE_MATCH || first->type == AST_NODE_UNWIND ||
                    first->type == AST_NODE_WITH || first->type == AST_NODE_CREATE)) {
        return false;
    }
    bool write_seen = false;
    for (int i = 0; i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        switch (c->type) {
            case AST_NODE_MATCH:
            case AST_NODE_UNWIND:
                if (write_seen) return false;
                break;
            case AST_NODE_WITH:
            case AST_NODE_RETURN:
                break;
            case AST_NODE_FOREACH:
            case AST_NODE_CREATE:
            case AST_NODE_MERGE:
            case AST_NODE_SET:
            case AST_NODE_REMOVE:
            case AST_NODE_DELETE:
                write_seen = true;
                break;
            default:
                return false;
        }
    }
    return true;
}

/* ---- An aggregate inside a list operation of RETURN --------------------
 * `RETURN all(ok IN collect(x) WHERE ok)` put the aggregate inside a
 * json_each() in a subquery's FROM, which SQLite refuses ("misuse of
 * aggregate"; List11 [3]). Rewrite the RETURN as `WITH collect(x) AS a, <keys>
 * RETURN all(ok IN a WHERE ok)`: the aggregates and the grouping keys move
 * to a WITH in front. Only for a RETURN without ORDER BY whose other items
 * keep their column names (an alias, a variable, or var.prop). */
static bool expr_has_list_op_aggregate(ast_node *e, bool in_list_op)
{
    if (!e) return false;
    if (in_list_op && aggregating_call_name(e)) return true;
    switch (e->type) {
        /* Only the list that is iterated: an aggregate in the per-element
         * part is an InvalidAggregation error (List12 [7]). */
        case AST_NODE_LIST_PREDICATE:
            return expr_has_list_op_aggregate(((cypher_list_predicate *)e)->list_expr, true);
        case AST_NODE_LIST_COMPREHENSION:
            return expr_has_list_op_aggregate(((cypher_list_comprehension *)e)->list_expr, true);
        case AST_NODE_REDUCE_EXPR:
            return expr_has_list_op_aggregate(((cypher_reduce_expr *)e)->list_expr, true);
        case AST_NODE_BINARY_OP:
            return expr_has_list_op_aggregate(((cypher_binary_op *)e)->left, in_list_op) ||
                   expr_has_list_op_aggregate(((cypher_binary_op *)e)->right, in_list_op);
        case AST_NODE_NOT_EXPR:
            return expr_has_list_op_aggregate(((cypher_not_expr *)e)->expr, in_list_op);
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *f = (cypher_function_call *)e;
            for (int i = 0; f->args && i < f->args->count; i++)
                if (expr_has_list_op_aggregate(f->args->items[i], in_list_op)) return true;
            return false;
        }
        default:
            return false;
    }
}

static bool expr_has_aggregate(ast_node *e);

/* Replace each aggregate call under e by `__gql_agg_N`, moving the call to
 * the WITH items. Returns the (possibly new) node. */
static ast_node *lift_aggregates(ast_node *e, ast_list *with_items, int *n)
{
    if (!e) return e;
    if (aggregating_call_name(e)) {
        char name[32];
        snprintf(name, sizeof(name), "__gql_agg_%d", (*n)++);
        ast_list_append(with_items, (ast_node *)make_return_item(e, name));
        return (ast_node *)make_identifier(name, 0);
    }
    switch (e->type) {
        case AST_NODE_LIST_PREDICATE: {
            cypher_list_predicate *lp = (cypher_list_predicate *)e;
            lp->list_expr = lift_aggregates(lp->list_expr, with_items, n);
            break;
        }
        case AST_NODE_LIST_COMPREHENSION: {
            cypher_list_comprehension *lc = (cypher_list_comprehension *)e;
            lc->list_expr = lift_aggregates(lc->list_expr, with_items, n);
            break;
        }
        case AST_NODE_REDUCE_EXPR: {
            cypher_reduce_expr *r = (cypher_reduce_expr *)e;
            r->list_expr = lift_aggregates(r->list_expr, with_items, n);
            break;
        }
        case AST_NODE_BINARY_OP: {
            cypher_binary_op *b = (cypher_binary_op *)e;
            b->left = lift_aggregates(b->left, with_items, n);
            b->right = lift_aggregates(b->right, with_items, n);
            break;
        }
        case AST_NODE_NOT_EXPR: {
            cypher_not_expr *ne = (cypher_not_expr *)e;
            ne->expr = lift_aggregates(ne->expr, with_items, n);
            break;
        }
        case AST_NODE_FUNCTION_CALL: {
            cypher_function_call *f = (cypher_function_call *)e;
            for (int i = 0; f->args && i < f->args->count; i++)
                f->args->items[i] = lift_aggregates(f->args->items[i], with_items, n);
            break;
        }
        default:
            break;
    }
    return e;
}

static bool expr_has_aggregate(ast_node *e)
{
    if (!e) return false;
    if (aggregating_call_name(e)) return true;
    return expr_has_list_op_aggregate(e, true) ||
           (e->type == AST_NODE_BINARY_OP &&
            (expr_has_aggregate(((cypher_binary_op *)e)->left) ||
             expr_has_aggregate(((cypher_binary_op *)e)->right))) ||
           (e->type == AST_NODE_NOT_EXPR && expr_has_aggregate(((cypher_not_expr *)e)->expr));
}

static void split_list_op_aggregates(cypher_query *query)
{
    if (!query || !query->clauses || query->clauses->count == 0) return;
    ast_node *last = query->clauses->items[query->clauses->count - 1];
    if (!last || last->type != AST_NODE_RETURN) return;
    cypher_return *ret = (cypher_return *)last;
    if (!ret->items || ret->return_all || ret->order_by) return;

    bool needed = false;
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *it = (cypher_return_item *)ret->items->items[i];
        if (!it || !it->expr) return;
        if (expr_has_list_op_aggregate(it->expr, false)) needed = true;
        else if (!expr_has_aggregate(it->expr) && !it->alias &&
                 it->expr->type != AST_NODE_IDENTIFIER &&
                 !(it->expr->type == AST_NODE_PROPERTY &&
                   ((cypher_property *)it->expr)->expr &&
                   ((cypher_property *)it->expr)->expr->type == AST_NODE_IDENTIFIER)) {
            return;   /* its column name would change */
        }
    }
    if (!needed) return;

    ast_list *with_items = ast_list_create();
    if (!with_items) return;
    int n_agg = 0;
    for (int i = 0; i < ret->items->count; i++) {
        cypher_return_item *it = (cypher_return_item *)ret->items->items[i];
        if (expr_has_aggregate(it->expr)) {
            it->expr = lift_aggregates(it->expr, with_items, &n_agg);
            continue;
        }
        /* A grouping key: computed in the WITH, read back by name. */
        char key[32];
        snprintf(key, sizeof(key), "__gql_key_%d", i);
        if (!it->alias) {
            char col[256];
            if (it->expr->type == AST_NODE_IDENTIFIER) {
                snprintf(col, sizeof(col), "%s", ((cypher_identifier *)it->expr)->name);
            } else {
                cypher_property *pr = (cypher_property *)it->expr;
                snprintf(col, sizeof(col), "%s.%s", ((cypher_identifier *)pr->expr)->name, pr->property_name);
            }
            it->alias = strdup(col);
        }
        ast_list_append(with_items, (ast_node *)make_return_item(it->expr, key));
        it->expr = (ast_node *)make_identifier(key, 0);
    }
    cypher_with *w = make_cypher_with(false, with_items, NULL, NULL, NULL, NULL);
    if (!w) return;
    ast_list_append(query->clauses, (ast_node *)w);
    /* the WITH goes before the RETURN */
    int c = query->clauses->count;
    query->clauses->items[c - 1] = (ast_node *)ret;
    query->clauses->items[c - 2] = (ast_node *)w;
}

/* Row values as named parameters (see rw_param_for_literal). */
typedef struct { char *json; int n; } rw_param_sink;
static char *rw_params_merged(const char *user, const rw_param_sink *sink);
static graph_algo_result *run_graph_algorithm(cypher_executor *executor, graph_algo_params algo_params);

/* ---- CALL name(args) [YIELD ...] (GQLITE-T-0389) -----------------------
 * The graph algorithms are the procedures. The algorithm runs in C; its
 * result objects become rows through
 *   UNWIND $__gql_algo_rows AS __gql_algo_row
 *   WITH __gql_algo_row.k AS a, ... [WHERE ...]
 * followed by the rest of the query (or a RETURN of the yielded names). */
/* The outputs of each algorithm, in order: YIELD is checked against them,
 * and YIELD * on an empty result still names them (GQLITE-T-0442). */
static const char *const *algo_outputs(graph_algo_type t)
{
    static const char *const node_score[] = {"node_id", "user_id", "score", NULL};
    static const char *const community[] = {"node_id", "user_id", "community", NULL};
    static const char *const component[] = {"node_id", "user_id", "component", NULL};
    static const char *const degree[] = {"node_id", "user_id", "in_degree", "out_degree", "degree", NULL};
    static const char *const triangles[] = {"node_id", "user_id", "triangles", "clustering_coefficient", NULL};
    static const char *const path[] = {"path", "distance", "found", NULL};
    static const char *const astar[] = {"path", "distance", "found", "nodes_explored", NULL};
    static const char *const search[] = {"node_id", "user_id", "depth", "order", NULL};
    static const char *const similarity[] = {"node1", "node2", "similarity", NULL};
    static const char *const knn[] = {"neighbor", "similarity", "rank", NULL};
    static const char *const apsp[] = {"source", "target", "distance", NULL};
    switch (t) {
        case GRAPH_ALGO_PAGERANK: case GRAPH_ALGO_BETWEENNESS_CENTRALITY:
        case GRAPH_ALGO_CLOSENESS_CENTRALITY: case GRAPH_ALGO_EIGENVECTOR_CENTRALITY: return node_score;
        case GRAPH_ALGO_LABEL_PROPAGATION: case GRAPH_ALGO_LOUVAIN: return community;
        case GRAPH_ALGO_WCC: case GRAPH_ALGO_SCC: return component;
        case GRAPH_ALGO_DEGREE_CENTRALITY: return degree;
        case GRAPH_ALGO_TRIANGLE_COUNT: return triangles;
        case GRAPH_ALGO_DIJKSTRA: return path;
        case GRAPH_ALGO_ASTAR: return astar;
        case GRAPH_ALGO_BFS: case GRAPH_ALGO_DFS: return search;
        case GRAPH_ALGO_NODE_SIMILARITY: return similarity;
        case GRAPH_ALGO_KNN: return knn;
        case GRAPH_ALGO_APSP: return apsp;
        default: return NULL;
    }
}

static int handle_call_procedure(cypher_executor *executor, cypher_query *query, cypher_result *result)
{
    ast_list *clauses = query->clauses;
    cypher_call_procedure *cp = (cypher_call_procedure*)clauses->items[0];
    for (int i = 1; i < clauses->count; i++) {
        if (clauses->items[i] && clauses->items[i]->type == AST_NODE_CALL_PROCEDURE) {
            set_result_error(result, "SyntaxError: InvalidClauseComposition: a procedure CALL must be the first clause");
            return -1;
        }
    }
    /* which algorithm: the detection reads a RETURN of the function call */
    ast_list *args = cp->args ? cp->args : ast_list_create();
    cypher_function_call *fc = make_function_call(cp->name, args, false, 0);
    ast_list *ritems = ast_list_create();
    ast_list_append(ritems, (ast_node*)make_return_item((ast_node*)fc, NULL));
    cypher_return *ret = make_cypher_return(false, ritems, NULL, NULL, NULL);
    graph_algo_params params = detect_graph_algorithm(ret, executor->params_json);
    fc->args = NULL;                       /* the arguments stay with the CALL */
    if (!cp->args) ast_list_free(args);
    ast_node_free((ast_node*)ret);
    if (params.type == GRAPH_ALGO_NONE) {
        char msg[300];
        snprintf(msg, sizeof(msg), "ProcedureError: ProcedureNotFound: there is no procedure %s", cp->name);
        set_result_error(result, msg);
        return -1;
    }
    graph_algo_result *ar = run_graph_algorithm(executor, params);
    if (!ar || !ar->success) {
        set_result_error(result, ar && ar->error_message ? ar->error_message : "Graph algorithm failed");
        if (ar) graph_algo_result_free(ar);
        return -1;
    }
    /* one row per result object (dijkstra and astar give one object) */
    char *rows = NULL;
    char **keys = NULL; int nk = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(executor->db,
            "SELECT CASE json_type(?1) WHEN 'array' THEN json(?1) WHEN 'object' THEN json_array(json(?1)) "
            "ELSE json('[]') END", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, ar->json_result ? ar->json_result : "[]", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
            rows = strdup((const char*)sqlite3_column_text(st, 0));
        sqlite3_finalize(st);
    }
    graph_algo_result_free(ar);
    if (!rows) { set_result_error(result, "CALL: could not read the algorithm result"); return -1; }
    if (sqlite3_prepare_v2(executor->db, "SELECT key FROM json_each(json_extract(?1, '$[0]'))", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, rows, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            char **grown = realloc(keys, (size_t)(nk + 1) * sizeof(char*));
            char *k = grown ? strdup((const char*)sqlite3_column_text(st, 0)) : NULL;
            if (grown) keys = grown;
            if (!k) break;
            keys[nk++] = k;
        }
        sqlite3_finalize(st);
    }
    /* The algorithm's declared outputs, whether or not there are rows. */
    const char *const *decl = algo_outputs(params.type);
    if (decl) {
        for (int k = 0; k < nk; k++) free(keys[k]);
        free(keys);
        keys = NULL; nk = 0;
        for (int k = 0; decl[k]; k++) {
            char **grown = realloc(keys, (size_t)(nk + 1) * sizeof(char*));
            char *dk = grown ? strdup(decl[k]) : NULL;
            if (grown) keys = grown;
            if (!dk) break;
            keys[nk++] = dk;
        }
    }
    int rc = 0;
    /* outputs: the YIELD items, or every key */
    ast_list *with_items = ast_list_create();
    ast_list *ret_items = ast_list_create();
    if (cp->yields && !cp->yield_all) {
        for (int i = 0; i < cp->yields->count && rc == 0; i++) {
            cypher_return_item *y = (cypher_return_item*)cp->yields->items[i];
            const char *out = ((cypher_identifier*)y->expr)->name;
            bool known = false;
            for (int k = 0; k < nk && !known; k++) known = strcmp(keys[k], out) == 0;
            if (!known) {
                char msg[300];
                snprintf(msg, sizeof(msg), "SyntaxError: UndefinedVariable: procedure %s has no output `%s`", cp->name, out);
                set_result_error(result, msg);
                rc = -1;
                break;
            }
            const char *alias = y->alias ? y->alias : out;
            ast_list_append(with_items, (ast_node*)make_return_item(
                (ast_node*)make_property((ast_node*)make_identifier("__gql_algo_row", 0), (char*)out, 0), (char*)alias));
            ast_list_append(ret_items, (ast_node*)make_return_item((ast_node*)make_identifier((char*)alias, 0), (char*)alias));
        }
    } else {
        for (int k = 0; k < nk; k++) {
            ast_list_append(with_items, (ast_node*)make_return_item(
                (ast_node*)make_property((ast_node*)make_identifier("__gql_algo_row", 0), keys[k], 0), keys[k]));
            ast_list_append(ret_items, (ast_node*)make_return_item((ast_node*)make_identifier(keys[k], 0), keys[k]));
        }
    }
    for (int k = 0; k < nk; k++) free(keys[k]);
    free(keys);
    if (rc == 0 && with_items->count == 0) {
        /* no rows and no outputs to name: an empty result */
        result->success = true;
        ast_list_free(with_items); ast_list_free(ret_items); free(rows);
        return 0;
    }
    if (rc < 0) { ast_list_free(with_items); ast_list_free(ret_items); free(rows); return -1; }

    ast_list *made = ast_list_create();        /* clauses made here */
    ast_list_append(made, (ast_node*)make_cypher_unwind((ast_node*)make_parameter("__gql_algo_rows", 0), "__gql_algo_row", 0));
    ast_list_append(made, (ast_node*)make_cypher_with(false, with_items, NULL, NULL, NULL, cp->where));
    bool own_return = clauses->count == 1;
    if (own_return) ast_list_append(made, (ast_node*)make_cypher_return(false, ret_items, NULL, NULL, NULL));
    else ast_list_free(ret_items);
    ast_list *all = ast_list_create();
    for (int i = 0; i < made->count; i++) ast_list_append(all, made->items[i]);
    for (int i = 1; i < clauses->count; i++) ast_list_append(all, clauses->items[i]);
    cypher_query *q = make_cypher_query(all, false);

    rw_param_sink sink = { NULL, 0 };
    sink.json = sqlite3_mprintf("\"__gql_algo_rows\":%s", rows);
    free(rows);
    const char *user_params = executor->params_json;
    char *merged = rw_params_merged(user_params, &sink);
    executor->params_json = merged;
    executor->stmt_capture = false;
    cypher_result *ir = cypher_executor_execute_ast(executor, (ast_node*)q);
    executor->params_json = user_params;
    sqlite3_free(merged);
    sqlite3_free(sink.json);

    if (!ir || !ir->success) {
        set_result_error(result, ir && ir->error_message ? ir->error_message : "CALL failed");
        rc = -1;
    } else {
        /* the rows of the rewritten query are the result */
        result->success = true;
        result->row_count = ir->row_count;
        result->column_count = ir->column_count;
        result->column_names = ir->column_names; ir->column_names = NULL;
        result->data = ir->data; ir->data = NULL;
        result->data_types = ir->data_types; ir->data_types = NULL;
        result->agtype_data = ir->agtype_data; ir->agtype_data = NULL;
        result->use_agtype = ir->use_agtype;
        result->nodes_created += ir->nodes_created;
        result->nodes_deleted += ir->nodes_deleted;
        result->relationships_created += ir->relationships_created;
        result->relationships_deleted += ir->relationships_deleted;
        result->properties_set += ir->properties_set;
    }
    if (ir) cypher_result_free(ir);
    /* free what was made here; the WHERE and the rest belong to the query */
    ((cypher_with*)made->items[1])->where = NULL;
    for (int i = 0; i < made->count; i++) ast_node_free(made->items[i]);
    free(made->items); free(made);
    free(all->items); free(all); free(q);
    return rc;
}

/* ---- LOAD CSV (GQLITE-T-0134) --------------------------------------------
 * The file is read and parsed here (RFC 4180: quoted fields, "" in quotes,
 * line breaks in quotes). Each line becomes a row: a map with HEADERS, a
 * list without. The rest of the query runs as
 *   UNWIND $__gql_csv_rows AS __gql_csv
 *   WITH __gql_csv.row AS <var>, __gql_csv.line AS __gql_linenumber, ...
 * so linenumber() and file() work in the clause after LOAD CSV. */

static void csv_json_append_string(dynamic_buffer *b, const char *t, size_t n)
{
    dbuf_append(b, "\"");
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c == '"' || c == '\\') { char e[3] = {'\\', (char)c, 0}; dbuf_append(b, e); }
        else if (c == '\n') dbuf_append(b, "\\n");
        else if (c == '\r') dbuf_append(b, "\\r");
        else if (c == '\t') dbuf_append(b, "\\t");
        else if (c < 0x20) dbuf_appendf(b, "\\u%04x", c);
        else { char e[2] = {(char)c, 0}; dbuf_append(b, e); }
    }
    dbuf_append(b, "\"");
}

typedef struct { char *text; size_t len; bool quoted; } csv_field;

/* One record from text at *pos. Returns the field count (fields owned),
 * -1 at end of input. */
/* The next record of t from *pos: its field count, -1 at the end, -2 for
 * a quote that is not closed (GQLITE-T-0443: the rest of the file became
 * one field), -3 when out of memory. */
static int csv_next_record(const char *t, size_t n, size_t *pos, char sep, csv_field **out)
{
    if (*pos >= n) return -1;
    int count = 0, cap = 8;
    csv_field *f = calloc((size_t)cap, sizeof(csv_field));
    if (!f) return -3;
    size_t i = *pos;
    for (;;) {
        dynamic_buffer v; dbuf_init(&v);
        bool quoted = false;
        if (i < n && t[i] == '"') {
            quoted = true;
            bool closed = false;
            i++;
            while (i < n) {
                if (t[i] == '"') {
                    if (i + 1 < n && t[i + 1] == '"') { dbuf_append(&v, "\""); i += 2; continue; }
                    i++;
                    closed = true;
                    break;
                }
                char c[2] = {t[i], 0};
                dbuf_append(&v, c);
                i++;
            }
            if (!closed) {
                dbuf_free(&v);
                for (int k = 0; k < count; k++) free(f[k].text);
                free(f);
                return -2;
            }
        }
        while (i < n && t[i] != sep && t[i] != '\n' && t[i] != '\r') {
            char c[2] = {t[i], 0};
            dbuf_append(&v, c);
            i++;
        }
        if (count == cap) {
            csv_field *grown = realloc(f, (size_t)cap * 2 * sizeof(csv_field));
            if (!grown) {
                dbuf_free(&v);
                for (int k = 0; k < count; k++) free(f[k].text);
                free(f);
                return -3;
            }
            f = grown;
            cap *= 2;
        }
        const char *got = dbuf_get(&v);
        f[count].text = strdup(got ? got : "");
        if (!f[count].text) {
            dbuf_free(&v);
            for (int k = 0; k < count; k++) free(f[k].text);
            free(f);
            return -3;
        }
        f[count].len = dbuf_len(&v);
        f[count].quoted = quoted;
        count++;
        dbuf_free(&v);
        if (i < n && t[i] == sep) { i++; continue; }
        if (i < n && t[i] == '\r') i++;
        if (i < n && t[i] == '\n') i++;
        break;
    }
    *pos = i;
    *out = f;
    return count;
}

static void csv_free_record(csv_field *f, int n)
{
    for (int i = 0; i < n; i++) free(f[i].text);
    free(f);
}

static int handle_load_csv(cypher_executor *executor, cypher_query *query, cypher_result *result)
{
    ast_list *clauses = query->clauses;
    cypher_load_csv *lc = (cypher_load_csv*)clauses->items[0];
    const char *path = lc->file_path ? lc->file_path : "";
    if (strncmp(path, "file://", 7) == 0) path += 7;
    if (strstr(path, "://")) {
        set_result_error(result, "LOAD CSV: only local files are supported (a path or file://)");
        return -1;
    }
    char sep = lc->fieldterminator && lc->fieldterminator[0] ? lc->fieldterminator[0] : ',';
    if (lc->fieldterminator && strcmp(lc->fieldterminator, "\\t") == 0) sep = '\t';
    else if (lc->fieldterminator && strlen(lc->fieldterminator) != 1) {
        /* only its first character was used (GQLITE-T-0443) */
        set_result_error(result, "LOAD CSV: FIELDTERMINATOR must be one character");
        return -1;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        char msg[600];
        snprintf(msg, sizeof(msg), "LOAD CSV: cannot open %s", path);
        set_result_error(result, msg);
        return -1;
    }
    dynamic_buffer text; dbuf_init(&text);
    char chunk[65536];
    size_t got;
    bool has_nul = false;
    while ((got = fread(chunk, 1, sizeof(chunk) - 1, fp)) > 0) {
        /* a NUL byte cut the data short (GQLITE-T-0443) */
        if (memchr(chunk, 0, got)) { has_nul = true; break; }
        chunk[got] = 0;
        dbuf_append(&text, chunk);
    }
    fclose(fp);
    if (has_nul) {
        dbuf_free(&text);
        char msg[600];
        snprintf(msg, sizeof(msg), "LOAD CSV: %s contains a NUL byte", path);
        set_result_error(result, msg);
        return -1;
    }
    const char *t = dbuf_get(&text);
    size_t n = dbuf_len(&text);
    size_t pos = 0;
    if (n >= 3 && (unsigned char)t[0] == 0xEF && (unsigned char)t[1] == 0xBB && (unsigned char)t[2] == 0xBF) pos = 3;  /* BOM */

    csv_field *hdr = NULL; int nh = 0;
    int bad = 0, bad_line = 0;     /* -2 an unclosed quote, -3 out of memory */
    if (lc->with_headers) {
        nh = csv_next_record(t ? t : "", n, &pos, sep, &hdr);
        if (nh < -1) { bad = nh; bad_line = 1; }
        if (nh < 0) { nh = 0; hdr = NULL; }
    }
    dynamic_buffer rows; dbuf_init(&rows);
    dbuf_append(&rows, "[");
    int line = lc->with_headers ? 1 : 0, nrows = 0;
    csv_field *rec = NULL; int nf;
    while (!bad && (nf = csv_next_record(t ? t : "", n, &pos, sep, &rec)) >= 0) {
        line++;
        if (nf == 1 && rec[0].len == 0 && !rec[0].quoted) { csv_free_record(rec, nf); continue; }  /* blank line */
        dbuf_append(&rows, nrows++ ? ",{\"line\":" : "{\"line\":");
        dbuf_appendf(&rows, "%d,\"row\":", line);
        if (lc->with_headers) {
            dbuf_append(&rows, "{");
            for (int i = 0; i < nh; i++) {
                if (i) dbuf_append(&rows, ",");
                csv_json_append_string(&rows, hdr[i].text, hdr[i].len);
                dbuf_append(&rows, ":");
                /* a missing or empty (unquoted) field is null */
                if (i >= nf || (rec[i].len == 0 && !rec[i].quoted)) dbuf_append(&rows, "null");
                else csv_json_append_string(&rows, rec[i].text, rec[i].len);
            }
            dbuf_append(&rows, "}");
        } else {
            dbuf_append(&rows, "[");
            for (int i = 0; i < nf; i++) {
                if (i) dbuf_append(&rows, ",");
                if (rec[i].len == 0 && !rec[i].quoted) dbuf_append(&rows, "null");
                else csv_json_append_string(&rows, rec[i].text, rec[i].len);
            }
            dbuf_append(&rows, "]");
        }
        dbuf_append(&rows, "}");
        csv_free_record(rec, nf);
    }
    if (!bad && nf < -1) { bad = nf; bad_line = line + 1; }
    dbuf_append(&rows, "]");
    if (hdr) csv_free_record(hdr, nh);
    dbuf_free(&text);
    if (bad) {
        dbuf_free(&rows);
        char msg[600];
        if (bad == -2)
            snprintf(msg, sizeof(msg), "LOAD CSV: a quoted field is not closed (record %d of %s)", bad_line, path);
        else
            snprintf(msg, sizeof(msg), "LOAD CSV: out of memory reading %s", path);
        set_result_error(result, msg);
        return -1;
    }

    /* the rest of the query over the rows */
    ast_list *with_items = ast_list_create();
    ast_list_append(with_items, (ast_node*)make_return_item(
        (ast_node*)make_property((ast_node*)make_identifier("__gql_csv", 0), "row", 0), lc->variable));
    ast_list_append(with_items, (ast_node*)make_return_item(
        (ast_node*)make_property((ast_node*)make_identifier("__gql_csv", 0), "line", 0), "__gql_linenumber"));
    ast_list_append(with_items, (ast_node*)make_return_item(
        (ast_node*)make_string_literal((char*)path, 0), "__gql_file"));
    ast_list *made = ast_list_create();
    ast_list_append(made, (ast_node*)make_cypher_unwind((ast_node*)make_parameter("__gql_csv_rows", 0), "__gql_csv", 0));
    ast_list_append(made, (ast_node*)make_cypher_with(false, with_items, NULL, NULL, NULL, NULL));
    ast_list *all = ast_list_create();
    for (int i = 0; i < made->count; i++) ast_list_append(all, made->items[i]);
    for (int i = 1; i < clauses->count; i++) ast_list_append(all, clauses->items[i]);
    cypher_query *q = make_cypher_query(all, false);

    rw_param_sink sink = { NULL, 0 };
    sink.json = sqlite3_mprintf("\"__gql_csv_rows\":%s", dbuf_get(&rows));
    dbuf_free(&rows);
    const char *user_params = executor->params_json;
    char *merged = sink.json ? rw_params_merged(user_params, &sink) : NULL;
    if (!merged) {
        /* rows over SQLite's string limit: zero rows and success before
         * (GQLITE-T-0443) */
        sqlite3_free(sink.json);
        for (int i = 0; i < made->count; i++) ast_node_free(made->items[i]);
        free(made->items); free(made);
        free(all->items); free(all); free(q);
        char msg[600];
        snprintf(msg, sizeof(msg), "LOAD CSV: %s is too large to load", path);
        set_result_error(result, msg);
        return -1;
    }
    executor->params_json = merged;
    executor->stmt_capture = false;
    cypher_result *ir = cypher_executor_execute_ast(executor, (ast_node*)q);
    executor->params_json = user_params;
    sqlite3_free(merged);
    sqlite3_free(sink.json);

    int rc = 0;
    if (!ir || !ir->success) {
        set_result_error(result, ir && ir->error_message ? ir->error_message : "LOAD CSV failed");
        rc = -1;
    } else {
        result->success = true;
        result->row_count = ir->row_count;
        result->column_count = ir->column_count;
        result->column_names = ir->column_names; ir->column_names = NULL;
        result->data = ir->data; ir->data = NULL;
        result->data_types = ir->data_types; ir->data_types = NULL;
        result->agtype_data = ir->agtype_data; ir->agtype_data = NULL;
        result->use_agtype = ir->use_agtype;
        result->nodes_created += ir->nodes_created;
        result->nodes_deleted += ir->nodes_deleted;
        result->relationships_created += ir->relationships_created;
        result->relationships_deleted += ir->relationships_deleted;
        result->properties_set += ir->properties_set;
    }
    if (ir) cypher_result_free(ir);
    for (int i = 0; i < made->count; i++) ast_node_free(made->items[i]);
    free(made->items); free(made);
    free(all->items); free(all); free(q);
    return rc;
}

int dispatch_query_pattern(cypher_executor *executor, cypher_query *query,
                           cypher_result *result)
{
    split_list_op_aggregates(query);

    /* Analyze query clauses */
    clause_flags flags = analyze_query_clauses(query);

    CYPHER_DEBUG("Query clauses: %s", clause_flags_to_string(flags));

    if (query->clauses && query->clauses->count > 0 && query->clauses->items[0] &&
        query->clauses->items[0]->type == AST_NODE_CALL_PROCEDURE) {
        CYPHER_DEBUG("Matched shape: CALL of a procedure");
        return handle_call_procedure(executor, query, result);
    }
    if (query->clauses && query->clauses->count > 0 && query->clauses->items[0] &&
        query->clauses->items[0]->type == AST_NODE_LOAD_CSV) {
        CYPHER_DEBUG("Matched shape: LOAD CSV");
        return handle_load_csv(executor, query, result);
    }

    /* #136: an UNWIND that feeds a MATCH + write, or a MERGE + SET, runs
     * row by row: the per-pattern handlers drop the UNWIND variable (SET saw
     * `row` as unbound) or the UNWIND itself (every node was updated). */
    /* A procedure CALL after other clauses fell through to the generic path
     * and failed with "`x` is not defined" for its YIELD (GQLITE-T-0447). */
    for (int i = 1; query->clauses && i < query->clauses->count; i++) {
        if (query->clauses->items[i] && query->clauses->items[i]->type == AST_NODE_CALL_PROCEDURE) {
            set_result_error(result, "SyntaxError: InvalidClauseComposition: a procedure CALL must be the first clause");
            return -1;
        }
    }

    if (call_in_rows(query, flags)) {
        CYPHER_DEBUG("Matched shape: CALL { } in the row pipeline");
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

    if (writes_need_rows(query, flags)) {
        CYPHER_DEBUG("Matched shape: writes that need the row pipeline");
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

    if (delete_of_value(query, flags)) {
        CYPHER_DEBUG("Matched shape: DELETE of a path or an expression (row-wise)");
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

    if (delete_then_create(query, flags)) {
        CYPHER_DEBUG("Matched shape: MATCH .. DELETE .. CREATE (row-wise)");
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

    if (unwind_feeds_write(query, flags)) {
        CYPHER_DEBUG("Matched shape: UNWIND feeding a write (row-wise)");
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

    /* A leading MERGE with a RETURN: every match of the pattern is a row
     * (`MERGE (b) RETURN count(*)` counted one row for two nodes). */
    if (query->clauses && query->clauses->count >= 2 && query->clauses->items[0] &&
        query->clauses->items[0]->type == AST_NODE_MERGE && (flags & CLAUSE_RETURN) &&
        !(flags & (CLAUSE_CALL | CLAUSE_FOREACH | CLAUSE_UNION | CLAUSE_LOAD_CSV | CLAUSE_EXPLAIN))) {
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

    /* #135: FOREACH after a MATCH / UNWIND / WITH runs per row of that
     * prefix; the FOREACH handler ignored the prefix. */
    if (foreach_after_read(query, flags)) {
        CYPHER_DEBUG("Matched shape: FOREACH after a read prefix (row-wise)");
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }

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
            result->column_count = gql_visible_column_count(transform_result->stmt);

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
 * GQLITE-T-0393: RETURN after SET or REMOVE projects the rows the MATCH
 * bound before the write, read after it.
 *
 * Before the write, every node and fixed-length relationship of the pattern
 * gets a variable (anonymous ones a temporary name) and the id of each is
 * read per row. After the write, RETURN runs over the same pattern with the
 * WHERE, labels and inline properties of those elements replaced by an id
 * filter: the write may have changed what they test. The rows are the
 * original rows, one for one.
 */
typedef struct {
    char *var;               /* variable of the element */
    bool named_here;         /* var is a temporary name set by row_pin */
    cypher_node_pattern *node;
    cypher_rel_pattern *rel;
    ast_list *saved_labels;
    ast_list *saved_alts;     /* `:A|B` (GQLITE-T-0439) */
    ast_node *saved_props;
} pinned_elem;

typedef struct {
    pinned_elem *elems;
    int count;
    char **vars;             /* distinct variables, in capture column order */
    int var_count;
    char ***ids;             /* ids[row][var] */
    int row_count;
    bool captured;
} row_pin;

static void row_pin_add_var(row_pin *pin, const char *var)
{
    for (int i = 0; i < pin->var_count; i++) {
        if (strcmp(pin->vars[i], var) == 0) return;
    }
    char **grown = realloc(pin->vars, (size_t)(pin->var_count + 1) * sizeof(char *));
    if (!grown) return;
    pin->vars = grown;
    pin->vars[pin->var_count++] = strdup(var);
}

static void row_pin_prepare(row_pin *pin, cypher_match *match)
{
    memset(pin, 0, sizeof(*pin));
    if (!match || !match->pattern) return;
    int cap = 0;
    for (int pi = 0; pi < match->pattern->count; pi++) {
        cypher_path *path = (cypher_path *)match->pattern->items[pi];
        if (!path || path->base.type != AST_NODE_PATH || !path->elements) continue;
        cap += path->elements->count;
    }
    if (cap == 0) return;
    pin->elems = calloc((size_t)cap, sizeof(pinned_elem));
    if (!pin->elems) return;

    for (int pi = 0; pi < match->pattern->count; pi++) {
        cypher_path *path = (cypher_path *)match->pattern->items[pi];
        if (!path || path->base.type != AST_NODE_PATH || !path->elements) continue;
        for (int ei = 0; ei < path->elements->count; ei++) {
            ast_node *el = path->elements->items[ei];
            if (!el) continue;
            char **varp = NULL;
            pinned_elem *pe = &pin->elems[pin->count];
            if (el->type == AST_NODE_NODE_PATTERN) {
                pe->node = (cypher_node_pattern *)el;
                varp = &pe->node->variable;
            } else if (el->type == AST_NODE_REL_PATTERN) {
                pe->rel = (cypher_rel_pattern *)el;
                /* A variable-length relationship is a list, not one id; its
                 * endpoints are pinned and its own filters stay. */
                if (pe->rel->varlen) { pe->rel = NULL; continue; }
                varp = &pe->rel->variable;
            } else {
                continue;
            }
            if (!*varp) {
                char name[32];
                snprintf(name, sizeof(name), "_gql_pin_%d", pin->count);
                *varp = strdup(name);
                pe->named_here = true;
            }
            pe->var = *varp;
            row_pin_add_var(pin, pe->var);
            pin->count++;
        }
    }
}

/* Read the id of each pinned variable for every row the MATCH binds now. */
static void row_pin_capture(row_pin *pin, cypher_executor *executor, cypher_match *match)
{
    if (pin->var_count == 0) return;
    ast_list *items = ast_list_create();
    for (int i = 0; i < pin->var_count; i++) {
        ast_list *args = ast_list_create();
        ast_list_append(args, (ast_node *)make_identifier(pin->vars[i], 0));
        ast_node *call = (ast_node *)make_function_call("id", args, false, 0);
        ast_list_append(items, (ast_node *)make_return_item(call, pin->vars[i]));
    }
    cypher_return *ret = make_cypher_return(false, items, NULL, NULL, NULL);
    cypher_result *res = create_empty_result();
    if (ret && res && execute_match_return_query(executor, match, ret, res) >= 0 &&
        res->column_count == pin->var_count) {
        pin->ids = calloc((size_t)(res->row_count > 0 ? res->row_count : 1), sizeof(char **));
        if (pin->ids) {
            for (int r = 0; r < res->row_count; r++) {
                pin->ids[r] = calloc((size_t)pin->var_count, sizeof(char *));
                if (!pin->ids[r]) break;
                for (int c = 0; c < pin->var_count; c++) {
                    const char *v = res->data && res->data[r] ? res->data[r][c] : NULL;
                    pin->ids[r][c] = strdup(v ? v : "-1");
                }
                pin->row_count++;
            }
            pin->captured = (pin->row_count == res->row_count);
        }
    }
    if (res) cypher_result_free(res);
    if (ret) ast_node_free((ast_node *)ret);
}

/* id(v) IN [ids] for each variable, and for more than one variable the
 * row key toString(id(a)) + ',' + toString(id(b)) IN ['1,2', ...] so that
 * only the captured combinations match. */
static ast_node *row_pin_where(row_pin *pin)
{
    if (pin->row_count == 0) return (ast_node *)make_boolean_literal(false, 0);
    ast_node *where = NULL;
    for (int c = 0; c < pin->var_count; c++) {
        ast_list *vals = ast_list_create();
        for (int r = 0; r < pin->row_count; r++) {
            ast_list_append(vals, (ast_node *)make_integer_literal(strtoll(pin->ids[r][c], NULL, 10), 0));
        }
        ast_list *args = ast_list_create();
        ast_list_append(args, (ast_node *)make_identifier(pin->vars[c], 0));
        ast_node *in = (ast_node *)make_binary_op(BINARY_OP_IN,
            (ast_node *)make_function_call("id", args, false, 0),
            (ast_node *)make_list(vals, 0), 0);
        where = where ? (ast_node *)make_binary_op(BINARY_OP_AND, where, in, 0) : in;
    }
    if (pin->var_count > 1) {
        ast_node *key = NULL;
        for (int c = 0; c < pin->var_count; c++) {
            ast_list *id_args = ast_list_create();
            ast_list_append(id_args, (ast_node *)make_identifier(pin->vars[c], 0));
            ast_list *str_args = ast_list_create();
            ast_list_append(str_args, (ast_node *)make_function_call("id", id_args, false, 0));
            ast_node *part = (ast_node *)make_function_call("toString", str_args, false, 0);
            if (key) {
                key = (ast_node *)make_binary_op(BINARY_OP_ADD, key,
                                                 (ast_node *)make_string_literal(",", 0), 0);
                key = (ast_node *)make_binary_op(BINARY_OP_ADD, key, part, 0);
            } else {
                key = part;
            }
        }
        ast_list *keys = ast_list_create();
        for (int r = 0; r < pin->row_count; r++) {
            dynamic_buffer kb;
            dbuf_init(&kb);
            for (int c = 0; c < pin->var_count; c++) {
                if (c > 0) dbuf_append(&kb, ",");
                dbuf_append(&kb, pin->ids[r][c]);
            }
            ast_list_append(keys, (ast_node *)make_string_literal((char *)dbuf_get(&kb), 0));
            dbuf_free(&kb);
        }
        ast_node *in = (ast_node *)make_binary_op(BINARY_OP_IN, key,
                                                  (ast_node *)make_list(keys, 0), 0);
        where = (ast_node *)make_binary_op(BINARY_OP_AND, where, in, 0);
    }
    return where;
}

/* A MATCH over the captured rows (see row_pin): the same pattern, with the
 * labels and inline properties of the pinned elements switched off and the
 * WHERE replaced by the id filter. row_pin_end puts the pattern back. */
static cypher_match *row_pin_begin(row_pin *pin, cypher_match *match)
{
    for (int i = 0; i < pin->count; i++) {
        pinned_elem *pe = &pin->elems[i];
        if (pe->node) {
            pe->saved_labels = pe->node->labels;
            pe->saved_alts = pe->node->label_alternatives;
            pe->saved_props = pe->node->properties;
            pe->node->labels = NULL;
            pe->node->label_alternatives = NULL;
            pe->node->properties = NULL;
        } else if (pe->rel) {
            pe->saved_props = pe->rel->properties;
            pe->rel->properties = NULL;
        }
    }
    return make_cypher_match(match->pattern, row_pin_where(pin), match->optional,
                             match->from_graph);
}

static void row_pin_end(row_pin *pin, cypher_match *synth)
{
    if (synth) {
        if (synth->where) ast_node_free(synth->where);
        free(synth);
    }
    for (int i = 0; i < pin->count; i++) {
        pinned_elem *pe = &pin->elems[i];
        if (pe->node) {
            pe->node->labels = pe->saved_labels;
            pe->node->label_alternatives = pe->saved_alts;
            pe->node->properties = pe->saved_props;
        } else if (pe->rel) {
            pe->rel->properties = pe->saved_props;
        }
    }
}

/* Run RETURN over the captured rows. */
static int row_pin_return(row_pin *pin, cypher_executor *executor, cypher_match *match,
                          cypher_return *ret, cypher_result *result)
{
    cypher_match *synth = row_pin_begin(pin, match);
    int rc = synth ? execute_match_return_query(executor, synth, ret, result) : -1;
    row_pin_end(pin, synth);
    return rc;
}

static void row_pin_release(row_pin *pin)
{
    for (int i = 0; i < pin->count; i++) {
        pinned_elem *pe = &pin->elems[i];
        if (!pe->named_here) continue;
        if (pe->node) { free(pe->node->variable); pe->node->variable = NULL; }
        if (pe->rel) { free(pe->rel->variable); pe->rel->variable = NULL; }
    }
    for (int r = 0; r < pin->row_count; r++) {
        for (int c = 0; c < pin->var_count; c++) free(pin->ids[r][c]);
        free(pin->ids[r]);
    }
    free(pin->ids);
    for (int i = 0; i < pin->var_count; i++) free(pin->vars[i]);
    free(pin->vars);
    free(pin->elems);
    memset(pin, 0, sizeof(*pin));
}

/*
 * Pattern-specific handlers - wrap existing executor functions
 */

/* A SET value the MATCH + SET path evaluates itself: a literal, a
 * parameter, a variable, `n.key`, or a list / map of those. */
static bool set_value_plain(ast_node *v)
{
    if (!v) return true;
    switch (v->type) {
        case AST_NODE_LITERAL: case AST_NODE_PARAMETER: case AST_NODE_IDENTIFIER: return true;
        case AST_NODE_PROPERTY:
            return ((cypher_property *)v)->expr &&
                   ((cypher_property *)v)->expr->type == AST_NODE_IDENTIFIER;
        case AST_NODE_LIST: {
            ast_list *a = ((cypher_list *)v)->items;
            for (int i = 0; a && i < a->count; i++) if (!set_value_plain(a->items[i])) return false;
            return true;
        }
        case AST_NODE_MAP: {
            ast_list *a = ((cypher_map *)v)->pairs;
            for (int i = 0; a && i < a->count; i++)
                if (!set_value_plain(((cypher_map_pair *)a->items[i])->value)) return false;
            return true;
        }
        default: return false;
    }
}

/* Does a SET of the query take a value MATCH + SET cannot evaluate? It
 * refused CASE and `properties(n).s`, and stored nothing for `+=` maps
 * that read the node or for reverse() (GQLITE-T-0456). */
static bool set_values_need_pipeline(cypher_query *query)
{
    for (int c = 0; query->clauses && c < query->clauses->count; c++) {
        ast_node *cl = query->clauses->items[c];
        if (!cl || cl->type != AST_NODE_SET) continue;
        ast_list *items = ((cypher_set *)cl)->items;
        for (int i = 0; items && i < items->count; i++) {
            cypher_set_item *it = (cypher_set_item *)items->items[i];
            if (!it || !it->property || it->property->type == AST_NODE_LABEL_EXPR) continue;
            if (!set_value_plain(it->expr)) return true;
            /* `SET a += {k: a.s}` reads the node it writes */
            if (it->is_merge && it->expr && it->expr->type == AST_NODE_MAP) {
                ast_list *a = ((cypher_map *)it->expr)->pairs;
                for (int k = 0; a && k < a->count; k++) {
                    ast_node *pv = ((cypher_map_pair *)a->items[k])->value;
                    if (pv && pv->type != AST_NODE_LITERAL && pv->type != AST_NODE_PARAMETER) return true;
                }
            }
        }
    }
    return false;
}

static int handle_match_set(cypher_executor *executor, cypher_query *query,
                            cypher_result *result, clause_flags flags)
{
    if (set_values_need_pipeline(query)) {
        executor->stmt_capture = false;
        return handle_rowwise_write(executor, query, result, flags);
    }
    cypher_match *match = find_match_clause(query);
    cypher_set *set = find_set_clause(query);

    int match_count = 0;
    for (int i = 0; i < query->clauses->count; i++) {
        if (query->clauses->items[i]->type == AST_NODE_MATCH) match_count++;
    }

    CYPHER_DEBUG("Executing MATCH+SET via pattern dispatch (match_count=%d)", match_count);

    /* GQLITE-T-0393: pin the rows before the write; RETURN reads them after. */
    row_pin pin;
    memset(&pin, 0, sizeof(pin));
    cypher_return *pin_ret = (flags & CLAUSE_RETURN) ? find_return_clause(query) : NULL;
    if (pin_ret && match_count == 1) {
        row_pin_prepare(&pin, match);
        row_pin_capture(&pin, executor, match);
    }

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
        /* GQLITE-T-0405: every SET and REMOVE clause runs, in order. The
         * first runs on the MATCH; the others on the pinned rows, since an
         * earlier write may change what the MATCH tests. */
        int writes = 0;
        for (int i = 0; i < query->clauses->count; i++) {
            ast_node_type t = query->clauses->items[i]->type;
            if (t == AST_NODE_SET || t == AST_NODE_REMOVE) writes++;
        }
        if (writes > 1 && !pin.elems) {
            row_pin_prepare(&pin, match);
            row_pin_capture(&pin, executor, match);
        }
        rc = 0;
        bool first = true;
        for (int i = 0; i < query->clauses->count && rc >= 0; i++) {
            ast_node *clause = query->clauses->items[i];
            if (clause->type != AST_NODE_SET && clause->type != AST_NODE_REMOVE) continue;
            cypher_match *synth = (!first && pin.captured) ? row_pin_begin(&pin, match) : NULL;
            cypher_match *m = synth ? synth : match;
            if (clause->type == AST_NODE_SET) {
                rc = execute_match_set_query(executor, m, (cypher_set *)clause, result);
            } else {
                rc = execute_match_remove_query(executor, m, (cypher_remove *)clause, result);
            }
            if (synth) row_pin_end(&pin, synth);
            first = false;
        }
    }

    if (rc >= 0) {
        result->success = true;
        if (flags & CLAUSE_RETURN) {
            cypher_return *ret = find_return_clause(query);
            if (ret && pin.captured) {
                /* SET may change what the WHERE or the inline properties
                 * test (`WHERE n.name = 'Andres' SET n.name = 'Michael'
                 * RETURN n`), so RETURN reads the pinned rows. */
                rc = row_pin_return(&pin, executor, match, ret, result);
            } else if (ret) {
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
    row_pin_release(&pin);
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

    /* GQLITE-T-0393: REMOVE may take away a label or property the pattern
     * or the WHERE tests (T-0315), so RETURN reads the rows pinned before
     * the write. */
    row_pin pin;
    memset(&pin, 0, sizeof(pin));
    cypher_return *ret = (flags & CLAUSE_RETURN) ? find_return_clause(query) : NULL;
    if (ret) {
        row_pin_prepare(&pin, match);
        row_pin_capture(&pin, executor, match);
    }

    int rc = execute_match_remove_query(executor, match, remove, result);
    if (rc >= 0) {
        result->success = true;
        if (ret && pin.captured) {
            rc = row_pin_return(&pin, executor, match, ret, result);
        } else if (ret) {
            cypher_match *synth = make_cypher_match(match->pattern, NULL,
                                                   match->optional,
                                                   match->from_graph);
            rc = execute_match_return_query(executor, synth ? synth : match,
                                            ret, result);
            if (synth) free(synth);
        }
    }
    row_pin_release(&pin);
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

/* A value the fast CREATE paths evaluate correctly: a literal, a
 * parameter, a variable, `var.key`, or a list / map / function call over
 * constants only. Any other expression (CASE, a list holding a variable,
 * toString(n.v), ...) needs the row's values, which only the row pipeline
 * has; the fast paths wrote null (GQLITE-T-0438). */
static bool create_value_is_simple(ast_node *v, bool constant_only)
{
    if (!v) return true;
    switch (v->type) {
        case AST_NODE_LITERAL: case AST_NODE_PARAMETER: return true;
        case AST_NODE_IDENTIFIER: return !constant_only;
        case AST_NODE_PROPERTY:
            return !constant_only && ((cypher_property *)v)->expr &&
                   ((cypher_property *)v)->expr->type == AST_NODE_IDENTIFIER;
        case AST_NODE_LIST: {
            ast_list *a = ((cypher_list *)v)->items;
            for (int i = 0; a && i < a->count; i++)
                if (!create_value_is_simple(a->items[i], true)) return false;
            return true;
        }
        case AST_NODE_MAP: {
            ast_list *a = ((cypher_map *)v)->pairs;
            for (int i = 0; a && i < a->count; i++)
                if (!create_value_is_simple(((cypher_map_pair *)a->items[i])->value, true)) return false;
            return true;
        }
        case AST_NODE_FUNCTION_CALL: {
            ast_list *a = ((cypher_function_call *)v)->args;
            for (int i = 0; a && i < a->count; i++)
                if (!create_value_is_simple(a->items[i], true)) return false;
            return true;
        }
        default: return false;
    }
}

static bool create_props_simple(ast_node *props)
{
    if (!props) return true;
    if (props->type != AST_NODE_MAP) return props->type == AST_NODE_PARAMETER;
    ast_list *a = ((cypher_map *)props)->pairs;
    for (int i = 0; a && i < a->count; i++)
        if (!create_value_is_simple(((cypher_map_pair *)a->items[i])->value, false)) return false;
    return true;
}

/* Does a CREATE (or MERGE) of the query set a property the fast paths
 * cannot evaluate? */
static bool write_props_need_pipeline(cypher_query *query, ast_node_type kind)
{
    for (int c = 0; query->clauses && c < query->clauses->count; c++) {
        ast_node *cl = query->clauses->items[c];
        if (!cl || cl->type != kind) continue;
        ast_list *pat = kind == AST_NODE_MERGE ? ((cypher_merge *)cl)->pattern
                                               : ((cypher_create *)cl)->pattern;
        for (int i = 0; pat && i < pat->count; i++) {
            ast_node *pn = pat->items[i];
            if (!pn || pn->type != AST_NODE_PATH) continue;
            ast_list *el = ((cypher_path *)pn)->elements;
            for (int j = 0; el && j < el->count; j++) {
                ast_node *e = el->items[j];
                if (!e) continue;
                if (e->type == AST_NODE_NODE_PATTERN &&
                    !create_props_simple(((cypher_node_pattern *)e)->properties)) return true;
                if (e->type == AST_NODE_REL_PATTERN &&
                    !create_props_simple(((cypher_rel_pattern *)e)->properties)) return true;
            }
        }
    }
    return false;
}

static bool create_props_need_pipeline(cypher_query *query)
{
    return write_props_need_pipeline(query, AST_NODE_CREATE);
}

/* The MERGE fast paths wrote no property for a computed value
 * (`UNWIND [1, 2] AS i MERGE (:N {k: i * 2})` merged every row into one
 * empty node; GQLITE-T-0461). */
static bool merge_props_need_pipeline(cypher_query *query)
{
    return write_props_need_pipeline(query, AST_NODE_MERGE);
}

static int handle_match_create(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    if (unwind_before_create(query) || create_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
    if (unwind_before_create(query) || create_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
    if (merge_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
/* GQLITE-T-0388: UNWIND before a MATCH ... CREATE. The MATCH+CREATE
 * handlers run CREATE per matched row without the UNWIND variable, so a
 * property such as {tag: s} was dropped; the row-wise pipeline keeps it. */
static bool unwind_before_create(cypher_query *query)
{
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        if (c->type == AST_NODE_CREATE) return false;
        if (c->type == AST_NODE_UNWIND) return true;
    }
    return false;
}

/* GQLITE-T-0424: the fast paths run one UNWIND, then one CREATE (and a
 * SET, and a WITH after it). Any other shape goes row by row: a clause
 * before the UNWIND (`CREATE (n) WITH n UNWIND ... CREATE ...` ran only
 * the first CREATE), a second CREATE, or a MERGE / DELETE / REMOVE. */
static bool unwind_create_needs_pipeline(cypher_query *query)
{
    int n_unwind = 0, n_create = 0;
    for (int i = 0; query->clauses && i < query->clauses->count; i++) {
        ast_node *c = query->clauses->items[i];
        if (!c) continue;
        switch (c->type) {
            case AST_NODE_UNWIND:
                if (i != 0 || n_create > 0) return true;
                n_unwind++;
                break;
            case AST_NODE_CREATE:
                n_create++;
                break;
            case AST_NODE_SET:
            case AST_NODE_RETURN:
                break;
            case AST_NODE_WITH:
                /* WITH ... WHERE / aggregation after the CREATE stays on the
                 * fast path (Create6); a WITH before it does not. */
                if (n_create == 0) return true;
                break;
            default:
                return true;
        }
    }
    return n_unwind >= 2 || n_create >= 2;
}

static int handle_unwind_create(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    if (unwind_create_needs_pipeline(query) || create_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
                 "SELECT value, type FROM json_each(json_extract(?, '$.%s'))", param->name);
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
            bind_foreach_item_json_each(ctx, unwind->alias, stmt, 0, 1);

            /* Execute CREATE, capturing the variable map */
            variable_map *create_vars = NULL;
            if (execute_create_clause_with_varmap(executor, create, result, &create_vars) < 0) {
                g_foreach_ctx = prev_ctx;
                free_foreach_context(ctx);
                sqlite3_finalize(stmt);
                return -1;
            }

            /* Execute SET if present, using variable map from CREATE */
            int set_rc = 0;
            if (set && create_vars) {
                set_rc = execute_set_operations(executor, set, create_vars, result);
            }
            if (create_vars) free_variable_map(create_vars);
            if (set_rc < 0) {
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
                    int set_rc = 0;
                    if (set && create_vars) {
                        set_rc = execute_set_operations(executor, set, create_vars, result);
                    }
                    if (create_vars) free_variable_map(create_vars);
                    if (set_rc < 0) {
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

        /* Bind the loop variable, keeping the item's type */
        if (bind_foreach_item_ast(ctx, unwind->alias, item) < 0) {
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
        int set_rc = 0;
        if (set && create_vars) {
            set_rc = execute_set_operations(executor, set, create_vars, result);
        }
        if (create_vars) free_variable_map(create_vars);
        if (set_rc < 0) {
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
 * UNWIND+MERGE handler - iterates over list/parameter and merges nodes per item
 */
static int handle_unwind_merge(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags)
{
    if (merge_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
                 "SELECT value, type FROM json_each(json_extract(?, '$.%s'))", param->name);
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
            bind_foreach_item_json_each(ctx, unwind->alias, stmt, 0, 1);

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
            if (bind_foreach_item_ast(ctx, unwind->alias, item) < 0) continue;

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

/* Run a graph algorithm in C (GQLITE-T-0389: also for CALL algo()). */
static graph_algo_result *run_graph_algorithm(cypher_executor *executor, graph_algo_params algo_params)
{
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

    /* the string parameters are ours: they were freed for Dijkstra only
     * (A*, BFS, DFS, similarity and KNN leaked them, GQLITE-T-0447) */
    free(algo_params.source_id);
    free(algo_params.target_id);
    free(algo_params.weight_prop);
    free(algo_params.lat_prop);
    free(algo_params.lon_prop);
    return algo_result;
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
        graph_algo_result *algo_result = run_graph_algorithm(executor, algo_params);

        if (algo_result) {
            if (algo_result->success) {
                result->column_count = 1;
                result->row_count = 1;
                result->data = malloc(sizeof(char**));
                result->data[0] = malloc(sizeof(char*));
                result->data[0][0] = strdup(algo_result->json_result);
                result->success = true;
                /* `RETURN pageRank() AS pr` names the column; without an
                 * alias it stays column_0, which the bindings read
                 * (GQLITE-T-0389). */
                cypher_return_item *it0 = ret && ret->items && ret->items->count > 0
                    ? (cypher_return_item*)ret->items->items[0] : NULL;
                if (it0 && it0->alias) {
                    result->column_names = malloc(sizeof(char*));
                    if (result->column_names) result->column_names[0] = strdup(it0->alias);
                }
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
            result->column_names[i] = strdup(item->alias ? item->alias :
                                              item->source_text ? item->source_text : "?column?");
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
            result->column_names[i] = strdup(item->source_text ? item->source_text : "?column?");
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
                            type_tables[t], id_col, entity_id, sql_esc_tmp(prop_name));

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
                    char b[64]; gql_format_double(b, sizeof(b), pv.as_real);
                    result->data[0][i] = strdup(b);
                    result->data_types[0][i] = SQLITE_FLOAT;
                } else if (pt == PROP_TYPE_BOOLEAN) {
                    result->data[0][i] = strdup(pv.as_bool ? "true" : "false");
                    result->data_types[0][i] = GQL_COL_TYPE_BOOLEAN;
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
    if (unwind_create_needs_pipeline(query) || create_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
        /* #136: $param and other list expressions go row by row. */
        return handle_rowwise_write(executor, query, result, flags);
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
        if (bind_foreach_item_ast(ctx, unwind->alias, item) < 0) {
            CYPHER_DEBUG("UNWIND+CREATE+RETURN: skipping item type %d", item->type);
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
    if (merge_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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
                if (bind_foreach_item_ast(ctx, unwind->alias, item) < 0) continue;
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
                 "SELECT value, type FROM json_each(json_extract(?, '$.%s'))", param->name);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            set_result_error(result, "Failed to prepare UNWIND parameter query");
            free(maps); g_foreach_ctx = prev_ctx; free_foreach_context(ctx); return -1;
        }
        sqlite3_bind_text(stmt, 1, executor->params_json, -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            bind_foreach_item_json_each(ctx, unwind->alias, stmt, 0, 1);
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
    if (merge_props_need_pipeline(query))
        return handle_rowwise_write(executor, query, result, flags);
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

/* The id of a rendered entity: a node {"id":N,"labels":[..],"properties":{..}}
 * or an edge {"id":N,"type":..,"startNode":..,..}. -1 for any other cell, so
 * a plain map such as {"id":"a","value":1} stays a value (#124). */
static long long rw_entity_id(const char *cell, bool *is_edge)
{
    if (!cell || strncmp(cell, "{\"id\":", 6) != 0) return -1;
    const char *p = cell + 6;
    if (*p < '0' || *p > '9') return -1;
    char *end = NULL;
    long long id = strtoll(p, &end, 10);
    if (!end || *end != ',') return -1;
    if (!strstr(cell, "\"properties\":")) return -1;
    if (strncmp(end, ",\"labels\":", 10) == 0) { *is_edge = false; return id; }
    if (strncmp(end, ",\"type\":", 8) == 0 && strstr(cell, "\"startNode\":")) { *is_edge = true; return id; }
    return -1;
}

static bool rw_is_write_clause(ast_node_type t)
{
    return t == AST_NODE_CREATE || t == AST_NODE_MERGE || t == AST_NODE_DELETE ||
           t == AST_NODE_SET || t == AST_NODE_REMOVE || t == AST_NODE_FOREACH;
}

static bool text_is_json_value(sqlite3 *db, const char *text)
{
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db, "SELECT json_valid(?)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, text, -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(st) == SQLITE_ROW && sqlite3_column_int(st, 0) == 1;
        sqlite3_finalize(st);
    }
    return ok;
}

/* #135: the row's scalars (UNWIND / WITH values) as FOREACH bindings, so a
 * FOREACH list or body can read them. */
static foreach_context *rw_row_foreach_ctx(cypher_executor *executor, const rw_row *row)
{
    foreach_context *ctx = create_foreach_context();
    if (!ctx) return NULL;
    for (int i = 0; i < row->n_scalars; i++) {
        const rw_scalar *sc = &row->scalars[i];
        if (!sc->text || sc->type == SQLITE_NULL) { set_foreach_binding_null(ctx, sc->name); continue; }
        switch (sc->type) {
            case SQLITE_INTEGER: set_foreach_binding_int(ctx, sc->name, strtoll(sc->text, NULL, 10)); break;
            case SQLITE_FLOAT:   set_foreach_binding_real(ctx, sc->name, strtod(sc->text, NULL)); break;
            case GQL_COL_TYPE_BOOLEAN:
                set_foreach_binding_bool(ctx, sc->name, strcmp(sc->text, "true") == 0 || strcmp(sc->text, "1") == 0);
                break;
            case GQL_COL_TYPE_STRING: set_foreach_binding_string(ctx, sc->name, sc->text); break;
            default:
                if ((sc->text[0] == '[' || sc->text[0] == '{') && text_is_json_value(executor->db, sc->text))
                    set_foreach_binding_json(ctx, sc->name, sc->text);
                else
                    set_foreach_binding_string(ctx, sc->name, sc->text);
                break;
        }
    }
    return ctx;
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
/* SQL literal for a scalar. Always sqlite3_mprintf memory: the caller
 * frees it with sqlite3_free. strdup memory freed that way crashed on
 * Linux, where SQLite's allocator keeps a size header ("free(): invalid
 * pointer"). */
static char *rw_scalar_sql(const rw_scalar *sc)
{
    if (!sc || !sc->text) return sqlite3_mprintf("NULL");
    if (sc->type == SQLITE_INTEGER || sc->type == SQLITE_FLOAT) return sqlite3_mprintf("%s", sc->text);
    /* GQLITE-A-0007: a boolean keeps its subtype; a known string is never
     * read as JSON. */
    if (sc->type == GQL_COL_TYPE_BOOLEAN)
        return sqlite3_mprintf("_gql_bool_str(%d)", strcmp(sc->text, "true") == 0 || strcmp(sc->text, "1") == 0);
    if (sc->type == GQL_COL_TYPE_STRING) return sqlite3_mprintf("%Q", sc->text);
    const char *t = sc->text;
    if ((t[0] == '[' || t[0] == '{')) return sqlite3_mprintf("json(%Q)", t);
    return sqlite3_mprintf("%Q", t);
}

/* Does `expr` reference only scalar bindings of `row` (and literals)? */
/* Names a list comprehension, list predicate or reduce binds inside
 * itself: neither row values nor entities (GQLITE-T-0461). */
static GQL_THREAD_LOCAL const char *rw_local_names[16];
static GQL_THREAD_LOCAL int rw_local_n = 0;

static bool rw_is_local(const char *n)
{
    for (int i = 0; n && i < rw_local_n; i++)
        if (strcmp(rw_local_names[i], n) == 0) return true;
    return false;
}

static bool rw_push_local(const char *n)
{
    if (!n || rw_local_n >= 16) return false;
    rw_local_names[rw_local_n++] = n;
    return true;
}

static void rw_pop_local(bool pushed)
{
    if (pushed) rw_local_n--;
}

static bool rw_expr_scalar_only(ast_node *expr, const rw_row *row)
{
    if (!expr) return true;
    switch (expr->type) {
        case AST_NODE_LITERAL: return true;
        case AST_NODE_PARAMETER: return true;
        case AST_NODE_IDENTIFIER:
            return rw_is_local(((cypher_identifier*)expr)->name) ||
                   rw_row_scalar(row, ((cypher_identifier*)expr)->name) != NULL;
        case AST_NODE_PROPERTY:   /* #124: row.key on an UNWIND map */
            return rw_expr_scalar_only(((cypher_property*)expr)->expr, row);
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
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *s = (cypher_subscript*)expr;
            return rw_expr_scalar_only(s->expr, row) && rw_expr_scalar_only(s->index, row) &&
                   rw_expr_scalar_only(s->slice_start, row) && rw_expr_scalar_only(s->slice_end, row);
        }
        /* CASE, NOT, IS NULL and map literals over the row's values: they
         * were left to the write executor, which has no row values, so
         * `CREATE (:S {f: CASE WHEN i = 1 ...})` wrote null (GQLITE-T-0438). */
        case AST_NODE_NOT_EXPR: return rw_expr_scalar_only(((cypher_not_expr*)expr)->expr, row);
        case AST_NODE_NULL_CHECK: return rw_expr_scalar_only(((cypher_null_check*)expr)->expr, row);
        case AST_NODE_CASE_EXPR: {
            cypher_case_expr *c = (cypher_case_expr*)expr;
            if (!rw_expr_scalar_only(c->operand, row) || !rw_expr_scalar_only(c->else_expr, row)) return false;
            for (int i = 0; c->when_clauses && i < c->when_clauses->count; i++) {
                cypher_when_clause *w = (cypher_when_clause*)c->when_clauses->items[i];
                if (!rw_expr_scalar_only(w->condition, row) || !rw_expr_scalar_only(w->result, row)) return false;
            }
            return true;
        }
        case AST_NODE_MAP: {
            cypher_map *m = (cypher_map*)expr;
            for (int i = 0; m->pairs && i < m->pairs->count; i++)
                if (!rw_expr_scalar_only(((cypher_map_pair*)m->pairs->items[i])->value, row)) return false;
            return true;
        }
        case AST_NODE_LIST_COMPREHENSION: {
            cypher_list_comprehension *lc = (cypher_list_comprehension*)expr;
            if (!rw_expr_scalar_only(lc->list_expr, row)) return false;
            bool p = rw_push_local(lc->variable);
            bool ok = rw_expr_scalar_only(lc->where_expr, row) && rw_expr_scalar_only(lc->transform_expr, row);
            rw_pop_local(p);
            return ok;
        }
        case AST_NODE_LIST_PREDICATE: {
            cypher_list_predicate *lp = (cypher_list_predicate*)expr;
            if (!rw_expr_scalar_only(lp->list_expr, row)) return false;
            bool p = rw_push_local(lp->variable);
            bool ok = rw_expr_scalar_only(lp->predicate, row);
            rw_pop_local(p);
            return ok;
        }
        case AST_NODE_REDUCE_EXPR: {
            cypher_reduce_expr *r = (cypher_reduce_expr*)expr;
            if (!rw_expr_scalar_only(r->initial_value, row) || !rw_expr_scalar_only(r->list_expr, row)) return false;
            bool p1 = rw_push_local(r->accumulator), p2 = rw_push_local(r->variable);
            bool ok = rw_expr_scalar_only(r->expression, row);
            rw_pop_local(p2); rw_pop_local(p1);
            return ok;
        }
        default: return false;
    }
}

/* Is expr a boolean by its shape: a boolean expression, or a CASE whose
 * results are all booleans or null (`CASE WHEN i = 1 THEN a.s IS NULL
 * END` gave 1 / 0, GQLITE-T-0438)? */
static bool rw_expr_yields_boolean(const ast_node *expr)
{
    if (!expr) return false;
    if (ast_is_boolean_expr(expr)) return true;
    if (expr->type != AST_NODE_CASE_EXPR) return false;
    const cypher_case_expr *c = (const cypher_case_expr *)expr;
    bool any = false;
    for (int i = 0; c->when_clauses && i < c->when_clauses->count; i++) {
        const ast_node *r = ((const cypher_when_clause *)c->when_clauses->items[i])->result;
        bool is_null = r && r->type == AST_NODE_LITERAL &&
                       ((const cypher_literal *)r)->literal_type == LITERAL_NULL;
        if (is_null) continue;
        if (!rw_expr_yields_boolean(r)) return false;
        any = true;
    }
    if (c->else_expr) {
        const ast_node *e = c->else_expr;
        bool is_null = e->type == AST_NODE_LITERAL &&
                       ((const cypher_literal *)e)->literal_type == LITERAL_NULL;
        if (!is_null) {
            if (!rw_expr_yields_boolean(e)) return false;
            any = true;
        }
    }
    return any;
}

/* Evaluate a scalar-only expression for a row. Returns the SQLite type;
 * *out_text is owned (NULL for SQL NULL). Returns -1 on failure. */
static int rw_eval_scalar(cypher_executor *executor, ast_node *expr, const rw_row *row, char **out_text)
{
    *out_text = NULL;
    cypher_transform_context *ctx = cypher_transform_create_context_ex(executor->db, false);
    if (!ctx) return -1;
    /* Each value is a numbered parameter (?100 + i), so the SQL text is the
     * same for every row and its prepared statement is reused
     * (GQLITE-T-0431); the wrapper keeps the type (a boolean, JSON). */
    for (int i = 0; i < row->n_scalars; i++) {
        const rw_scalar *sc = &row->scalars[i];
        char ref[64];
        int pn = 100 + i;
        if (!sc->text) snprintf(ref, sizeof(ref), "NULL");
        else if (sc->type == SQLITE_INTEGER || sc->type == SQLITE_FLOAT || sc->type == GQL_COL_TYPE_STRING)
            snprintf(ref, sizeof(ref), "?%d", pn);
        else if (sc->type == GQL_COL_TYPE_BOOLEAN) snprintf(ref, sizeof(ref), "_gql_bool_str(?%d)", pn);
        else if (sc->text[0] == '[' || sc->text[0] == '{') snprintf(ref, sizeof(ref), "json(?%d)", pn);
        else snprintf(ref, sizeof(ref), "?%d", pn);
        transform_var_register_projected(ctx->var_ctx, sc->name, ref);
        transform_var_set_bound(ctx->var_ctx, sc->name, true);
        transform_var_set_scalar_value(ctx->var_ctx, sc->name, true);
    }
    char *sql_expr = cypher_transform_capture_expression(ctx, expr);
    /* The value's type tag (GQLITE-A-0007): `coalesce(a.v > 0, 'n')` is a
     * boolean that SQL gives as 1 (GQLITE-T-0461). */
    char *tag_sql = sql_expr ? cypher_expr_tag_sql(ctx, expr, sql_expr) : NULL;
    cypher_transform_free_context(ctx);
    if (!sql_expr) { free(tag_sql); return -1; }
    char *sql = sqlite3_mprintf("SELECT %s, %s", sql_expr, tag_sql ? tag_sql : "NULL");
    free(sql_expr);
    free(tag_sql);
    sqlite3_stmt *st = sql ? cypher_executor_helper_stmt(executor, sql) : NULL;
    int type = -1;
    if (st) {
        for (int i = 0; i < row->n_scalars; i++) {
            const rw_scalar *sc = &row->scalars[i];
            if (!sc->text) continue;
            int pn = 100 + i;
            if (sc->type == SQLITE_INTEGER) sqlite3_bind_int64(st, pn, strtoll(sc->text, NULL, 10));
            else if (sc->type == SQLITE_FLOAT) sqlite3_bind_double(st, pn, strtod(sc->text, NULL));
            else if (sc->type == GQL_COL_TYPE_BOOLEAN)
                sqlite3_bind_int(st, pn, strcmp(sc->text, "true") == 0 || strcmp(sc->text, "1") == 0);
            else sqlite3_bind_text(st, pn, sc->text, -1, SQLITE_TRANSIENT);
        }
        if (executor->params_json) bind_params_from_json(st, executor->params_json);
        if (sqlite3_step(st) == SQLITE_ROW) {
            type = sqlite3_column_type(st, 0);
            sqlite3_value *cv = sqlite3_column_value(st, 0);
            if (cv && sqlite3_value_subtype(cv) == GQL_SUBTYPE_BOOLEAN) type = GQL_COL_TYPE_BOOLEAN;
            const char *v = (const char*)sqlite3_column_text(st, 0);
            const char *tag = (const char *)sqlite3_column_text(st, 1);
            bool tag_bool = tag && strcmp(tag, "bool") == 0;
            if (tag_bool && type == SQLITE_TEXT && v &&
                (strcmp(v, "true") == 0 || strcmp(v, "false") == 0)) {
                type = GQL_COL_TYPE_BOOLEAN;
            } else if (type == SQLITE_INTEGER && (tag_bool || rw_expr_yields_boolean(expr))) {
                /* a comparison is 1 / 0 in SQL: keep it a boolean
                 * (`MERGE (:C {k: i % 2 = 0})` stored 0 / 1) */
                type = GQL_COL_TYPE_BOOLEAN;
                v = sqlite3_column_int64(st, 0) ? "true" : "false";
            }
            *out_text = v ? strdup(v) : NULL;
        }
        sqlite3_reset(st);
    }
    sqlite3_free(sql);
    return type;
}

/* JSON text to a list or map literal AST, recursively (#136: maps and
 * nested lists were stored as strings). NULL when text is not a JSON
 * array or object. */
static ast_node *rw_json_to_ast(sqlite3 *db, const char *text, int depth)
{
    if (!text || depth > 64) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT type, value, key FROM json_each(?1) "
            "WHERE json_valid(?1) AND json_type(?1) IN ('array','object')",
            -1, &st, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_bind_text(st, 1, text, -1, SQLITE_TRANSIENT);
    bool is_map = text[0] == '{';
    ast_list *items = ast_list_create();
    bool ok = items != NULL;
    bool any_row = false;
    while (ok && sqlite3_step(st) == SQLITE_ROW) {
        any_row = true;
        const char *jt = (const char*)sqlite3_column_text(st, 0);
        const char *jv = (const char*)sqlite3_column_text(st, 1);
        ast_node *lit = NULL;
        if (!jt) { ok = false; break; }
        if (strcmp(jt, "integer") == 0) lit = (ast_node*)make_integer_literal(sqlite3_column_int64(st, 1), 0);
        else if (strcmp(jt, "real") == 0) lit = (ast_node*)make_decimal_literal(sqlite3_column_double(st, 1), 0);
        else if (strcmp(jt, "true") == 0) lit = (ast_node*)make_boolean_literal(true, 0);
        else if (strcmp(jt, "false") == 0) lit = (ast_node*)make_boolean_literal(false, 0);
        else if (strcmp(jt, "null") == 0) lit = (ast_node*)make_null_literal(0);
        else if (strcmp(jt, "text") == 0) lit = (ast_node*)make_string_literal((char*)(jv ? jv : ""), 0);
        else lit = rw_json_to_ast(db, jv, depth + 1);   /* array / object */
        if (!lit) { ok = false; break; }
        if (is_map) {
            const char *key = (const char*)sqlite3_column_text(st, 2);
            cypher_map_pair *pair = make_map_pair((char*)(key ? key : ""), lit, 0);
            if (!pair) { ast_node_free(lit); ok = false; break; }
            ast_list_append(items, (ast_node*)pair);
        } else {
            ast_list_append(items, lit);
        }
    }
    sqlite3_finalize(st);
    if (!ok || (!any_row && !(strcmp(text, "[]") == 0 || strcmp(text, "{}") == 0))) {
        /* An empty result is an empty list/map only for those texts;
         * otherwise the text was not valid JSON. */
        if (items) ast_list_free(items);
        return NULL;
    }
    ast_node *out = is_map ? (ast_node*)make_map(items, 0) : (ast_node*)make_list(items, 0);
    if (!out) ast_list_free(items);
    return out;
}

/* Literal AST for a scalar value (JSON arrays become list literals). */
static ast_node *rw_make_literal(cypher_executor *executor, const char *text, int type)
{
    if (!text || type == SQLITE_NULL) return (ast_node*)make_null_literal(0);
    /* A boolean cell stays a boolean (it was stored as the text 'true'). */
    if (type == GQL_COL_TYPE_BOOLEAN)
        return (ast_node*)make_boolean_literal(strcmp(text, "true") == 0 || strcmp(text, "1") == 0, 0);
    if (type == SQLITE_INTEGER) return (ast_node*)make_integer_literal(strtoll(text, NULL, 10), 0);
    if (type == SQLITE_FLOAT) return (ast_node*)make_decimal_literal(strtod(text, NULL), 0);
    if (type == GQL_COL_TYPE_STRING) return (ast_node*)make_string_literal((char*)text, 0);
    if (text[0] == '[' || text[0] == '{') {
        /* #136: a list or map keeps its shape, nested values too. */
        ast_node *lit = rw_json_to_ast(executor->db, text, 0);
        if (lit) return lit;
    }
    return (ast_node*)make_string_literal((char*)text, 0);
}

/* `row.key` where row holds a JSON map: a literal with the JSON type kept,
 * so true stays a boolean (SQL json_extract gives 1). NULL when expr is not
 * that shape; the caller then evaluates it through SQL. */
static ast_node *rw_map_property_literal(cypher_executor *executor, ast_node *expr, const rw_row *row)
{
    if (expr->type != AST_NODE_PROPERTY) return NULL;
    cypher_property *prop = (cypher_property*)expr;
    if (!prop->expr || prop->expr->type != AST_NODE_IDENTIFIER || !prop->property_name) return NULL;
    const rw_scalar *sc = rw_row_scalar(row, ((cypher_identifier*)prop->expr)->name);
    if (!sc || sc->type != SQLITE_TEXT || !sc->text || sc->text[0] != '{') return NULL;

    property_type type = PROP_TYPE_TEXT;
    property_value pv;
    property_value_init(&pv);
    int rc = json_map_property(executor->db, sc->text, prop->property_name, &type, &pv);
    ast_node *lit = NULL;
    if (rc == -2) {
        lit = (ast_node*)make_null_literal(0);
    } else if (rc == 0) {
        switch (type) {
            case PROP_TYPE_BOOLEAN: lit = (ast_node*)make_boolean_literal(pv.as_bool != 0, 0); break;
            case PROP_TYPE_INTEGER: lit = (ast_node*)make_integer_literal(pv.as_int, 0); break;
            case PROP_TYPE_REAL:    lit = (ast_node*)make_decimal_literal(pv.as_real, 0); break;
            case PROP_TYPE_TEXT:    lit = (ast_node*)make_string_literal(pv.as_str, 0); break;
            default:                lit = rw_make_literal(executor, pv.as_str, SQLITE_TEXT); break;
        }
    }
    property_value_free(&pv);
    return lit;
}

typedef struct { ast_node **slot; ast_node *original; ast_node *substitute; } rw_subst;

/* A literal for a scalar-only expression of the row, keeping its type.
 * NULL when expr is not scalar-only or cannot be evaluated. */
static void rw_eval_cell(cypher_executor *executor, ast_node *expr, rw_row *row, char **text, int *type);

/* Is every variable of expr bound by the row (a value or an entity)? */
static bool rw_expr_bound(ast_node *expr, const rw_row *row)
{
    if (!expr) return true;
    switch (expr->type) {
        case AST_NODE_LITERAL: case AST_NODE_PARAMETER: return true;
        case AST_NODE_IDENTIFIER: {
            const char *n = ((cypher_identifier*)expr)->name;
            return rw_is_local(n) || rw_row_scalar(row, n) ||
                   get_variable_node_id(row->vm, n) >= 0 || is_variable_edge(row->vm, n);
        }
        case AST_NODE_PROPERTY: return rw_expr_bound(((cypher_property*)expr)->expr, row);
        case AST_NODE_BINARY_OP:
            return rw_expr_bound(((cypher_binary_op*)expr)->left, row) && rw_expr_bound(((cypher_binary_op*)expr)->right, row);
        case AST_NODE_FUNCTION_CALL: {
            ast_list *a = ((cypher_function_call*)expr)->args;
            if (aggregating_call_name(expr)) return false;
            for (int i = 0; a && i < a->count; i++) if (!rw_expr_bound(a->items[i], row)) return false;
            return true;
        }
        case AST_NODE_LIST: {
            ast_list *a = ((cypher_list*)expr)->items;
            for (int i = 0; a && i < a->count; i++) if (!rw_expr_bound(a->items[i], row)) return false;
            return true;
        }
        case AST_NODE_NOT_EXPR: return rw_expr_bound(((cypher_not_expr*)expr)->expr, row);
        case AST_NODE_NULL_CHECK: return rw_expr_bound(((cypher_null_check*)expr)->expr, row);
        case AST_NODE_CASE_EXPR: {
            cypher_case_expr *c = (cypher_case_expr*)expr;
            if (!rw_expr_bound(c->operand, row) || !rw_expr_bound(c->else_expr, row)) return false;
            for (int i = 0; c->when_clauses && i < c->when_clauses->count; i++) {
                cypher_when_clause *w = (cypher_when_clause*)c->when_clauses->items[i];
                if (!rw_expr_bound(w->condition, row) || !rw_expr_bound(w->result, row)) return false;
            }
            return true;
        }
        case AST_NODE_MAP: {
            cypher_map *m = (cypher_map*)expr;
            for (int i = 0; m->pairs && i < m->pairs->count; i++)
                if (!rw_expr_bound(((cypher_map_pair*)m->pairs->items[i])->value, row)) return false;
            return true;
        }
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *ss = (cypher_subscript*)expr;
            return rw_expr_bound(ss->expr, row) && rw_expr_bound(ss->index, row) &&
                   rw_expr_bound(ss->slice_start, row) && rw_expr_bound(ss->slice_end, row);
        }
        case AST_NODE_LIST_COMPREHENSION: {
            cypher_list_comprehension *lc = (cypher_list_comprehension*)expr;
            if (!rw_expr_bound(lc->list_expr, row)) return false;
            bool p = rw_push_local(lc->variable);
            bool ok = rw_expr_bound(lc->where_expr, row) && rw_expr_bound(lc->transform_expr, row);
            rw_pop_local(p);
            return ok;
        }
        case AST_NODE_LIST_PREDICATE: {
            cypher_list_predicate *lp = (cypher_list_predicate*)expr;
            if (!rw_expr_bound(lp->list_expr, row)) return false;
            bool p = rw_push_local(lp->variable);
            bool ok = rw_expr_bound(lp->predicate, row);
            rw_pop_local(p);
            return ok;
        }
        case AST_NODE_REDUCE_EXPR: {
            cypher_reduce_expr *r = (cypher_reduce_expr*)expr;
            if (!rw_expr_bound(r->initial_value, row) || !rw_expr_bound(r->list_expr, row)) return false;
            bool p1 = rw_push_local(r->accumulator), p2 = rw_push_local(r->variable);
            bool ok = rw_expr_bound(r->expression, row);
            rw_pop_local(p2); rw_pop_local(p1);
            return ok;
        }
        default: return false;
    }
}

/* Slots of `entity.key` reads in expr (base bound to a node or a relationship). */
static void rw_find_entity_props(ast_node **slot, const rw_row *row, ast_node ****slots, int *n)
{
    ast_node *e = slot ? *slot : NULL;
    if (!e) return;
    switch (e->type) {
        case AST_NODE_PROPERTY: {
            cypher_property *p = (cypher_property*)e;
            if (p->expr && p->expr->type == AST_NODE_IDENTIFIER) {
                const char *b = ((cypher_identifier*)p->expr)->name;
                if (!rw_is_local(b) && !rw_row_scalar(row, b) &&
                    (get_variable_node_id(row->vm, b) >= 0 || is_variable_edge(row->vm, b))) {
                    ast_node ***grown = realloc(*slots, (size_t)(*n + 1) * sizeof(ast_node**));
                    if (grown) { *slots = grown; (*slots)[(*n)++] = slot; }
                }
                return;
            }
            rw_find_entity_props(&p->expr, row, slots, n);
            return;
        }
        case AST_NODE_BINARY_OP:
            rw_find_entity_props(&((cypher_binary_op*)e)->left, row, slots, n);
            rw_find_entity_props(&((cypher_binary_op*)e)->right, row, slots, n);
            return;
        case AST_NODE_NOT_EXPR: rw_find_entity_props(&((cypher_not_expr*)e)->expr, row, slots, n); return;
        case AST_NODE_FUNCTION_CALL: {
            ast_list *a = ((cypher_function_call*)e)->args;
            for (int i = 0; a && i < a->count; i++) rw_find_entity_props(&a->items[i], row, slots, n);
            return;
        }
        case AST_NODE_LIST: {
            ast_list *a = ((cypher_list*)e)->items;
            for (int i = 0; a && i < a->count; i++) rw_find_entity_props(&a->items[i], row, slots, n);
            return;
        }
        case AST_NODE_NULL_CHECK: rw_find_entity_props(&((cypher_null_check*)e)->expr, row, slots, n); return;
        case AST_NODE_CASE_EXPR: {
            cypher_case_expr *c = (cypher_case_expr*)e;
            rw_find_entity_props(&c->operand, row, slots, n);
            for (int i = 0; c->when_clauses && i < c->when_clauses->count; i++) {
                cypher_when_clause *w = (cypher_when_clause*)c->when_clauses->items[i];
                rw_find_entity_props(&w->condition, row, slots, n);
                rw_find_entity_props(&w->result, row, slots, n);
            }
            rw_find_entity_props(&c->else_expr, row, slots, n);
            return;
        }
        case AST_NODE_MAP: {
            cypher_map *m = (cypher_map*)e;
            for (int i = 0; m->pairs && i < m->pairs->count; i++)
                rw_find_entity_props(&((cypher_map_pair*)m->pairs->items[i])->value, row, slots, n);
            return;
        }
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *ss = (cypher_subscript*)e;
            rw_find_entity_props(&ss->expr, row, slots, n);
            rw_find_entity_props(&ss->index, row, slots, n);
            rw_find_entity_props(&ss->slice_start, row, slots, n);
            rw_find_entity_props(&ss->slice_end, row, slots, n);
            return;
        }
        case AST_NODE_LIST_COMPREHENSION: {
            cypher_list_comprehension *lc = (cypher_list_comprehension*)e;
            rw_find_entity_props(&lc->list_expr, row, slots, n);
            bool p = rw_push_local(lc->variable);
            rw_find_entity_props(&lc->where_expr, row, slots, n);
            rw_find_entity_props(&lc->transform_expr, row, slots, n);
            rw_pop_local(p);
            return;
        }
        case AST_NODE_LIST_PREDICATE: {
            cypher_list_predicate *lp = (cypher_list_predicate*)e;
            rw_find_entity_props(&lp->list_expr, row, slots, n);
            bool p = rw_push_local(lp->variable);
            rw_find_entity_props(&lp->predicate, row, slots, n);
            rw_pop_local(p);
            return;
        }
        case AST_NODE_REDUCE_EXPR: {
            cypher_reduce_expr *r = (cypher_reduce_expr*)e;
            rw_find_entity_props(&r->initial_value, row, slots, n);
            rw_find_entity_props(&r->list_expr, row, slots, n);
            bool p1 = rw_push_local(r->accumulator), p2 = rw_push_local(r->variable);
            rw_find_entity_props(&r->expression, row, slots, n);
            rw_pop_local(p2); rw_pop_local(p1);
            return;
        }
        default: return;
    }
}

/* Evaluate expr when its entities appear only as `entity.key`: read each
 * property (a cached statement), then run the rest as a value expression
 * with bound parameters (GQLITE-T-0431). -1 when expr has another shape. */
static int rw_eval_with_entity_props(cypher_executor *executor, ast_node *expr, const rw_row *row,
                                     char **out_text, int *out_type)
{
    ast_node ***slots = NULL; int n = 0;
    rw_find_entity_props(&expr, row, &slots, &n);
    if (n == 0) { free(slots); return -1; }
    rw_row ev = {0};
    ev.vm = row->vm;   /* borrowed */
    for (int i = 0; i < row->n_scalars; i++)
        rw_row_add_scalar(&ev, row->scalars[i].name, row->scalars[i].text, row->scalars[i].type);
    ast_node **orig = calloc((size_t)n, sizeof(ast_node*));
    for (int i = 0; i < n; i++) {
        char *t = NULL; int ty;
        rw_eval_cell(executor, *slots[i], (rw_row*)row, &t, &ty);
        char nm[32];
        snprintf(nm, sizeof(nm), "__rw_prop_%d", i);
        rw_row_add_scalar(&ev, nm, t, t ? ty : SQLITE_NULL);
        free(t);
        orig[i] = *slots[i];
        *slots[i] = (ast_node*)make_identifier(nm, 0);
    }
    int rc = -1;
    if (rw_expr_scalar_only(expr, &ev)) {
        *out_type = rw_eval_scalar(executor, expr, &ev, out_text);
        rc = *out_type >= 0 ? 0 : -1;
    }
    for (int i = 0; i < n; i++) { ast_node_free(*slots[i]); *slots[i] = orig[i]; }
    free(orig); free(slots);
    ev.vm = NULL;
    rw_row_free(&ev);
    return rc;
}

static ast_node *rw_value_literal(cypher_executor *executor, ast_node *expr, const rw_row *row)
{
    if (expr && !rw_expr_scalar_only(expr, row) && expr->type != AST_NODE_IDENTIFIER && rw_expr_bound(expr, row)) {
        /* A value over the row's entities (`MERGE (:City {name: p.bornIn})`). */
        char *text = NULL; int type;
        if (expr->type == AST_NODE_PROPERTY ||
            rw_eval_with_entity_props(executor, expr, row, &text, &type) < 0)
            rw_eval_cell(executor, expr, (rw_row*)row, &text, &type);
        ast_node *lit = rw_make_literal(executor, text, text ? type : SQLITE_NULL);
        free(text);
        return lit;
    }
    if (!expr || !rw_expr_scalar_only(expr, row)) return NULL;
    ast_node *lit = rw_map_property_literal(executor, expr, row);
    if (!lit && expr->type == AST_NODE_IDENTIFIER) {
        /* A bound value: its own text and type, so a string that looks
         * like JSON stays a string (GQLITE-A-0007). */
        const rw_scalar *sc = rw_row_scalar(row, ((cypher_identifier *)expr)->name);
        if (sc) lit = rw_make_literal(executor, sc->text, sc->text ? sc->type : SQLITE_NULL);
    }
    if (!lit) {
        char *text = NULL;
        int type = rw_eval_scalar(executor, expr, row, &text);
        if (type < 0) { free(text); return NULL; }
        lit = rw_make_literal(executor, text, type);
        free(text);
    }
    return lit;
}

static void rw_subst_add(rw_subst **subs, int *n_subs, ast_node **slot, ast_node *lit)
{
    rw_subst *grown = realloc(*subs, (size_t)(*n_subs + 1) * sizeof(rw_subst));
    if (!grown) { ast_node_free(lit); return; }
    *subs = grown;
    (*subs)[*n_subs].slot = slot;
    (*subs)[*n_subs].original = *slot;
    (*subs)[*n_subs].substitute = lit;
    (*n_subs)++;
    *slot = lit;
}

/* Row values of a pattern as named parameters, so the SQL text of a
 * per-row MATCH / MERGE is the same for every row and its prepared
 * statement is reused (GQLITE-T-0431). Off when NULL. */
static GQL_THREAD_LOCAL rw_param_sink *g_rw_params;

/* A scalar literal as a parameter of the sink: the parameter node, or NULL
 * for a literal that stays as it is (null, a list, a map, inf / NaN). */
static ast_node *rw_param_for_literal(ast_node *lit)
{
    if (!g_rw_params || !lit || lit->type != AST_NODE_LITERAL) return NULL;
    cypher_literal *l = (cypher_literal*)lit;
    char *val = NULL;
    switch (l->literal_type) {
        case LITERAL_INTEGER: val = sqlite3_mprintf("%lld", (long long)l->value.integer); break;
        case LITERAL_BOOLEAN: val = sqlite3_mprintf("%s", l->value.boolean ? "true" : "false"); break;
        case LITERAL_DECIMAL: {
            double d = l->value.decimal;
            if (isnan(d) || isinf(d)) return NULL;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", d);
            if (!strpbrk(buf, ".eE")) strcat(buf, ".0");
            val = sqlite3_mprintf("%s", buf);
            break;
        }
        case LITERAL_STRING: {
            const char *t = l->value.string ? l->value.string : "";
            size_t n = strlen(t);
            char *q = malloc(n * 6 + 3), *o = q;
            if (!q) return NULL;
            *o++ = '"';
            for (const unsigned char *c = (const unsigned char*)t; *c; c++) {
                if (*c == '"' || *c == '\\') { *o++ = '\\'; *o++ = (char)*c; }
                else if (*c < 0x20) o += sprintf(o, "\\u%04x", *c);
                else *o++ = (char)*c;
            }
            *o++ = '"'; *o = 0;
            val = sqlite3_mprintf("%s", q);
            free(q);
            break;
        }
        default: return NULL;
    }
    if (!val) return NULL;
    char name[32];
    snprintf(name, sizeof(name), "__rwp_%d", g_rw_params->n++);
    char *grown = sqlite3_mprintf("%s%s\"%s\":%s", g_rw_params->json ? g_rw_params->json : "",
                                  g_rw_params->json ? "," : "", name, val);
    sqlite3_free(val);
    sqlite3_free(g_rw_params->json);
    g_rw_params->json = grown;
    return (ast_node*)make_parameter(name, 0);
}

/* The query's parameters with the sink's added (owned, sqlite3_free). */
static char *rw_params_merged(const char *user, const rw_param_sink *sink)
{
    if (!sink || !sink->json) return user ? sqlite3_mprintf("%s", user) : NULL;
    const char *u = user;
    while (u && (*u == ' ' || *u == '\t' || *u == '\n' || *u == '\r')) u++;
    if (!u || *u != '{') return sqlite3_mprintf("{%s}", sink->json);
    /* `{...}` + `,"__rwp_0":...` */
    const char *end = strrchr(u, '}');
    if (!end) return sqlite3_mprintf("{%s}", sink->json);
    bool empty = true;
    for (const char *c = u + 1; c < end; c++) if (*c != ' ' && *c != '\t' && *c != '\n' && *c != '\r') { empty = false; break; }
    return sqlite3_mprintf("%.*s%s%s}", (int)(end - u), u, empty ? "" : ",", sink->json);
}

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
        ast_node *lit = rw_value_literal(executor, pair->value, row);
        ast_node *param = rw_param_for_literal(lit);
        if (param) { ast_node_free(lit); lit = param; }
        if (lit) rw_subst_add(subs, n_subs, &pair->value, lit);
    }
}

/* #136: SET values that read the row (`row.value`, a WITH variable) become
 * literals for this row, so SET sees them with their types. */
static void rw_literalize_set(cypher_executor *executor, cypher_set *set, const rw_row *row,
                              rw_subst **subs, int *n_subs)
{
    if (!set || !set->items) return;
    for (int i = 0; i < set->items->count; i++) {
        cypher_set_item *it = (cypher_set_item*)set->items->items[i];
        if (!it || !it->expr || !it->property) continue;
        if (it->property->type == AST_NODE_LABEL_EXPR) continue;
        if (it->expr->type == AST_NODE_LITERAL || it->expr->type == AST_NODE_PARAMETER) continue;
        if (it->expr->type == AST_NODE_MAP) {
            rw_literalize_props(executor, it->expr, row, subs, n_subs);
            continue;
        }
        /* A plain `n.q` is read by SET itself, which keeps a string that
         * looks like JSON a string; any other value over entities (CASE,
         * IS NULL, properties(n).s, ...) is computed for the row: SET
         * refused it or stored nothing (GQLITE-T-0456). */
        if (!rw_expr_scalar_only(it->expr, row) && it->expr->type == AST_NODE_PROPERTY &&
            ((cypher_property *)it->expr)->expr &&
            ((cypher_property *)it->expr)->expr->type == AST_NODE_IDENTIFIER) continue;
        ast_node *lit = rw_value_literal(executor, it->expr, row);
        if (lit) rw_subst_add(subs, n_subs, &it->expr, lit);
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
        *subs[i].slot = subs[i].original;
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
static void rw_eval_cell(cypher_executor *executor, ast_node *expr, rw_row *row, char **text, int *type);
static void rw_bind_value(rw_row *row, const char *name, const char *text, int type);

/* Does a WITH ... WHERE keep the row? A predicate over values runs as SQL;
 * one over entities (`WHERE n.num % 2 = 0`) through the row's bindings. */
static bool rw_where_keeps(cypher_executor *executor, ast_node *where, rw_row *row)
{
    char *text = NULL;
    int type;
    if (rw_expr_scalar_only(where, row)) type = rw_eval_scalar(executor, where, row, &text);
    else rw_eval_cell(executor, where, row, &text, &type);
    bool keep = type >= 0 && text && (strcmp(text, "1") == 0 || strcmp(text, "true") == 0);
    free(text);
    return keep;
}

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
            if (type >= 0) rw_bind_value(&next, it->alias, text, type);   /* list[i] can be an entity */
            free(text);
        } else if (it->alias) {
            /* An expression over the row's entities (`n.x + 1`). */
            char *text = NULL; int type;
            rw_eval_cell(executor, it->expr, row, &text, &type);
            rw_bind_value(&next, it->alias, text, type);
            free(text);
        }
    }
    bool keep = !w->where || rw_where_keeps(executor, w->where, &next);
    rw_row_free(row);
    *row = next;
    return keep ? 0 : 1;   /* 1 = row filtered out */
}

static bool is_path_name(ast_list *pattern, const char *name)
{
    for (int i = 0; pattern && i < pattern->count; i++) {
        ast_node *it = pattern->items[i];
        if (it && it->type == AST_NODE_PATH && ((cypher_path *)it)->var_name &&
            strcmp(((cypher_path *)it)->var_name, name) == 0) return true;
    }
    return false;
}

/* An interleaved path id array ([n, e, n, ...]) as the hydrated
 * {nodes, rels} object (owned), or NULL. */
static char *rw_hydrate_path(cypher_executor *executor, const char *ids)
{
    if (!ids || ids[0] != '[') return NULL;
    char *nj = gql_sql_node_json_expr("", "je.value");
    char *ej = gql_sql_edge_json_expr("", "e.id", "e.type", "e.source_id", "e.target_id");
    char *sql = (nj && ej) ? sqlite3_mprintf(
        "SELECT json_object('nodes', json((SELECT json_group_array(json(%s)) FROM "
        "(SELECT value FROM json_each(?1) WHERE key %% 2 = 0 ORDER BY key) je)), "
        "'rels', json((SELECT json_group_array(json(%s)) FROM "
        "(SELECT value FROM json_each(?1) WHERE key %% 2 = 1 ORDER BY key) je2 "
        "JOIN edges e ON e.id = je2.value)))", nj, ej) : NULL;
    free(nj); free(ej);
    char *out = NULL;
    sqlite3_stmt *st = NULL;
    if (sql && sqlite3_prepare_v2(executor->db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, ids, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
            out = strdup((const char *)sqlite3_column_text(st, 0));
        sqlite3_finalize(st);
    }
    sqlite3_free(sql);
    return out;
}

/* The matches of a pattern for one row of a write pipeline: a read query
 * `MATCH <pattern> WHERE id(v) = <id> AND ... AND <where> RETURN <vars>`,
 * with each entity the row binds pinned to its id. *out gets one variable
 * map per match (the row's entities plus the pattern's), owned by the
 * caller. Returns the number of matches, or -1. */
/* Is `name` a node or a single-relationship variable of the pattern (not a
 * path, not a variable-length relationship)? / a relationship? */
static int rw_pattern_var_kind(ast_list *pattern, const char *name)
{
    for (int i = 0; pattern && i < pattern->count; i++) {
        ast_node *it = pattern->items[i];
        if (!it) continue;
        if (it->type == AST_NODE_NODE_PATTERN) {
            const char *v = ((cypher_node_pattern*)it)->variable;
            if (v && strcmp(v, name) == 0) return 1;
            continue;
        }
        if (it->type != AST_NODE_PATH) continue;
        cypher_path *p = (cypher_path*)it;
        if (p->var_name && strcmp(p->var_name, name) == 0) return 0;
        for (int j = 0; p->elements && j < p->elements->count; j++) {
            ast_node *el = p->elements->items[j];
            if (el->type == AST_NODE_NODE_PATTERN) {
                const char *v = ((cypher_node_pattern*)el)->variable;
                if (v && strcmp(v, name) == 0) return 1;
            } else if (el->type == AST_NODE_REL_PATTERN) {
                cypher_rel_pattern *rp = (cypher_rel_pattern*)el;
                if (rp->variable && strcmp(rp->variable, name) == 0) return rp->varlen ? 0 : 2;
            }
        }
    }
    return 0;
}
static bool rw_pattern_var_is_entity(ast_list *pattern, const char *name) { return rw_pattern_var_kind(pattern, name) > 0; }
static bool rw_pattern_var_is_rel(ast_list *pattern, const char *name) { return rw_pattern_var_kind(pattern, name) == 2; }

static int rw_match_maps(cypher_executor *executor, ast_list *pattern, ast_node *where,
                         rw_row *row, rw_row **out, cypher_result *result)
{
    *out = NULL;
    char **names = NULL; int n = 0;
    rw_collect_pattern_vars(pattern, &names, &n);
    ast_node *cond = NULL;
    /* Bound entities as id parameters: the SQL text stays the same across
     * rows (GQLITE-T-0431). */
    char *id_json = NULL;
    for (int i = 0; i < n; i++) {
        int id = -1;
        if (is_variable_edge(row->vm, names[i])) id = get_variable_edge_id(row->vm, names[i]);
        else id = get_variable_node_id(row->vm, names[i]);
        if (id < 0) continue;
        ast_list *args = ast_list_create();
        ast_list_append(args, (ast_node*)make_identifier(names[i], 0));
        ast_node *fn = (ast_node*)make_function_call("id", args, false, 0);
        char pname[32];
        snprintf(pname, sizeof(pname), "__rwid_%d", i);
        char *grown = sqlite3_mprintf("%s%s\"%s\":%d", id_json ? id_json : "", id_json ? "," : "", pname, id);
        sqlite3_free(id_json);
        id_json = grown;
        ast_node *eq = (ast_node*)make_binary_op(BINARY_OP_EQ, fn, (ast_node*)make_parameter(pname, 0), 0);
        cond = cond ? (ast_node*)make_binary_op(BINARY_OP_AND, cond, eq, 0) : eq;
    }
    /* the original WHERE is shared: AND it in without owning it */
    cypher_binary_op *and_where = NULL;
    ast_node *full = cond;
    if (where) {
        if (cond) { and_where = make_binary_op(BINARY_OP_AND, cond, where, 0); full = (ast_node*)and_where; }
        else full = where;
    }
    cypher_match *m = make_cypher_match(pattern, full, false, NULL);
    /* node / relationship variables come back as ids, not rendered */
    bool *as_id = calloc((size_t)(n > 0 ? n : 1), sizeof(bool));
    for (int i = 0; i < n; i++) as_id[i] = rw_pattern_var_is_entity(pattern, names[i]);
    ast_list *items = ast_list_create();
    for (int i = 0; i < n; i++) {
        ast_node *e = (ast_node*)make_identifier(names[i], 0);
        if (as_id[i]) {
            ast_list *a = ast_list_create();
            ast_list_append(a, e);
            e = (ast_node*)make_function_call("id", a, false, 0);
        }
        ast_list_append(items, (ast_node*)make_return_item(e, names[i]));
    }
    if (n == 0) ast_list_append(items, (ast_node*)make_return_item((ast_node*)make_integer_literal(1, 0), "_one"));
    cypher_return *ret = make_cypher_return(false, items, NULL, NULL, NULL);
    ast_list *cl = ast_list_create();
    ast_list_append(cl, (ast_node*)m);
    ast_list_append(cl, (ast_node*)ret);
    cypher_query *q = make_cypher_query(cl, false);

    /* the row's values (UNWIND / WITH) as bindings for the pattern's maps */
    foreach_context *fctx = rw_row_foreach_ctx(executor, row);
    foreach_context *prev = g_foreach_ctx;
    if (fctx) g_foreach_ctx = fctx;
    const char *user_params = executor->params_json;
    rw_param_sink ids = { id_json, 0 };
    char *merged = id_json ? rw_params_merged(user_params, &ids) : NULL;
    if (merged) executor->params_json = merged;
    cypher_result *pr = cypher_executor_execute_ast(executor, (ast_node*)q);
    executor->params_json = user_params;
    sqlite3_free(merged);
    sqlite3_free(id_json);
    g_foreach_ctx = prev;
    if (fctx) free_foreach_context(fctx);

    int count = -1;
    if (!pr || !pr->success) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to match a pattern in a write pipeline: %s",
                 pr && pr->error_message ? pr->error_message : "no result");
        set_result_error(result, msg);
    } else {
        count = pr->row_count;
        *out = calloc((size_t)(count > 0 ? count : 1), sizeof(rw_row));
        for (int r = 0; r < count && *out; r++) {
            rw_row *o = &(*out)[r];
            o->vm = create_variable_map();
            rw_fold_map(o->vm, row->vm);
            for (int c = 0; c < n && c < pr->column_count; c++) {
                bool is_edge = false;
                long long id = -1;
                if (as_id[c]) {
                    /* an id column: a null id is an unbound OPTIONAL part */
                    if (!pr->data[r][c]) continue;
                    id = strtoll(pr->data[r][c], NULL, 10);
                    is_edge = rw_pattern_var_is_rel(pattern, names[c]);
                } else {
                    id = rw_entity_id(pr->data[r][c], &is_edge);
                }
                if (id >= 0) {
                    if (is_edge) set_variable_edge_id(o->vm, names[c], (int)id);
                    else set_variable_node_id(o->vm, names[c], (int)id);
                } else {
                    /* a value, e.g. a path variable (hydrated from its ids) */
                    int ty = pr->data_types ? pr->data_types[r][c] : SQLITE_TEXT;
                    if (ty == GQL_COL_TYPE_JSON) ty = SQLITE_TEXT;
                    char *hp = is_path_name(pattern, names[c]) ? rw_hydrate_path(executor, pr->data[r][c]) : NULL;
                    rw_row_add_scalar(o, names[c], hp ? hp : pr->data[r][c],
                                      (hp || pr->data[r][c]) ? (hp ? SQLITE_TEXT : ty) : SQLITE_NULL);
                    free(hp);
                }
            }
        }
    }
    if (pr) cypher_result_free(pr);
    /* free what was made here; the pattern and the WHERE belong to the query */
    ast_node_free((ast_node*)ret);
    free(m);
    if (and_where) free(and_where);
    if (cond) ast_node_free(cond);
    free(cl->items); free(cl); free(q);
    rw_free_names(names, n);
    free(as_id);
    return count;
}

/* The list of an UNWIND in a write pipeline's tail, as JSON text (owned;
 * NULL for a null list), evaluated with the row's values and entities. */
static int rw_eval_list(cypher_executor *executor, ast_node *expr, rw_row *row, char **out)
{
    *out = NULL;
    if (rw_expr_scalar_only(expr, row)) {
        char *text = NULL;
        int type = rw_eval_scalar(executor, expr, row, &text);
        if (type < 0) { free(text); return -1; }
        *out = text;
        return 0;
    }
    foreach_context *fctx = rw_row_foreach_ctx(executor, row);
    foreach_context *prev = g_foreach_ctx;
    if (fctx) g_foreach_ctx = fctx;
    property_type pt = PROP_TYPE_TEXT;
    property_value pv;
    property_value_init(&pv);
    int erc = executor_eval_value(executor, expr, row->vm, &pt, &pv);
    g_foreach_ctx = prev;
    if (fctx) free_foreach_context(fctx);
    if (erc == 0 && (pt == PROP_TYPE_JSON || pt == PROP_TYPE_TEXT)) { *out = pv.as_str; pv.as_str = NULL; }
    property_value_free(&pv);
    return erc == -1 ? -1 : 0;
}

/* ---- Row-wise RETURN: projection, grouping, DISTINCT, ORDER BY, SKIP,
 * LIMIT (#136 review: the shapes routed here returned rows unsorted, not
 * deduplicated, not limited, and aggregated without grouping). ---------- */

typedef struct { char **cells; int *types; } rw_out_row;

/* Project one live row into out (owned). Non-aggregate cells only. */
static void rw_project_row(cypher_executor *executor, cypher_return *ret, rw_row *row,
                           rw_out_row *out)
{
    int cols = ret->items->count;
    cypher_result tmp = {0};
    char **cells = calloc((size_t)cols, sizeof(char*));
    int *types = calloc((size_t)cols, sizeof(int));
    tmp.data = &cells;
    tmp.data_types = &types;
    project_return_row_from_var_map(executor, ret, row->vm, &tmp, 0);
    for (int i = 0; i < cols; i++) {
        cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
        if (aggregating_call_name(it->expr)) {
            free(cells[i]); cells[i] = NULL; types[i] = SQLITE_NULL;
        } else if (it->expr && it->expr->type != AST_NODE_LITERAL && rw_expr_scalar_only(it->expr, row)) {
            char *text = NULL;
            int type = rw_eval_scalar(executor, it->expr, row, &text);
            free(cells[i]);
            cells[i] = text;
            types[i] = type >= 0 ? type : SQLITE_NULL;
        } else if (!cells[i] && it->expr && it->expr->type != AST_NODE_LITERAL &&
                   it->expr->type != AST_NODE_IDENTIFIER &&
                   (it->expr->type != AST_NODE_PROPERTY ||
                    (((cypher_property*)it->expr)->expr &&
                     ((cypher_property*)it->expr)->expr->type != AST_NODE_IDENTIFIER))) {
            /* also `startNode(r).id`: a property of a computed value */
            /* An expression over the row's entities and values
             * (`n.x + 1`, `toUpper(n.id)`). */
            foreach_context *fctx = rw_row_foreach_ctx(executor, row);
            foreach_context *prev = g_foreach_ctx;
            if (fctx) g_foreach_ctx = fctx;
            property_type pt = PROP_TYPE_TEXT;
            property_value pv;
            property_value_init(&pv);
            int erc = executor_eval_value(executor, it->expr, row->vm, &pt, &pv);
            g_foreach_ctx = prev;
            if (fctx) free_foreach_context(fctx);
            if (erc == 0) {
                char buf[64];
                switch (pt) {
                    case PROP_TYPE_INTEGER:
                        snprintf(buf, sizeof(buf), "%lld", (long long)pv.as_int);
                        cells[i] = strdup(buf); types[i] = SQLITE_INTEGER; break;
                    case PROP_TYPE_REAL:
                        cells[i] = strdup(gql_format_double(buf, sizeof(buf), pv.as_real));
                        types[i] = SQLITE_FLOAT; break;
                    case PROP_TYPE_BOOLEAN:
                        cells[i] = strdup(pv.as_bool ? "true" : "false");
                        types[i] = GQL_COL_TYPE_BOOLEAN; break;
                    case PROP_TYPE_JSON:
                        cells[i] = pv.as_str ? strdup(pv.as_str) : NULL;
                        types[i] = GQL_COL_TYPE_JSON; break;
                    default:
                        cells[i] = pv.as_str ? strdup(pv.as_str) : NULL;
                        types[i] = GQL_COL_TYPE_STRING; break;
                }
            }
            property_value_free(&pv);
        }
    }
    out->cells = cells;
    out->types = types;
}

static bool rw_cell_equal(const char *a, int ta, const char *b, int tb)
{
    if (!a || !b) return a == b;
    if ((ta == SQLITE_INTEGER || ta == SQLITE_FLOAT) && (tb == SQLITE_INTEGER || tb == SQLITE_FLOAT))
        return strtod(a, NULL) == strtod(b, NULL);
    return ta == tb && strcmp(a, b) == 0;
}

static bool rw_rows_equal(const rw_out_row *a, const rw_out_row *b, int cols, const bool *only)
{
    for (int i = 0; i < cols; i++) {
        if (only && !only[i]) continue;
        if (!rw_cell_equal(a->cells[i], a->types[i], b->cells[i], b->types[i])) return false;
    }
    return true;
}

/* Cypher order of two cells: numbers by value, other values by text,
 * null after every value. */
static int rw_cell_cmp(const char *a, int ta, const char *b, int tb)
{
    if (!a || ta == SQLITE_NULL) return (!b || tb == SQLITE_NULL) ? 0 : 1;
    if (!b || tb == SQLITE_NULL) return -1;
    bool na = (ta == SQLITE_INTEGER || ta == SQLITE_FLOAT);
    bool nb = (tb == SQLITE_INTEGER || tb == SQLITE_FLOAT);
    if (na && nb) {
        double x = strtod(a, NULL), y = strtod(b, NULL);
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    if (na != nb) return na ? 1 : -1;   /* strings before numbers */
    return strcmp(a, b);
}

typedef struct { const int *col; const bool *desc; int n; } rw_sort_spec;
static GQL_THREAD_LOCAL const rw_sort_spec *g_rw_sort;

static int rw_out_row_cmp(const void *pa, const void *pb)
{
    const rw_out_row *a = (const rw_out_row*)pa, *b = (const rw_out_row*)pb;
    for (int k = 0; k < g_rw_sort->n; k++) {
        int c = g_rw_sort->col[k];
        int r = rw_cell_cmp(a->cells[c], a->types[c], b->cells[c], b->types[c]);
        if (r != 0) {
            /* DESC reverses the order, so nulls come first */
            return g_rw_sort->desc[k] ? -r : r;
        }
    }
    return 0;
}

static bool rw_same_expr(ast_node *a, ast_node *b)
{
    if (!a || !b || a->type != b->type) return false;
    if (a->type == AST_NODE_IDENTIFIER)
        return strcmp(((cypher_identifier*)a)->name, ((cypher_identifier*)b)->name) == 0;
    if (a->type == AST_NODE_PROPERTY) {
        cypher_property *pa = (cypher_property*)a, *pb = (cypher_property*)b;
        return pa->property_name && pb->property_name &&
               strcmp(pa->property_name, pb->property_name) == 0 && rw_same_expr(pa->expr, pb->expr);
    }
    return false;
}

/* SKIP / LIMIT value: a literal or a parameter. -1 when absent. */
static int rw_count_value(cypher_executor *executor, ast_node *expr, long long *out)
{
    *out = -1;
    if (!expr) return 0;
    if (expr->type == AST_NODE_LITERAL && ((cypher_literal*)expr)->literal_type == LITERAL_INTEGER) {
        *out = ((cypher_literal*)expr)->value.integer;
        return 0;
    }
    if (expr->type == AST_NODE_PARAMETER && executor->params_json) {
        property_type pt; property_value pv; property_value_init(&pv);
        int rc = get_param_value(executor->params_json, ((cypher_parameter*)expr)->name, &pt, &pv);
        bool ok = rc == 0 && pt == PROP_TYPE_INTEGER;
        if (ok) *out = pv.as_int;
        property_value_free(&pv);
        return ok ? 0 : -1;
    }
    return -1;
}

/* ---- Row-wise aggregation (GQLITE-T-0426): count, collect, sum, avg, min
 * and max over the cells of the rows of a group, so collect(n) of entities
 * and an aggregate inside a larger expression work after a write. ------- */

/* The value of expr for the row, as a result cell (an entity is rendered). */
static void rw_eval_cell(cypher_executor *executor, ast_node *expr, rw_row *row, char **text, int *type)
{
    ast_list *items = ast_list_create();
    ast_list_append(items, (ast_node*)make_return_item(expr, NULL));
    cypher_return one = {0};
    one.items = items;
    rw_out_row cell = {0};
    rw_project_row(executor, &one, row, &cell);
    *text = cell.cells ? cell.cells[0] : NULL;
    *type = cell.types ? cell.types[0] : SQLITE_NULL;
    if (!*text) *type = SQLITE_NULL;
    free(cell.cells); free(cell.types);
    free(items->items[0]);   /* shell only: expr belongs to the query */
    items->count = 0;
    ast_list_free(items);
}

/* A cell as a JSON element of a collect() list. */
static void rw_json_append_cell(char **buf, const char *text, int type)
{
    char *el;
    if (type == SQLITE_INTEGER || type == SQLITE_FLOAT) el = sqlite3_mprintf("%s", text);
    else if (type == GQL_COL_TYPE_BOOLEAN)
        el = sqlite3_mprintf("%s", (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) ? "true" : "false");
    else if (type != GQL_COL_TYPE_STRING && (text[0] == '[' || text[0] == '{')) el = sqlite3_mprintf("%s", text);
    else {
        /* a JSON string */
        size_t n = strlen(text);
        char *q = malloc(n * 6 + 3), *o = q;
        if (!q) return;
        *o++ = '"';
        for (const unsigned char *p = (const unsigned char*)text; *p; p++) {
            if (*p == '"' || *p == '\\') { *o++ = '\\'; *o++ = (char)*p; }
            else if (*p == '\n') { *o++ = '\\'; *o++ = 'n'; }
            else if (*p == '\r') { *o++ = '\\'; *o++ = 'r'; }
            else if (*p == '\t') { *o++ = '\\'; *o++ = 't'; }
            else if (*p < 0x20) o += sprintf(o, "\\u%04x", *p);
            else *o++ = (char)*p;
        }
        *o++ = '"'; *o = 0;
        el = sqlite3_mprintf("%s", q);
        free(q);
    }
    char *grown = sqlite3_mprintf("%s%s%s", *buf ? *buf : "", *buf ? "," : "", el);
    sqlite3_free(el);
    sqlite3_free(*buf);
    *buf = grown;
}

/* Aggregate call `call` over rows[idx[0..n)]. Returns 0 and an owned cell,
 * or -1 for an aggregate this does not compute. */
static int rw_aggregate(cypher_executor *executor, ast_node *call, rw_row *rows, const int *idx, int n,
                        char **out_text, int *out_type)
{
    *out_text = NULL; *out_type = SQLITE_NULL;
    const char *agg = aggregating_call_name(call);
    if (!agg) return -1;
    char name[16];
    snprintf(name, sizeof(name), "%s", agg);   /* agg points to a shared buffer */
    cypher_function_call *fc = (cypher_function_call*)call;
    ast_node *arg = (fc->args && fc->args->count > 0) ? fc->args->items[0] : NULL;
    bool is_count = strcmp(name, "count") == 0, is_collect = strcmp(name, "collect") == 0;
    bool is_sum = strcmp(name, "sum") == 0, is_avg = strcmp(name, "avg") == 0;
    bool is_min = strcmp(name, "min") == 0, is_max = strcmp(name, "max") == 0;
    if (!is_count && !is_collect && !is_sum && !is_avg && !is_min && !is_max) return -1;
    if (!arg) {
        if (!is_count) return -1;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", n);
        *out_text = strdup(buf); *out_type = SQLITE_INTEGER;
        return 0;
    }
    if (is_count && arg->type == AST_NODE_IDENTIFIER) {
        /* count(x): x bound and not null; DISTINCT by entity id or value.
         * No need to render the entity (GQLITE-T-0431). */
        const char *v = ((cypher_identifier*)arg)->name;
        char **keys = calloc((size_t)(n > 0 ? n : 1), sizeof(char*));
        int cnt = 0;
        for (int k = 0; k < n; k++) {
            rw_row *row = &rows[idx[k]];
            char key[48];
            const char *kp = NULL;
            int nid = get_variable_node_id(row->vm, v);
            if (is_variable_edge(row->vm, v) && get_variable_edge_id(row->vm, v) >= 0) {
                snprintf(key, sizeof(key), "e%d", get_variable_edge_id(row->vm, v)); kp = key;
            } else if (nid >= 0) {
                snprintf(key, sizeof(key), "n%d", nid); kp = key;
            } else {
                const rw_scalar *sc = rw_row_scalar(row, v);
                if (sc && sc->text && sc->type != SQLITE_NULL) kp = sc->text;
            }
            if (!kp) continue;
            if (fc->distinct) {
                bool dup = false;
                for (int q = 0; q < cnt && !dup; q++) dup = strcmp(keys[q], kp) == 0;
                if (dup) continue;
                keys[cnt] = strdup(kp);
            }
            cnt++;
        }
        for (int q = 0; fc->distinct && q < cnt; q++) free(keys[q]);
        free(keys);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", cnt);
        *out_text = strdup(buf); *out_type = SQLITE_INTEGER;
        return 0;
    }
    char **cells = calloc((size_t)(n > 0 ? n : 1), sizeof(char*));
    int *types = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    int m = 0;
    for (int k = 0; k < n; k++) {
        char *t; int ty;
        rw_eval_cell(executor, arg, &rows[idx[k]], &t, &ty);
        if (!t) continue;
        bool dup = false;
        for (int q = 0; fc->distinct && q < m && !dup; q++) dup = rw_cell_equal(cells[q], types[q], t, ty);
        if (dup) { free(t); continue; }
        cells[m] = t; types[m] = ty; m++;
    }
    char buf[64];
    if (is_count) {
        snprintf(buf, sizeof(buf), "%d", m);
        *out_text = strdup(buf); *out_type = SQLITE_INTEGER;
    } else if (is_collect) {
        char *body = NULL;
        for (int k = 0; k < m; k++) rw_json_append_cell(&body, cells[k], types[k]);
        char *list = sqlite3_mprintf("[%s]", body ? body : "");
        sqlite3_free(body);
        *out_text = strdup(list); *out_type = GQL_COL_TYPE_JSON;
        sqlite3_free(list);
    } else if (is_min || is_max) {
        int best = -1;
        for (int k = 0; k < m; k++) {
            if (best < 0) { best = k; continue; }
            int c = rw_cell_cmp(cells[k], types[k], cells[best], types[best]);
            if ((is_min && c < 0) || (is_max && c > 0)) best = k;
        }
        if (best >= 0) { *out_text = strdup(cells[best]); *out_type = types[best]; }
    } else {
        bool all_int = true;
        long long isum = 0; double dsum = 0; int seen = 0;
        for (int k = 0; k < m; k++) {
            if (types[k] != SQLITE_INTEGER && types[k] != SQLITE_FLOAT) continue;
            if (types[k] == SQLITE_FLOAT) all_int = false;
            isum += strtoll(cells[k], NULL, 10);
            dsum += strtod(cells[k], NULL);
            seen++;
        }
        if (is_sum) {
            if (all_int) { snprintf(buf, sizeof(buf), "%lld", isum); *out_type = SQLITE_INTEGER; }
            else { gql_format_double(buf, sizeof(buf), dsum); *out_type = SQLITE_FLOAT; }
            *out_text = strdup(buf);
        } else if (seen > 0) {
            gql_format_double(buf, sizeof(buf), dsum / seen);
            *out_text = strdup(buf); *out_type = SQLITE_FLOAT;
        }
    }
    for (int k = 0; k < m; k++) free(cells[k]);
    free(cells); free(types);
    return 0;
}

/* The slots of the aggregate calls in expr (not inside another aggregate). */
typedef ast_node **rw_slot;

static void rw_find_aggregates(ast_node **slot, rw_slot **slots, int *n)
{
    ast_node *e = slot ? *slot : NULL;
    if (!e) return;
    if (aggregating_call_name(e)) {
        rw_slot *grown = realloc(*slots, (size_t)(*n + 1) * sizeof(rw_slot));
        if (grown) { *slots = grown; (*slots)[(*n)++] = slot; }
        return;
    }
    switch (e->type) {
        case AST_NODE_BINARY_OP:
            rw_find_aggregates(&((cypher_binary_op*)e)->left, slots, n);
            rw_find_aggregates(&((cypher_binary_op*)e)->right, slots, n);
            break;
        case AST_NODE_NOT_EXPR: rw_find_aggregates(&((cypher_not_expr*)e)->expr, slots, n); break;
        case AST_NODE_PROPERTY: rw_find_aggregates(&((cypher_property*)e)->expr, slots, n); break;
        case AST_NODE_SUBSCRIPT: {
            cypher_subscript *s = (cypher_subscript*)e;
            rw_find_aggregates(&s->expr, slots, n);
            rw_find_aggregates(&s->index, slots, n);
            rw_find_aggregates(&s->slice_start, slots, n);
            rw_find_aggregates(&s->slice_end, slots, n);
            break;
        }
        case AST_NODE_FUNCTION_CALL: {
            ast_list *a = ((cypher_function_call*)e)->args;
            for (int i = 0; a && i < a->count; i++) rw_find_aggregates(&a->items[i], slots, n);
            break;
        }
        case AST_NODE_LIST: {
            ast_list *a = ((cypher_list*)e)->items;
            for (int i = 0; a && i < a->count; i++) rw_find_aggregates(&a->items[i], slots, n);
            break;
        }
        default: break;
    }
}

static bool rw_with_aggregates(cypher_with *w)
{
    if (!w || w->pass_all || !w->items) return false;
    for (int i = 0; i < w->items->count; i++) {
        cypher_return_item *it = (cypher_return_item*)w->items->items[i];
        rw_slot *slots = NULL; int n = 0;
        if (it) rw_find_aggregates(&it->expr, &slots, &n);
        free(slots);
        if (n > 0) return true;
    }
    return false;
}

/* Bind a computed value to name: an entity cell binds the entity. */
static void rw_bind_value(rw_row *row, const char *name, const char *text, int type)
{
    bool is_edge = false;
    long long id = rw_entity_id(text, &is_edge);
    if (id >= 0 && is_edge) set_variable_edge_id(row->vm, name, (int)id);
    else if (id >= 0) set_variable_node_id(row->vm, name, (int)id);
    else rw_row_add_scalar(row, name, text, type);
}

/* An aggregating WITH over rows[idx[0..n)]: one output row per group of the
 * grouping items. 0 on success, -1 on error. */
static int rw_with_aggregate(cypher_executor *executor, cypher_with *w, rw_row *rows, const int *idx, int n,
                             rw_row **out, int *n_out, cypher_result *result)
{
    *out = NULL; *n_out = 0;
    if (w->order_by || w->skip || w->limit || w->distinct) {
        set_result_error(result, "WITH with an aggregate, ORDER BY, SKIP, LIMIT or DISTINCT after a write is not supported");
        return -1;
    }
    int ni = w->items->count;
    bool *is_agg = calloc((size_t)ni, sizeof(bool));
    for (int i = 0; i < ni; i++) {
        cypher_return_item *it = (cypher_return_item*)w->items->items[i];
        rw_slot *slots = NULL; int ns = 0;
        rw_find_aggregates(&it->expr, &slots, &ns);
        free(slots);
        is_agg[i] = ns > 0;
    }
    /* key cells per row */
    rw_out_row *keys = calloc((size_t)(n > 0 ? n : 1), sizeof(rw_out_row));
    bool any_key = false;
    for (int k = 0; k < n; k++) {
        keys[k].cells = calloc((size_t)ni, sizeof(char*));
        keys[k].types = calloc((size_t)ni, sizeof(int));
        for (int i = 0; i < ni; i++) {
            if (is_agg[i]) continue;
            any_key = true;
            rw_eval_cell(executor, ((cypher_return_item*)w->items->items[i])->expr, &rows[idx[k]],
                         &keys[k].cells[i], &keys[k].types[i]);
        }
    }
    bool *key_only = calloc((size_t)ni, sizeof(bool));
    for (int i = 0; i < ni; i++) key_only[i] = !is_agg[i];
    bool *used = calloc((size_t)(n > 0 ? n : 1), sizeof(bool));
    int *members = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    rw_row *res = calloc((size_t)(n > 0 ? n : 1), sizeof(rw_row));
    int n_res = 0, rc = 0;
    bool empty_group = (!any_key && n == 0);
    for (int g = 0; (g < n || empty_group) && rc == 0; g++) {
        if (!empty_group && used[g]) continue;
        int nm = 0;
        for (int j = g; !empty_group && j < n; j++) {
            if (used[j] || !rw_rows_equal(&keys[g], &keys[j], ni, key_only)) continue;
            used[j] = true;
            members[nm++] = idx[j];
        }
        rw_row *rep = empty_group ? NULL : &rows[idx[g]];
        rw_row *nr = &res[n_res++];
        nr->vm = create_variable_map();
        /* Scope to evaluate the aggregating items: the representative row's
         * values, its entities rendered as values, and the aggregates. */
        rw_row ev = {0};
        if (rep) {
            for (int s = 0; s < rep->n_scalars; s++)
                rw_row_add_scalar(&ev, rep->scalars[s].name, rep->scalars[s].text, rep->scalars[s].type);
            for (int v = 0; rep->vm && v < rep->vm->count; v++) {
                const char *vn = rep->vm->mappings[v].variable;
                if (!vn || rw_row_scalar(&ev, vn)) continue;
                ast_node *id = (ast_node*)make_identifier((char*)vn, 0);
                char *t; int ty;
                rw_eval_cell(executor, id, rep, &t, &ty);
                ast_node_free(id);
                rw_row_add_scalar(&ev, vn, t, ty == SQLITE_NULL ? SQLITE_NULL : SQLITE_TEXT);
                free(t);
            }
        }
        for (int i = 0; i < ni && rc == 0; i++) {
            cypher_return_item *it = (cypher_return_item*)w->items->items[i];
            const char *alias = it->alias ? it->alias
                : (it->expr && it->expr->type == AST_NODE_IDENTIFIER ? ((cypher_identifier*)it->expr)->name : NULL);
            if (!alias) continue;
            if (!is_agg[i]) {
                if (it->expr->type == AST_NODE_IDENTIFIER && rep) {
                    const char *src = ((cypher_identifier*)it->expr)->name;
                    if (is_variable_edge(rep->vm, src)) { set_variable_edge_id(nr->vm, alias, get_variable_edge_id(rep->vm, src)); continue; }
                    if (get_variable_node_id(rep->vm, src) >= 0) { set_variable_node_id(nr->vm, alias, get_variable_node_id(rep->vm, src)); continue; }
                }
                rw_bind_value(nr, alias, keys[g].cells[i], keys[g].types[i]);
                continue;
            }
            rw_slot *slots = NULL; int ns = 0;
            rw_find_aggregates(&it->expr, &slots, &ns);
            ast_node **orig = calloc((size_t)ns, sizeof(ast_node*));
            for (int s = 0; s < ns && rc == 0; s++) {
                char *t; int ty;
                if (rw_aggregate(executor, *slots[s], rows, members, nm, &t, &ty) < 0) {
                    set_result_error(result, "Unsupported aggregate in WITH after a write");
                    rc = -1;
                    break;
                }
                char an[32];
                snprintf(an, sizeof(an), "__rw_agg_%d", s);
                rw_row_add_scalar(&ev, an, t, ty);
                free(t);
                orig[s] = *slots[s];
                *slots[s] = (ast_node*)make_identifier(an, 0);
            }
            if (rc == 0) {
                char *t = NULL;
                int ty = rw_expr_scalar_only(it->expr, &ev) ? rw_eval_scalar(executor, it->expr, &ev, &t) : -1;
                if (ty < 0) { set_result_error(result, "Cannot evaluate an aggregating WITH item after a write"); rc = -1; }
                else rw_bind_value(nr, alias, t, ty);
                free(t);
            }
            for (int s = 0; s < ns; s++) {
                if (!orig[s]) continue;
                ast_node_free(*slots[s]);
                *slots[s] = orig[s];
            }
            free(orig); free(slots);
        }
        rw_row_free(&ev);
        if (rc == 0 && w->where && !rw_where_keeps(executor, w->where, nr)) { rw_row_free(nr); n_res--; }
        if (empty_group) break;
    }
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < ni; i++) free(keys[k].cells[i]);
        free(keys[k].cells); free(keys[k].types);
    }
    free(keys); free(key_only); free(used); free(members); free(is_agg);
    if (rc < 0) {
        for (int k = 0; k < n_res; k++) rw_row_free(&res[k]);
        free(res);
        return -1;
    }
    *out = res; *n_out = n_res;
    return 0;
}

static int rw_project_return(cypher_executor *executor, cypher_return *ret, rw_row *rows,
                             const bool *row_alive, int n_rows, cypher_result *result)
{
    set_return_column_names(ret, result);
    int out_cols = ret->items->count;

    /* An ORDER BY expression that is not a RETURN column is projected as a
     * hidden column after the RETURN columns, and dropped after the sort. */
    int n_order = ret->order_by ? ret->order_by->count : 0;
    int *order_col = calloc((size_t)(n_order > 0 ? n_order : 1), sizeof(int));
    ast_list *aug_items = ast_list_create();
    for (int i = 0; i < out_cols; i++) ast_list_append(aug_items, ret->items->items[i]);
    for (int k = 0; k < n_order; k++) {
        cypher_order_by_item *ob = (cypher_order_by_item*)ret->order_by->items[k];
        order_col[k] = -1;
        for (int i = 0; i < out_cols && order_col[k] < 0; i++) {
            cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
            if ((ob->expr && ob->expr->type == AST_NODE_IDENTIFIER && it->alias &&
                 strcmp(((cypher_identifier*)ob->expr)->name, it->alias) == 0) ||
                rw_same_expr(ob->expr, it->expr)) {
                order_col[k] = i;
            }
        }
        if (order_col[k] < 0) {
            order_col[k] = aug_items->count;
            ast_list_append(aug_items, (ast_node*)make_return_item(ob->expr, NULL));
        }
    }
    cypher_return aug = *ret;
    aug.items = aug_items;
    ret = &aug;
    int cols = aug_items->count;
    bool agg = return_has_aggregation(ret);

    /* 1. Project the live rows. */
    rw_out_row *out = calloc((size_t)(n_rows > 0 ? n_rows : 1), sizeof(rw_out_row));
    int *src = calloc((size_t)(n_rows > 0 ? n_rows : 1), sizeof(int));
    int n_out = 0;
    for (int r = 0; r < n_rows; r++) {
        if (!row_alive[r]) continue;
        rw_project_row(executor, ret, &rows[r], &out[n_out]);
        src[n_out++] = r;
    }

    /* 2. Aggregation: one row per group of the non-aggregate cells. */
    if (agg) {
        bool *key = calloc((size_t)cols, sizeof(bool));
        for (int i = 0; i < cols; i++)
            key[i] = !aggregating_call_name(((cypher_return_item*)ret->items->items[i])->expr);
        rw_out_row *groups = calloc((size_t)(n_out > 0 ? n_out : 1), sizeof(rw_out_row));
        int n_groups = 0;
        variable_map **maps = calloc((size_t)(n_out > 0 ? n_out : 1), sizeof(variable_map*));
        int *member = calloc((size_t)(n_out > 0 ? n_out : 1), sizeof(int));
        bool *used = calloc((size_t)(n_out > 0 ? n_out : 1), sizeof(bool));
        bool any_key = false;
        for (int i = 0; i < cols; i++) if (key[i]) any_key = true;
        /* No grouping key: one row even when there are no input rows. */
        int first_pass = (!any_key && n_out == 0) ? 1 : 0;
        for (int g = 0; g < n_out || first_pass; g++) {
            if (!first_pass && used[g]) continue;
            int k = 0;
            for (int j = first_pass ? n_out : g; j < n_out; j++) {
                if (used[j] || !rw_rows_equal(&out[g], &out[j], cols, key)) continue;
                used[j] = true;
                member[k] = src[j];
                maps[k++] = rows[src[j]].vm;
            }
            rw_out_row *grp = &groups[n_groups++];
            grp->cells = calloc((size_t)cols, sizeof(char*));
            grp->types = calloc((size_t)cols, sizeof(int));
            for (int i = 0; i < cols; i++) {
                cypher_return_item *it = (cypher_return_item*)ret->items->items[i];
                if (key[i]) {
                    grp->cells[i] = out[g].cells[i] ? strdup(out[g].cells[i]) : NULL;
                    grp->types[i] = out[g].types[i];
                } else if (rw_aggregate(executor, it->expr, rows, member, k,
                                        &grp->cells[i], &grp->types[i]) == 0) {
                    /* computed from the row cells */
                } else {
                    char ***save_d = result->data; int **save_t = result->data_types;
                    result->data = &grp->cells; result->data_types = &grp->types;
                    project_aggregate_cell(executor, it, maps, k, result, i);
                    result->data = save_d; result->data_types = save_t;
                }
            }
            if (first_pass) break;
        }
        for (int j = 0; j < n_out; j++) {
            for (int i = 0; i < cols; i++) free(out[j].cells[i]);
            free(out[j].cells); free(out[j].types);
        }
        free(out); free(maps); free(member); free(used); free(key);
        out = groups;
        n_out = n_groups;
    }
    free(src);

    /* 3. DISTINCT */
    if (ret->distinct) {
        int w = 0;
        for (int j = 0; j < n_out; j++) {
            bool dup = false;
            for (int q = 0; q < w && !dup; q++) dup = rw_rows_equal(&out[q], &out[j], cols, NULL);
            if (dup) {
                for (int i = 0; i < cols; i++) free(out[j].cells[i]);
                free(out[j].cells); free(out[j].types);
            } else {
                out[w++] = out[j];
            }
        }
        n_out = w;
    }

    int rc = 0;
    /* 4. ORDER BY */
    if (n_order > 0) {
        bool *desc = calloc((size_t)n_order, sizeof(bool));
        for (int k = 0; k < n_order; k++)
            desc[k] = ((cypher_order_by_item*)ret->order_by->items[k])->descending;
        rw_sort_spec spec = { order_col, desc, n_order };
        g_rw_sort = &spec;
        qsort(out, (size_t)n_out, sizeof(rw_out_row), rw_out_row_cmp);
        g_rw_sort = NULL;
        free(desc);
    }
    /* Drop the hidden sort columns. */
    for (int j = 0; j < n_out; j++) {
        for (int i = out_cols; i < cols; i++) { free(out[j].cells[i]); out[j].cells[i] = NULL; }
    }
    free(order_col);
    for (int i = out_cols; i < aug_items->count; i++) free(aug_items->items[i]);   /* shells only */
    aug_items->count = 0;
    ast_list_free(aug_items);

    /* 5. SKIP / LIMIT */
    long long skip = -1, limit = -1;
    if (rc == 0 && (rw_count_value(executor, ret->skip, &skip) < 0 ||
                    rw_count_value(executor, ret->limit, &limit) < 0)) {
        set_result_error(result, "SKIP and LIMIT must be integers");
        rc = -1;
    }
    int first = 0, last = n_out;
    if (rc == 0) {
        if (skip > 0) first = skip >= n_out ? n_out : (int)skip;
        if (limit >= 0 && first + limit < last) last = first + (int)limit;
    }

    if (rc == 0) {
        int m = last - first;
        result->row_count = m;
        result->data = calloc((size_t)(m > 0 ? m : 1), sizeof(char**));
        result->data_types = calloc((size_t)(m > 0 ? m : 1), sizeof(int*));
    }
    for (int j = 0; j < n_out; j++) {
        if (rc == 0 && j >= first && j < last) {
            result->data[j - first] = out[j].cells;
            result->data_types[j - first] = out[j].types;
        } else {
            for (int i = 0; i < out_cols; i++) free(out[j].cells[i]);
            free(out[j].cells); free(out[j].types);
        }
    }
    free(out);
    if (rc < 0) result->success = false;
    return rc;
}

/* DELETE for one row of the pipeline. A plain variable deletes its entity;
 * an expression (`DELETE list[0]`, `DELETE m.key`, a path) deletes each node
 * and relationship in its value (Delete5). */
static char *rw_hydrate_path(cypher_executor *executor, const char *ids);

/* Is `name` a path variable of a MATCH in clauses? */
static bool rw_is_match_path(ast_list *clauses, const char *name)
{
    for (int j = 0; clauses && name && j < clauses->count; j++) {
        ast_node *m = clauses->items[j];
        if (!m || m->type != AST_NODE_MATCH) continue;
        ast_list *pat = ((cypher_match*)m)->pattern;
        for (int q = 0; pat && q < pat->count; q++) {
            ast_node *pi = pat->items[q];
            if (pi && pi->type == AST_NODE_PATH && ((cypher_path*)pi)->var_name &&
                strcmp(((cypher_path*)pi)->var_name, name) == 0) return true;
        }
    }
    return false;
}

static int rw_execute_delete(cypher_executor *executor, cypher_delete *del, rw_row *row,
                             ast_list *clauses, cypher_result *result)
{
    bool has_expr = false;
    for (int i = 0; del->items && i < del->items->count; i++) {
        cypher_delete_item *it = (cypher_delete_item*)del->items->items[i];
        if (it && (it->expr || (it->variable && rw_row_scalar(row, it->variable)))) has_expr = true;
    }
    if (!has_expr) return execute_delete_operations(executor, del, row->vm, result);

    variable_map *vm = create_variable_map();
    ast_list *items = ast_list_create();
    if (!vm || !items) { if (vm) free_variable_map(vm); if (items) ast_list_free(items); return -1; }
    int n = 0;
    for (int i = 0; i < del->items->count; i++) {
        cypher_delete_item *it = (cypher_delete_item*)del->items->items[i];
        if (!it) continue;
        if (!it->expr && it->variable && !rw_row_scalar(row, it->variable)) {
            char name[32];
            snprintf(name, sizeof(name), "__del_%d", n++);
            if (is_variable_edge(row->vm, it->variable)) set_variable_edge_id(vm, name, get_variable_edge_id(row->vm, it->variable));
            else if (get_variable_node_id(row->vm, it->variable) >= 0) set_variable_node_id(vm, name, get_variable_node_id(row->vm, it->variable));
            else continue;
            ast_list_append(items, (ast_node*)make_delete_item(name));
            continue;
        }
        if (!it->expr && !it->variable) continue;
        /* An expression, or a variable bound to a value (a path, a list). */
        char *text = NULL; int type;
        ast_node *vexpr = it->expr ? it->expr : (ast_node*)make_identifier(it->variable, 0);
        rw_eval_cell(executor, vexpr, row, &text, &type);
        if (!it->expr) ast_node_free(vexpr);
        /* A path variable holds its interleaved ids: render the path. */
        if (!it->expr && text && rw_is_match_path(clauses, it->variable)) {
            char *hp = rw_hydrate_path(executor, text);
            if (hp) { free(text); text = hp; }
        }
        if (!text) continue;   /* DELETE null does nothing */
        /* Every rendered node ({"id", "labels", "properties"}) and
         * relationship ({"id", "type", "startNode", ...}) in the value. */
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(executor->db,
                "SELECT json_extract(value, '$.id'), json_type(value, '$.startNode') IS NOT NULL "
                "FROM json_tree(?1) WHERE type = 'object' AND json_type(value, '$.id') = 'integer' "
                "AND json_type(value, '$.properties') = 'object' "
                "AND (json_type(value, '$.labels') = 'array' OR json_type(value, '$.startNode') IS NOT NULL)",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, text, -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                char name[32];
                snprintf(name, sizeof(name), "__del_%d", n++);
                int id = sqlite3_column_int(st, 0);
                if (sqlite3_column_int(st, 1)) set_variable_edge_id(vm, name, id);
                else set_variable_node_id(vm, name, id);
                ast_list_append(items, (ast_node*)make_delete_item(name));
            }
            sqlite3_finalize(st);
        }
        free(text);
    }
    cypher_delete synth = *del;
    synth.items = items;
    int rc = execute_delete_operations(executor, &synth, vm, result);
    ast_list_free(items);
    free_variable_map(vm);
    return rc;
}

/* ---- CALL { } in the row pipeline (GQLITE-T-0432) ----------------------
 * For each outer row the inner query runs with the imported variables bound:
 * an entity through `MATCH (v) WHERE id(v) = $p`, a value through
 * `WITH $p AS v`. Each inner row continues the outer row. */

/* A row value as JSON text for a parameter (owned, sqlite3_free). */
static char *rw_scalar_json(const rw_scalar *sc)
{
    if (!sc || !sc->text || sc->type == SQLITE_NULL) return sqlite3_mprintf("null");
    char *buf = NULL;
    rw_json_append_cell(&buf, sc->text, sc->type);
    return buf ? buf : sqlite3_mprintf("null");
}

/* Run the inner query of a CALL for one row. NULL on error (result set). */
static cypher_result *rw_run_call(cypher_executor *executor, ast_node *inner, rw_row *row,
                                  cypher_result *result)
{
    cypher_with *imp = NULL;
    cypher_query *iq = inner && inner->type == AST_NODE_QUERY ? (cypher_query*)inner : NULL;
    if (iq && iq->clauses && iq->clauses->count > 0 && iq->clauses->items[0] &&
        iq->clauses->items[0]->type == AST_NODE_WITH)
        imp = (cypher_with*)iq->clauses->items[0];
    if (!imp) {
        /* not correlated: the query as it is */
        cypher_result *ir = cypher_executor_execute_ast(executor, inner);
        if (!ir || !ir->success) {
            set_result_error(result, ir && ir->error_message ? ir->error_message : "CALL subquery failed");
            if (ir) cypher_result_free(ir);
            return NULL;
        }
        return ir;
    }
    /* names imported by the leading WITH */
    char **names = NULL; int n = 0;
    if (imp->pass_all) {
        for (int v = 0; row->vm && v < row->vm->count; v++) {
            names = realloc(names, (size_t)(n + 1) * sizeof(char*));
            names[n++] = strdup(row->vm->mappings[v].variable);
        }
        for (int v = 0; v < row->n_scalars; v++) {
            names = realloc(names, (size_t)(n + 1) * sizeof(char*));
            names[n++] = strdup(row->scalars[v].name);
        }
    } else {
        for (int i = 0; imp->items && i < imp->items->count; i++) {
            cypher_return_item *it = (cypher_return_item*)imp->items->items[i];
            if (!it || !it->expr || it->expr->type != AST_NODE_IDENTIFIER) continue;
            names = realloc(names, (size_t)(n + 1) * sizeof(char*));
            names[n++] = strdup(((cypher_identifier*)it->expr)->name);
        }
    }
    ast_list *synth = ast_list_create();          /* clauses made here */
    ast_list *with_items = ast_list_create();
    char *pjson = NULL;
    for (int i = 0; i < n; i++) {
        char pname[32];
        snprintf(pname, sizeof(pname), "__imp_%d", i);
        const rw_scalar *sc = rw_row_scalar(row, names[i]);
        int nid = sc ? -1 : get_variable_node_id(row->vm, names[i]);
        int eid = (!sc && nid < 0 && is_variable_edge(row->vm, names[i])) ? get_variable_edge_id(row->vm, names[i]) : -1;
        char *val = NULL;
        if (nid >= 0 || eid >= 0) {
            ast_list *els = ast_list_create();
            if (nid >= 0) {
                ast_list_append(els, (ast_node*)make_node_pattern(names[i], NULL, NULL));
            } else {
                ast_list_append(els, (ast_node*)make_node_pattern(NULL, NULL, NULL));
                ast_list_append(els, (ast_node*)make_rel_pattern(names[i], NULL, NULL, false, true));
                ast_list_append(els, (ast_node*)make_node_pattern(NULL, NULL, NULL));
            }
            ast_list *pat = ast_list_create();
            ast_list_append(pat, (ast_node*)make_path(els));
            ast_list *args = ast_list_create();
            ast_list_append(args, (ast_node*)make_identifier(names[i], 0));
            ast_node *eq = (ast_node*)make_binary_op(BINARY_OP_EQ, (ast_node*)make_function_call("id", args, false, 0),
                                                    (ast_node*)make_parameter(pname, 0), 0);
            ast_list_append(synth, (ast_node*)make_cypher_match(pat, eq, false, NULL));
            val = sqlite3_mprintf("%d", nid >= 0 ? nid : eid);
            ast_list_append(with_items, (ast_node*)make_return_item((ast_node*)make_identifier(names[i], 0), NULL));
        } else {
            val = rw_scalar_json(sc);   /* unbound: null */
            ast_list_append(with_items, (ast_node*)make_return_item((ast_node*)make_parameter(pname, 0), names[i]));
        }
        char *grown = sqlite3_mprintf("%s%s\"%s\":%s", pjson ? pjson : "", pjson ? "," : "", pname, val);
        sqlite3_free(val); sqlite3_free(pjson);
        pjson = grown;
    }
    if (with_items->count > 0)
        ast_list_append(synth, (ast_node*)make_cypher_with(false, with_items, NULL, NULL, NULL, NULL));
    else
        ast_list_free(with_items);
    ast_list *all = ast_list_create();
    for (int i = 0; i < synth->count; i++) ast_list_append(all, synth->items[i]);
    for (int i = 0; i < iq->clauses->count; i++) ast_list_append(all, iq->clauses->items[i]);
    cypher_query *q = make_cypher_query(all, false);

    const char *user_params = executor->params_json;
    rw_param_sink sink = { pjson, 0 };
    char *merged = pjson ? rw_params_merged(user_params, &sink) : NULL;
    if (merged) executor->params_json = merged;
    cypher_result *ir = cypher_executor_execute_ast(executor, (ast_node*)q);
    executor->params_json = user_params;
    sqlite3_free(merged);
    sqlite3_free(pjson);
    if (!ir || !ir->success) {
        set_result_error(result, ir && ir->error_message ? ir->error_message : "CALL subquery failed");
        if (ir) { cypher_result_free(ir); ir = NULL; }
    }
    for (int i = 0; i < synth->count; i++) ast_node_free(synth->items[i]);
    free(synth->items); free(synth);
    free(all->items); free(all); free(q);
    for (int i = 0; i < n; i++) free(names[i]);
    free(names);
    return ir;
}

static int handle_rowwise_write(cypher_executor *executor, cypher_query *query,
                                cypher_result *result, clause_flags flags)
{
    ast_list *clauses = query->clauses;
    if (!clauses || clauses->count == 0) return -1;

    /* Shape check: the first clause must be MATCH, UNWIND, CREATE or WITH
     * (a WITH is a read prefix, GQLITE-T-0401). Anything else goes to the
     * handlers that owned these flag sets before. */
    ast_node *first = clauses->items[0];
    if (!first || !(first->type == AST_NODE_MATCH || first->type == AST_NODE_UNWIND ||
                    first->type == AST_NODE_CREATE || first->type == AST_NODE_WITH ||
                    first->type == AST_NODE_MERGE || first->type == AST_NODE_CALL_SUBQUERY)) {
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
        if (!c || rw_is_write_clause(c->type) || c->type == AST_NODE_RETURN ||
            c->type == AST_NODE_CALL_SUBQUERY) break;
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
                    /* JSON and string cells are TEXT to the row-wise
                     * evaluator (GQLITE-A-0007 result types). */
                    if (ty == GQL_COL_TYPE_JSON) ty = SQLITE_TEXT;
                    bool is_edge = false;
                    long long id = rw_entity_id(cell, &is_edge);
                    if (id >= 0) {
                        if (is_edge) set_variable_edge_id(row->vm, scope[c], (int)id);
                        else set_variable_node_id(row->vm, scope[c], (int)id);
                        scope_is_entity[c] = true;
                        continue;
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
    /* No read prefix (the query starts with a write such as MERGE): one
     * empty row runs the clauses. */
    if (!rows && !(n_scope > 0 || prefix_end > start || synth_create_match)) {
        rows = calloc(1, sizeof(rw_row));
        if (rows) { rows[0].vm = create_variable_map(); n_rows = 1; }
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
    /* ... and before the first CREATE: `CREATE (n) DELETE n` deletes the
     * node the row created (Delete4 [3]). */
    for (int i = prefix_end; i < clauses->count; i++)
        if (clauses->items[i] && (clauses->items[i]->type == AST_NODE_MERGE ||
                                  clauses->items[i]->type == AST_NODE_CREATE)) { first_merge = i; break; }
    bool *eager_done = calloc((size_t)clauses->count, sizeof(bool));
    for (int i = prefix_end; i < first_merge; i++) {
        ast_node *c = clauses->items[i];
        if (!c || c->type != AST_NODE_DELETE) continue;
        for (int r = 0; r < n_rows && rc >= 0; r++) {
            rc = rw_execute_delete(executor, (cypher_delete*)c, &rows[r], clauses, result);
        }
        if (eager_done) eager_done[i] = true;
        if (rc < 0) break;
    }

    /* --- 4. Per-row tail --------------------------------------------------- */
    /* An UNWIND in the tail forks a row into one row per element; the new
     * rows go on the end and start at the clause after the UNWIND
     * (row_start). */
    int rows_cap = n_rows > 0 ? n_rows : 1;
    bool *row_alive = calloc((size_t)rows_cap, sizeof(bool));
    int *row_start = calloc((size_t)rows_cap, sizeof(int));
    for (int r = 0; r < n_rows; r++) row_start[r] = prefix_end;
    /* An aggregating WITH is a barrier: a row that reaches it parks there
     * (row_start = -(clause + 1)); when every row has run, the parked rows
     * of the first barrier are grouped into new rows that go on from the
     * clause after it. */
    int scan_from = 0;
  run_rows:
    for (int r = scan_from; r < n_rows && rc >= 0; r++) {
        row_alive[r] = true;
        for (int i = row_start[r]; i < clauses->count && rc >= 0 && row_alive[r]; i++) {
            ast_node *c = clauses->items[i];
            if (!c) continue;
            switch (c->type) {
                case AST_NODE_UNWIND: {
                    cypher_unwind *uw = (cypher_unwind *)c;
                    char *list_json = NULL;
                    rc = rw_eval_list(executor, uw->expr, &rows[r], &list_json);
                    if (rc < 0) break;
                    row_alive[r] = false;   /* replaced by its forks */
                    sqlite3_stmt *st = NULL;
                    if (list_json && sqlite3_prepare_v2(executor->db,
                            "SELECT value, type FROM json_each(?1) WHERE json_valid(?1) AND json_type(?1) = 'array'",
                            -1, &st, NULL) == SQLITE_OK) {
                        sqlite3_bind_text(st, 1, list_json, -1, SQLITE_TRANSIENT);
                        while (sqlite3_step(st) == SQLITE_ROW) {
                            if (n_rows == rows_cap) {
                                int nc = rows_cap * 2;
                                rw_row *gr = realloc(rows, (size_t)nc * sizeof(rw_row));
                                bool *ga = realloc(row_alive, (size_t)nc * sizeof(bool));
                                int *gs = realloc(row_start, (size_t)nc * sizeof(int));
                                if (gr) rows = gr;
                                if (ga) row_alive = ga;
                                if (gs) row_start = gs;
                                if (!gr || !ga || !gs) { rc = -1; break; }
                                rows_cap = nc;
                            }
                            rw_row *nr = &rows[n_rows];
                            memset(nr, 0, sizeof(*nr));
                            nr->vm = create_variable_map();
                            rw_fold_map(nr->vm, rows[r].vm);
                            for (int k = 0; k < rows[r].n_scalars; k++) {
                                if (strcmp(rows[r].scalars[k].name, uw->alias) == 0) continue;
                                rw_row_add_scalar(nr, rows[r].scalars[k].name,
                                                  rows[r].scalars[k].text, rows[r].scalars[k].type);
                            }
                            const char *jt = (const char *)sqlite3_column_text(st, 1);
                            const char *jv = (const char *)sqlite3_column_text(st, 0);
                            int ty = SQLITE_NULL;
                            if (jt && strcmp(jt, "integer") == 0) ty = SQLITE_INTEGER;
                            else if (jt && strcmp(jt, "real") == 0) ty = SQLITE_FLOAT;
                            else if (jt && (strcmp(jt, "true") == 0 || strcmp(jt, "false") == 0)) {
                                ty = GQL_COL_TYPE_BOOLEAN; jv = jt;
                            } else if (jt && strcmp(jt, "text") == 0) ty = GQL_COL_TYPE_STRING;
                            else if (jt && strcmp(jt, "null") != 0) ty = SQLITE_TEXT;   /* list / map */
                            rw_row_add_scalar(nr, uw->alias, ty == SQLITE_NULL ? NULL : jv, ty);
                            row_alive[n_rows] = false;
                            row_start[n_rows] = i + 1;
                            n_rows++;
                        }
                        sqlite3_finalize(st);
                    }
                    free(list_json);
                    break;
                }
                case AST_NODE_DELETE:
                    if (eager_done && eager_done[i]) break;
                    rc = rw_execute_delete(executor, (cypher_delete*)c, &rows[r], clauses, result);
                    break;
                case AST_NODE_MATCH:
                case AST_NODE_MERGE: {
                    /* MATCH / OPTIONAL MATCH after a write, and MERGE: every
                     * match of the pattern is a row (Match8 [2]; MERGE (b)
                     * with two nodes gave one row). The first match continues
                     * this row; the others are new rows from the next clause. */
                    bool is_merge = c->type == AST_NODE_MERGE;
                    cypher_merge *mg = is_merge ? (cypher_merge*)c : NULL;
                    cypher_match *mt = is_merge ? NULL : (cypher_match*)c;
                    ast_list *pat = is_merge ? mg->pattern : mt->pattern;
                    rw_subst *subs = NULL; int n_subs = 0;
                    rw_param_sink sink = {0};
                    g_rw_params = &sink;
                    rw_literalize_pattern(executor, pat, &rows[r], &subs, &n_subs);
                    g_rw_params = NULL;
                    const char *user_params = executor->params_json;
                    char *row_params = rw_params_merged(user_params, &sink);
                    if (sink.json) executor->params_json = row_params;
                    /* ON CREATE / ON MATCH SET read the row's values too
                     * (`UNWIND $rows AS row MERGE (n {id: row.id}) ON CREATE
                     * SET n.v = row.v`). */
                    rw_subst *asubs = NULL; int n_asubs = 0;
                    if (is_merge) {
                        cypher_set on_create = {0}, on_match = {0};
                        on_create.items = mg->on_create;
                        on_match.items = mg->on_match;
                        rw_literalize_set(executor, &on_create, &rows[r], &asubs, &n_asubs);
                        rw_literalize_set(executor, &on_match, &rows[r], &asubs, &n_asubs);
                    }
                    rw_row *maps = NULL;
                    int nm = rw_match_maps(executor, pat, is_merge ? NULL : mt->where, &rows[r], &maps, result);
                    bool matched_existing = nm > 0;
                    if (nm < 0) {
                        rw_restore_subs(subs, n_subs); rw_restore_subs(asubs, n_asubs); rc = -1; free(maps);
                        executor->params_json = user_params; sqlite3_free(row_params); sqlite3_free(sink.json);
                        break;
                    }
                    if (nm == 0) {
                        free(maps); maps = NULL;
                        if (is_merge) {
                            variable_map *out = NULL;
                            rc = execute_merge_clause(executor, mg, result, rows[r].vm, &out);
                            if (out) { rw_fold_map(rows[r].vm, out); free_variable_map(out); }
                            /* bind the rest of the created pattern (a path
                             * variable): match it, pinned to what was made */
                            if (rc >= 0) {
                                nm = rw_match_maps(executor, pat, NULL, &rows[r], &maps, result);
                                if (nm > 1) nm = 1;
                            }
                        } else if (!mt->optional) {
                            row_alive[r] = false;
                        }
                        if (nm <= 0) {
                            rw_restore_subs(subs, n_subs);
                            rw_restore_subs(asubs, n_asubs);
                            if (nm < 0) rc = -1;
                            free(maps);
                            executor->params_json = user_params; sqlite3_free(row_params); sqlite3_free(sink.json);
                            break;
                        }
                    }
                    rw_restore_subs(subs, n_subs);
                    executor->params_json = user_params; sqlite3_free(row_params); sqlite3_free(sink.json);
                    for (int k = 0; k < nm; k++) {
                        int target = r;
                        if (k > 0) {
                            if (n_rows == rows_cap) {
                                int nc = rows_cap * 2;
                                rw_row *gr = realloc(rows, (size_t)nc * sizeof(rw_row));
                                bool *ga = realloc(row_alive, (size_t)nc * sizeof(bool));
                                int *gs = realloc(row_start, (size_t)nc * sizeof(int));
                                if (gr) rows = gr;
                                if (ga) row_alive = ga;
                                if (gs) row_start = gs;
                                if (!gr || !ga || !gs) { rc = -1; break; }
                                rows_cap = nc;
                            }
                            target = n_rows++;
                            memset(&rows[target], 0, sizeof(rw_row));
                            rows[target].vm = create_variable_map();
                            for (int q = 0; q < rows[r].n_scalars; q++)
                                rw_row_add_scalar(&rows[target], rows[r].scalars[q].name,
                                                  rows[r].scalars[q].text, rows[r].scalars[q].type);
                            row_alive[target] = false;
                            row_start[target] = i + 1;
                        }
                        rw_fold_map(rows[target].vm, maps[k].vm);
                        for (int q = 0; q < maps[k].n_scalars; q++)
                            rw_row_add_scalar(&rows[target], maps[k].scalars[q].name,
                                              maps[k].scalars[q].text, maps[k].scalars[q].type);
                        if (is_merge && matched_existing && mg->on_match && mg->on_match->count > 0 && rc >= 0)
                            rc = execute_set_items(executor, mg->on_match, rows[target].vm, result);
                    }
                    rw_restore_subs(asubs, n_asubs);
                    for (int k = 0; k < nm; k++) rw_row_free(&maps[k]);
                    free(maps);
                    break;
                }
                case AST_NODE_SET: {
                    rw_subst *subs = NULL; int n_subs = 0;
                    rw_literalize_set(executor, (cypher_set*)c, &rows[r], &subs, &n_subs);
                    rc = execute_set_operations(executor, (cypher_set*)c, rows[r].vm, result);
                    rw_restore_subs(subs, n_subs);
                    break;
                }
                case AST_NODE_REMOVE:
                    rc = execute_remove_operations(executor, (cypher_remove*)c, rows[r].vm, result);
                    break;
                case AST_NODE_FOREACH: {
                    /* #135: FOREACH after MATCH / UNWIND / WITH runs once per
                     * row, with the row's entities and values in scope. */
                    foreach_context *fctx = rw_row_foreach_ctx(executor, &rows[r]);
                    foreach_context *prev = g_foreach_ctx;
                    if (fctx) g_foreach_ctx = fctx;
                    rc = execute_foreach_clause_vm(executor, (cypher_foreach*)c, rows[r].vm, result);
                    g_foreach_ctx = prev;
                    if (fctx) free_foreach_context(fctx);
                    break;
                }
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
                    if (rw_with_aggregates((cypher_with*)c)) {
                        row_alive[r] = false;
                        row_start[r] = -(i + 1);
                        break;
                    }
                    int w = rw_apply_with(executor, (cypher_with*)c, &rows[r]);
                    if (w < 0) rc = -1;
                    else if (w == 1) row_alive[r] = false;
                    break;
                }
                case AST_NODE_CALL_SUBQUERY: {
                    cypher_call_subquery *cs = (cypher_call_subquery*)c;
                    ast_node *inner = cs->branches && cs->branches->count > 0 ? cs->branches->items[0] : NULL;
                    cypher_result *ir = inner ? rw_run_call(executor, inner, &rows[r], result) : NULL;
                    if (!ir) { rc = -1; break; }
                    result->nodes_created += ir->nodes_created;
                    result->nodes_deleted += ir->nodes_deleted;
                    result->relationships_created += ir->relationships_created;
                    result->relationships_deleted += ir->relationships_deleted;
                    result->properties_set += ir->properties_set;
                    bool unit = ir->column_count == 0 || !ir->column_names;
                    if (unit) { cypher_result_free(ir); break; }   /* a unit subquery: the row goes on */
                    if (ir->row_count == 0) { row_alive[r] = false; cypher_result_free(ir); break; }
                    /* the outer row as it was, for the forks */
                    rw_row base = {0};
                    base.vm = create_variable_map();
                    rw_fold_map(base.vm, rows[r].vm);
                    for (int q = 0; q < rows[r].n_scalars; q++)
                        rw_row_add_scalar(&base, rows[r].scalars[q].name, rows[r].scalars[q].text, rows[r].scalars[q].type);
                    for (int k = 0; k < ir->row_count && rc >= 0; k++) {
                        int target = r;
                        if (k > 0) {
                            if (n_rows == rows_cap) {
                                int nc = rows_cap * 2;
                                rw_row *gr = realloc(rows, (size_t)nc * sizeof(rw_row));
                                bool *ga = realloc(row_alive, (size_t)nc * sizeof(bool));
                                int *gs = realloc(row_start, (size_t)nc * sizeof(int));
                                if (gr) rows = gr;
                                if (ga) row_alive = ga;
                                if (gs) row_start = gs;
                                if (!gr || !ga || !gs) { rc = -1; break; }
                                rows_cap = nc;
                            }
                            target = n_rows++;
                            memset(&rows[target], 0, sizeof(rw_row));
                            rows[target].vm = create_variable_map();
                            rw_fold_map(rows[target].vm, base.vm);
                            for (int q = 0; q < base.n_scalars; q++)
                                rw_row_add_scalar(&rows[target], base.scalars[q].name, base.scalars[q].text, base.scalars[q].type);
                            row_alive[target] = false;
                            row_start[target] = i + 1;
                        }
                        for (int col = 0; col < ir->column_count; col++) {
                            const char *cell = ir->data && ir->data[k] ? ir->data[k][col] : NULL;
                            int ty = ir->data_types && ir->data_types[k] ? ir->data_types[k][col] : SQLITE_TEXT;
                            if (!cell) ty = SQLITE_NULL;
                            if (ty == GQL_COL_TYPE_JSON) ty = SQLITE_TEXT;
                            rw_bind_value(&rows[target], ir->column_names[col], cell, ty);
                        }
                    }
                    rw_row_free(&base);
                    cypher_result_free(ir);
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
    if (rc >= 0) {
        int barrier = -1;
        for (int r = 0; r < n_rows; r++)
            if (row_start[r] < 0 && (barrier < 0 || -row_start[r] - 1 < barrier)) barrier = -row_start[r] - 1;
        if (barrier >= 0) {
            int *idx = calloc((size_t)n_rows, sizeof(int));
            int n_idx = 0;
            for (int r = 0; r < n_rows; r++) {
                if (row_start[r] == -(barrier + 1)) { idx[n_idx++] = r; row_start[r] = clauses->count; }
            }
            rw_row *grouped = NULL; int n_grouped = 0;
            rc = idx ? rw_with_aggregate(executor, (cypher_with*)clauses->items[barrier], rows, idx, n_idx,
                                         &grouped, &n_grouped, result) : -1;
            free(idx);
            if (rc >= 0 && n_rows + n_grouped > rows_cap) {
                int nc = n_rows + n_grouped;
                rw_row *gr = realloc(rows, (size_t)nc * sizeof(rw_row));
                bool *ga = realloc(row_alive, (size_t)nc * sizeof(bool));
                int *gs = realloc(row_start, (size_t)nc * sizeof(int));
                if (gr) rows = gr;
                if (ga) row_alive = ga;
                if (gs) row_start = gs;
                if (!gr || !ga || !gs) rc = -1;
                else rows_cap = nc;
            }
            if (rc >= 0) {
                scan_from = n_rows;
                for (int g = 0; g < n_grouped; g++) {
                    rows[n_rows] = grouped[g];
                    row_alive[n_rows] = false;
                    row_start[n_rows] = barrier + 1;
                    n_rows++;
                }
                free(grouped);
                goto run_rows;
            }
            for (int g = 0; g < n_grouped; g++) rw_row_free(&grouped[g]);
            free(grouped);
        }
    }
    free(eager_done);

    /* --- 5. RETURN --------------------------------------------------------- */
    if (rc >= 0) {
        result->success = true;
        if (ret_clause && ret_clause->items) {
            rc = rw_project_return(executor, ret_clause, rows, row_alive, n_rows, result);
        }
    }

    for (int r = 0; r < n_rows; r++) rw_row_free(&rows[r]);
    free(rows);
    free(row_alive);
    free(row_start);
    return rc;
}

