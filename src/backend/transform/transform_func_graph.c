/*
 * transform_func_graph.c
 *    Graph algorithm function transformations for Cypher queries
 *
 * This file contains SQL generation for graph algorithms:
 *   - PageRank (standard, top-k, personalized)
 *   - Label Propagation community detection
 *   - Community queries (communityOf, communityMembers, communityCount)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "transform/cypher_transform.h"
#include "transform/transform_func_graph.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

/*
 * The unrolled steps below drop the rank of nodes without out-edges. With
 * teleport and that rank going to the same nodes, the leaky iteration
 * converges to the PageRank vector times a constant, so dividing by the
 * total gives the PageRank scores (#125). Computing the dangling total at
 * each step instead reads the previous step twice, and SQLite inlines a
 * CTE once per reference, so the SQL doubles at each step.
 */
#define PAGERANK_NORMALIZED_SCORE "score / NULLIF(SUM(score) OVER (), 0)"

/*
 * PageRank Algorithm
 *
 * Computes PageRank scores using iterative power method.
 * Uses unrolled iterations via nested CTEs since SQLite's recursive CTEs
 * don't support batch operations where each iteration depends on ALL
 * previous iteration values.
 *
 * Formula: PR(n) = (1-d)/N + d * SUM(PR(m)/out_degree(m))
 *          for all nodes m linking to n
 *
 * Default: damping=0.85, iterations=20
 */
int transform_pagerank_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming pageRank() function");

    /* Default parameters */
    double damping = 0.85;
    int iterations = 20;

    /* Parse optional arguments: pageRank() or pageRank(damping) or pageRank(damping, iterations) */
    if (func_call->args && func_call->args->count >= 1) {
        cypher_literal *damp_lit = (cypher_literal*)func_call->args->items[0];
        if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
            if (damp_lit->literal_type == LITERAL_DECIMAL) {
                damping = damp_lit->value.decimal;
            } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                damping = (double)damp_lit->value.integer;
            }
        }
    }
    if (func_call->args && func_call->args->count >= 2) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[1];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 100) iterations = 100;  /* Cap for safety */
        }
    }

    /* Generate unique CTE name */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_pagerank_%d", ctx->cte_count);

    /* Build CTEs using unified builder */
    char cte_name[128], cte_query[1024];

    /* Node count CTE */
    snprintf(cte_name, sizeof(cte_name), "%s_nc", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT CAST(COUNT(*) AS REAL) AS n FROM nodes", false);

    /* Out-degree CTE */
    snprintf(cte_name, sizeof(cte_name), "%s_od", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT source_id, CAST(COUNT(*) AS REAL) AS deg FROM edges GROUP BY source_id", false);

    /* Initial PageRank (iteration 0): uniform distribution */
    snprintf(cte_name, sizeof(cte_name), "%s_pr0", cte_base);
    snprintf(cte_query, sizeof(cte_query), "SELECT id AS node_id, 1.0/(SELECT n FROM %s_nc) AS score FROM nodes", cte_base);
    sql_cte(ctx->unified_builder, cte_name, cte_query, false);

    /* Generate iterations 1 through N using JOINs (faster than correlated subqueries) */
    for (int i = 1; i <= iterations; i++) {
        snprintf(cte_name, sizeof(cte_name), "%s_pr%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT n.id AS node_id, "
            "%.4f/(SELECT nc.n FROM %s_nc nc) + %.4f * COALESCE(SUM(p.score / COALESCE(od.deg, 1.0)), 0.0) AS score "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id "
            "LEFT JOIN %s_pr%d p ON p.node_id = e.source_id "
            "LEFT JOIN %s_od od ON od.source_id = e.source_id "
            "GROUP BY n.id",
            (1.0 - damping), cte_base, damping,
            cte_base, i - 1,
            cte_base);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return JSON array of {node_id, score} objects ordered by score descending.
     * The steps leak the rank of nodes without out-edges; normalizing gives
     * the same vector as spreading that rank to every node (#125). */
    append_sql(ctx, "(SELECT json_group_array(json_object('node_id', node_id, 'score', score)) "
               "FROM (SELECT node_id, " PAGERANK_NORMALIZED_SCORE " AS score FROM %s_pr%d ORDER BY score DESC))",
               cte_base, iterations);

    return 0;
}

/*
 * topPageRank(k) - Returns top-k nodes by PageRank score
 *
 * Usage: topPageRank(k) or topPageRank(k, damping, iterations)
 * Returns JSON array of top k nodes with their scores
 */
int transform_top_pagerank_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming topPageRank() function");

    /* First argument is k (required) */
    int k = 10;  /* default */
    double damping = 0.85;
    int iterations = 20;

    if (!func_call->args || func_call->args->count < 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("topPageRank() requires at least one argument (k)");
        return -1;
    }

    /* Parse k */
    cypher_literal *k_lit = (cypher_literal*)func_call->args->items[0];
    if (k_lit && k_lit->base.type == AST_NODE_LITERAL &&
        k_lit->literal_type == LITERAL_INTEGER) {
        k = k_lit->value.integer;
        if (k < 1) k = 1;
        if (k > 1000) k = 1000;
    }

    /* Parse optional damping and iterations */
    if (func_call->args->count >= 2) {
        cypher_literal *damp_lit = (cypher_literal*)func_call->args->items[1];
        if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
            if (damp_lit->literal_type == LITERAL_DECIMAL) {
                damping = damp_lit->value.decimal;
            } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                damping = (double)damp_lit->value.integer;
            }
        }
    }
    if (func_call->args->count >= 3) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[2];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 100) iterations = 100;
        }
    }

    /* Generate PageRank CTEs using unified builder */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_pagerank_%d", ctx->cte_count);

    char cte_name[128], cte_query[1024];

    /* Node count CTE */
    snprintf(cte_name, sizeof(cte_name), "%s_nc", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT CAST(COUNT(*) AS REAL) AS n FROM nodes", false);

    /* Out-degree CTE */
    snprintf(cte_name, sizeof(cte_name), "%s_od", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT source_id, CAST(COUNT(*) AS REAL) AS deg FROM edges GROUP BY source_id", false);

    /* Initial PageRank */
    snprintf(cte_name, sizeof(cte_name), "%s_pr0", cte_base);
    snprintf(cte_query, sizeof(cte_query), "SELECT id AS node_id, 1.0/(SELECT n FROM %s_nc) AS score FROM nodes", cte_base);
    sql_cte(ctx->unified_builder, cte_name, cte_query, false);

    for (int i = 1; i <= iterations; i++) {
        snprintf(cte_name, sizeof(cte_name), "%s_pr%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT n.id AS node_id, "
            "%.4f/(SELECT nc.n FROM %s_nc nc) + %.4f * COALESCE(SUM(p.score / COALESCE(od.deg, 1.0)), 0.0) AS score "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id "
            "LEFT JOIN %s_pr%d p ON p.node_id = e.source_id "
            "LEFT JOIN %s_od od ON od.source_id = e.source_id "
            "GROUP BY n.id",
            (1.0 - damping), cte_base, damping,
            cte_base, i - 1,
            cte_base);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return top-k as JSON array, normalized over all nodes before LIMIT (#125) */
    append_sql(ctx, "(SELECT json_group_array(json_object('node_id', node_id, 'score', score)) "
               "FROM (SELECT node_id, " PAGERANK_NORMALIZED_SCORE " AS score FROM %s_pr%d ORDER BY score DESC LIMIT %d))",
               cte_base, iterations, k);

    return 0;
}

/*
 * personalizedPageRank(seed_nodes) - PageRank biased toward seed nodes
 *
 * Usage: personalizedPageRank('[1,2,3]') - seed node IDs as JSON array
 * Or: personalizedPageRank('[1,2,3]', damping, iterations)
 *
 * Difference from regular PageRank:
 * - Initial distribution concentrated on seed nodes
 * - Teleportation returns to seed nodes instead of uniform
 */
int transform_personalized_pagerank_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming personalizedPageRank() function");

    double damping = 0.85;
    int iterations = 20;
    const char *seeds_json = NULL;

    if (!func_call->args || func_call->args->count < 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("personalizedPageRank() requires seed nodes as JSON array argument");
        return -1;
    }

    /* Parse seed nodes JSON string */
    cypher_literal *seeds_lit = (cypher_literal*)func_call->args->items[0];
    if (seeds_lit && seeds_lit->base.type == AST_NODE_LITERAL &&
        seeds_lit->literal_type == LITERAL_STRING) {
        seeds_json = seeds_lit->value.string;
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("personalizedPageRank() first argument must be a JSON array string of node IDs");
        return -1;
    }

    /* Parse optional damping and iterations */
    if (func_call->args->count >= 2) {
        cypher_literal *damp_lit = (cypher_literal*)func_call->args->items[1];
        if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
            if (damp_lit->literal_type == LITERAL_DECIMAL) {
                damping = damp_lit->value.decimal;
            } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                damping = (double)damp_lit->value.integer;
            }
        }
    }
    if (func_call->args->count >= 3) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[2];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 100) iterations = 100;
        }
    }

    /* The C algorithm via _gql_personalized_pagerank: one CTE per step hit
     * SQLite 3.54's nesting limit at 100 steps. */
    char *esc = escape_sql_string(seeds_json);
    append_sql(ctx, "json(_gql_personalized_pagerank('%s', %.17g, %d))",
               esc ? esc : seeds_json, damping, iterations);
    free(esc);

    return 0;
}

/*
 * Label Propagation Community Detection
 *
 * Iteratively assigns community labels to nodes based on neighbor majority.
 * Algorithm:
 *   1. Initialize: each node gets its own ID as label
 *   2. Iterate: each node adopts most frequent label among neighbors
 *   3. Converge: stop when labels stabilize or max iterations reached
 *
 * Returns JSON array of {node_id, community} pairs
 */
int transform_label_propagation_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming labelPropagation() function");

    /* Default parameters */
    int iterations = 10;

    /* Parse optional argument: labelPropagation() or labelPropagation(max_iterations) */
    if (func_call->args && func_call->args->count >= 1) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[0];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 50) iterations = 50;  /* Cap for safety */
        }
    }

    /* The C algorithm via _gql_label_propagation (GQLITE-T-0383) */
    append_sql(ctx, "json(_gql_label_propagation(%d))", iterations);

    return 0;
}

/*
 * communityOf(node_id) - Get community label for a specific node
 *
 * Returns the community ID that the specified node belongs to
 */
int transform_community_of_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming communityOf() function");

    if (!func_call->args || func_call->args->count < 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("communityOf() requires a node_id argument");
        return -1;
    }

    int iterations = 10;
    int node_id = 0;

    /* Parse node_id argument */
    cypher_literal *node_lit = (cypher_literal*)func_call->args->items[0];
    if (node_lit && node_lit->base.type == AST_NODE_LITERAL &&
        node_lit->literal_type == LITERAL_INTEGER) {
        node_id = node_lit->value.integer;
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("communityOf() argument must be an integer node_id");
        return -1;
    }

    /* Parse optional iterations argument */
    if (func_call->args->count >= 2) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[1];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 50) iterations = 50;
        }
    }

    append_sql(ctx, "(SELECT json_extract(value, '$.community') FROM json_each(_gql_label_propagation(%d)) "
               "WHERE json_extract(value, '$.node_id') = %d)",
               iterations, node_id);

    return 0;
}

/*
 * communityMembers(community_id) - Get all nodes in a community
 *
 * Returns JSON array of node IDs belonging to the specified community
 */
int transform_community_members_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming communityMembers() function");

    if (!func_call->args || func_call->args->count < 1) {
        ctx->has_error = true;
        ctx->error_message = strdup("communityMembers() requires a community_id argument");
        return -1;
    }

    int iterations = 10;
    int community_id = 0;

    /* Parse community_id argument */
    cypher_literal *comm_lit = (cypher_literal*)func_call->args->items[0];
    if (comm_lit && comm_lit->base.type == AST_NODE_LITERAL &&
        comm_lit->literal_type == LITERAL_INTEGER) {
        community_id = comm_lit->value.integer;
    } else {
        ctx->has_error = true;
        ctx->error_message = strdup("communityMembers() argument must be an integer community_id");
        return -1;
    }

    /* Parse optional iterations argument */
    if (func_call->args->count >= 2) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[1];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 50) iterations = 50;
        }
    }

    append_sql(ctx, "(SELECT json_group_array(nid) FROM (SELECT json_extract(value, '$.node_id') AS nid "
               "FROM json_each(_gql_label_propagation(%d)) WHERE json_extract(value, '$.community') = %d "
               "ORDER BY nid))",
               iterations, community_id);

    return 0;
}

/*
 * communityCount() - Count total number of communities
 *
 * Returns the number of distinct community labels after label propagation
 */
int transform_community_count_function(cypher_transform_context *ctx, cypher_function_call *func_call)
{
    CYPHER_DEBUG("Transforming communityCount() function");

    int iterations = 10;

    /* Parse optional iterations argument */
    if (func_call->args && func_call->args->count >= 1) {
        cypher_literal *iter_lit = (cypher_literal*)func_call->args->items[0];
        if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
            iter_lit->literal_type == LITERAL_INTEGER) {
            iterations = iter_lit->value.integer;
            if (iterations < 1) iterations = 1;
            if (iterations > 50) iterations = 50;
        }
    }

    append_sql(ctx, "(SELECT COUNT(DISTINCT json_extract(value, '$.community')) "
               "FROM json_each(_gql_label_propagation(%d)))",
               iterations);

    return 0;
}
