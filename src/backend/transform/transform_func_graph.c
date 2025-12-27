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
#include "transform/transform_functions.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

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

    /* Return JSON array of {node_id, score} objects ordered by score descending */
    append_sql(ctx, "(SELECT json_group_array(json_object('node_id', node_id, 'score', score)) "
               "FROM (SELECT node_id, score FROM %s_pr%d ORDER BY score DESC))",
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

    /* Return top-k as JSON array */
    append_sql(ctx, "(SELECT json_group_array(json_object('node_id', node_id, 'score', score)) "
               "FROM (SELECT node_id, score FROM %s_pr%d ORDER BY score DESC LIMIT %d))",
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

    /* Generate PageRank CTEs with personalization using unified builder */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_ppr_%d", ctx->cte_count);

    char cte_name[128], cte_query[1024];

    /* Seed nodes CTE - parse the JSON array into a table */
    snprintf(cte_name, sizeof(cte_name), "%s_seeds", cte_base);
    snprintf(cte_query, sizeof(cte_query), "SELECT value AS node_id FROM json_each('%s')", seeds_json);
    sql_cte(ctx->unified_builder, cte_name, cte_query, false);

    /* Seed count for normalization */
    snprintf(cte_name, sizeof(cte_name), "%s_seed_count", cte_base);
    snprintf(cte_query, sizeof(cte_query), "SELECT CAST(COUNT(*) AS REAL) AS n FROM %s_seeds", cte_base);
    sql_cte(ctx->unified_builder, cte_name, cte_query, false);

    /* Out-degree CTE */
    snprintf(cte_name, sizeof(cte_name), "%s_od", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT source_id, CAST(COUNT(*) AS REAL) AS deg FROM edges GROUP BY source_id", false);

    /* Initial PageRank: seeds get 1/|seeds|, others get 0 */
    snprintf(cte_name, sizeof(cte_name), "%s_pr0", cte_base);
    snprintf(cte_query, sizeof(cte_query),
        "SELECT n.id AS node_id, "
        "CASE WHEN n.id IN (SELECT node_id FROM %s_seeds) "
        "THEN 1.0 / (SELECT n FROM %s_seed_count) ELSE 0.0 END AS score "
        "FROM nodes n",
        cte_base, cte_base);
    sql_cte(ctx->unified_builder, cte_name, cte_query, false);

    /* Personalized PageRank iterations - teleport goes to seeds, not uniform */
    for (int i = 1; i <= iterations; i++) {
        snprintf(cte_name, sizeof(cte_name), "%s_pr%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT n.id AS node_id, "
            /* Teleport term: (1-d)/|seeds| if seed, else 0 */
            "CASE WHEN n.id IN (SELECT node_id FROM %s_seeds) "
            "THEN %.4f / (SELECT sc.n FROM %s_seed_count sc) ELSE 0.0 END + "
            /* Random walk term: d * sum(PR(m)/deg(m)) using JOINs */
            "%.4f * COALESCE(SUM(p.score / COALESCE(od.deg, 1.0)), 0.0) AS score "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id "
            "LEFT JOIN %s_pr%d p ON p.node_id = e.source_id "
            "LEFT JOIN %s_od od ON od.source_id = e.source_id "
            "GROUP BY n.id",
            cte_base,
            (1.0 - damping), cte_base,
            damping,
            cte_base, i - 1,
            cte_base);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return JSON array ordered by score descending */
    append_sql(ctx, "(SELECT json_group_array(json_object('node_id', node_id, 'score', score)) "
               "FROM (SELECT node_id, score FROM %s_pr%d ORDER BY score DESC))",
               cte_base, iterations);

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

    /* Generate unique CTE name */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_lp_%d", ctx->cte_count);

    char cte_name[128], cte_query[1024];

    /* Initial labels: each node gets its own ID */
    snprintf(cte_name, sizeof(cte_name), "%s_lbl0", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT id AS node_id, id AS label FROM nodes", false);

    /* Generate iterations using window functions to avoid correlated subqueries */
    for (int i = 1; i <= iterations; i++) {
        /* Each iteration: count neighbor label votes, pick most frequent */
        snprintf(cte_name, sizeof(cte_name), "%s_lbl%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT node_id, COALESCE(label, node_id) AS label FROM ("
            "SELECT n.id AS node_id, p.label, "
            "ROW_NUMBER() OVER (PARTITION BY n.id ORDER BY COUNT(*) DESC, p.label ASC) AS rn "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id OR e.source_id = n.id "
            "LEFT JOIN %s_lbl%d p ON p.node_id = CASE WHEN e.target_id = n.id THEN e.source_id ELSE e.target_id END "
            "GROUP BY n.id, p.label) WHERE rn = 1",
            cte_base, i - 1);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return JSON array of {node_id, community} pairs ordered by community then node */
    append_sql(ctx, "(SELECT json_group_array(json_object('node_id', node_id, 'community', label)) "
               "FROM (SELECT node_id, label FROM %s_lbl%d ORDER BY label, node_id))",
               cte_base, iterations);

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

    /* Generate unique CTE name */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_lp_%d", ctx->cte_count);

    char cte_name[128], cte_query[1024];

    /* Initial labels */
    snprintf(cte_name, sizeof(cte_name), "%s_lbl0", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT id AS node_id, id AS label FROM nodes", false);

    /* Generate iterations using window functions */
    for (int i = 1; i <= iterations; i++) {
        snprintf(cte_name, sizeof(cte_name), "%s_lbl%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT node_id, COALESCE(label, node_id) AS label FROM ("
            "SELECT n.id AS node_id, p.label, "
            "ROW_NUMBER() OVER (PARTITION BY n.id ORDER BY COUNT(*) DESC, p.label ASC) AS rn "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id OR e.source_id = n.id "
            "LEFT JOIN %s_lbl%d p ON p.node_id = CASE WHEN e.target_id = n.id THEN e.source_id ELSE e.target_id END "
            "GROUP BY n.id, p.label) WHERE rn = 1",
            cte_base, i - 1);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return just the community label for the specified node */
    append_sql(ctx, "(SELECT label FROM %s_lbl%d WHERE node_id = %d)",
               cte_base, iterations, node_id);

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

    /* Generate unique CTE name */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_lp_%d", ctx->cte_count);

    char cte_name[128], cte_query[1024];

    /* Initial labels */
    snprintf(cte_name, sizeof(cte_name), "%s_lbl0", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT id AS node_id, id AS label FROM nodes", false);

    /* Generate iterations using window functions */
    for (int i = 1; i <= iterations; i++) {
        snprintf(cte_name, sizeof(cte_name), "%s_lbl%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT node_id, COALESCE(label, node_id) AS label FROM ("
            "SELECT n.id AS node_id, p.label, "
            "ROW_NUMBER() OVER (PARTITION BY n.id ORDER BY COUNT(*) DESC, p.label ASC) AS rn "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id OR e.source_id = n.id "
            "LEFT JOIN %s_lbl%d p ON p.node_id = CASE WHEN e.target_id = n.id THEN e.source_id ELSE e.target_id END "
            "GROUP BY n.id, p.label) WHERE rn = 1",
            cte_base, i - 1);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return JSON array of node IDs in the community */
    append_sql(ctx, "(SELECT json_group_array(node_id) FROM %s_lbl%d WHERE label = %d ORDER BY node_id)",
               cte_base, iterations, community_id);

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

    /* Generate unique CTE name */
    char cte_base[64];
    snprintf(cte_base, sizeof(cte_base), "_lp_%d", ctx->cte_count);

    char cte_name[128], cte_query[1024];

    /* Initial labels */
    snprintf(cte_name, sizeof(cte_name), "%s_lbl0", cte_base);
    sql_cte(ctx->unified_builder, cte_name, "SELECT id AS node_id, id AS label FROM nodes", false);

    /* Generate iterations using window functions */
    for (int i = 1; i <= iterations; i++) {
        snprintf(cte_name, sizeof(cte_name), "%s_lbl%d", cte_base, i);
        snprintf(cte_query, sizeof(cte_query),
            "SELECT node_id, COALESCE(label, node_id) AS label FROM ("
            "SELECT n.id AS node_id, p.label, "
            "ROW_NUMBER() OVER (PARTITION BY n.id ORDER BY COUNT(*) DESC, p.label ASC) AS rn "
            "FROM nodes n "
            "LEFT JOIN edges e ON e.target_id = n.id OR e.source_id = n.id "
            "LEFT JOIN %s_lbl%d p ON p.node_id = CASE WHEN e.target_id = n.id THEN e.source_id ELSE e.target_id END "
            "GROUP BY n.id, p.label) WHERE rn = 1",
            cte_base, i - 1);
        sql_cte(ctx->unified_builder, cte_name, cte_query, false);
    }

    ctx->cte_count++;

    /* Return count of distinct communities */
    append_sql(ctx, "(SELECT COUNT(DISTINCT label) FROM %s_lbl%d)",
               cte_base, iterations);

    return 0;
}
