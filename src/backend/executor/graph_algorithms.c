/*
 * Graph Algorithms - Core Infrastructure
 *
 * CSR graph loading, algorithm detection, and result management.
 * Individual algorithms are in separate files:
 * - graph_algo_pagerank.c
 * - graph_algo_community.c
 * - graph_algo_paths.c
 * - graph_algo_centrality.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"
#include "parser/cypher_ast.h"

/* Free CSR graph */
void csr_graph_free(csr_graph *graph)
{
    if (!graph) return;

    free(graph->row_ptr);
    free(graph->col_idx);
    free(graph->node_ids);
    if (graph->user_ids) {
        for (int i = 0; i < graph->node_count; i++) {
            free(graph->user_ids[i]);
        }
        free(graph->user_ids);
    }
    free(graph->node_idx);
    free(graph->in_row_ptr);
    free(graph->in_col_idx);
    free(graph);
}

/* Load graph from SQLite into CSR format */
csr_graph* csr_graph_load(sqlite3 *db)
{
    if (!db) return NULL;

    csr_graph *graph = calloc(1, sizeof(csr_graph));
    if (!graph) return NULL;

    sqlite3_stmt *stmt = NULL;
    int rc;

    /* Step 1: Count nodes and get node IDs */
    rc = sqlite3_prepare_v2(db, "SELECT id FROM nodes ORDER BY id", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare node query: %s", sqlite3_errmsg(db));
        free(graph);
        return NULL;
    }

    int node_capacity = 1024;
    graph->node_ids = malloc(node_capacity * sizeof(int));
    if (!graph->node_ids) {
        sqlite3_finalize(stmt);
        free(graph);
        return NULL;
    }

    graph->node_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (graph->node_count >= node_capacity) {
            node_capacity *= 2;
            graph->node_ids = realloc(graph->node_ids, node_capacity * sizeof(int));
            if (!graph->node_ids) {
                sqlite3_finalize(stmt);
                csr_graph_free(graph);
                return NULL;
            }
        }
        graph->node_ids[graph->node_count++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (graph->node_count == 0) {
        CYPHER_DEBUG("No nodes found in graph");
        csr_graph_free(graph);
        return NULL;
    }

    CYPHER_DEBUG("Loaded %d nodes", graph->node_count);

    /* Build node ID -> index hash table */
    graph->node_idx_size = HASH_TABLE_SIZE;
    graph->node_idx = malloc(graph->node_idx_size * sizeof(int));
    if (!graph->node_idx) {
        csr_graph_free(graph);
        return NULL;
    }

    for (int i = 0; i < graph->node_idx_size; i++) {
        graph->node_idx[i] = -1;
    }

    for (int i = 0; i < graph->node_count; i++) {
        int node_id = graph->node_ids[i];
        int h = hash_int(node_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1) {
            h = (h + 1) % graph->node_idx_size;
        }
        graph->node_idx[h] = i;
    }

    /* Step 1b: Load user-defined 'id' property for each node */
    graph->user_ids = calloc(graph->node_count, sizeof(char*));
    if (graph->user_ids) {
        rc = sqlite3_prepare_v2(db,
            "SELECT np.node_id, np.value FROM node_props_text np "
            "JOIN property_keys pk ON pk.id = np.key_id AND pk.key = 'id'",
            -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int node_id = sqlite3_column_int(stmt, 0);
                const char *user_id = (const char*)sqlite3_column_text(stmt, 1);

                int h = hash_int(node_id, graph->node_idx_size);
                while (graph->node_idx[h] != -1) {
                    int idx = graph->node_idx[h];
                    if (graph->node_ids[idx] == node_id) {
                        graph->user_ids[idx] = user_id ? strdup(user_id) : NULL;
                        break;
                    }
                    h = (h + 1) % graph->node_idx_size;
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    /* Step 2: Count edges per node */
    graph->row_ptr = calloc(graph->node_count + 1, sizeof(int));
    graph->in_row_ptr = calloc(graph->node_count + 1, sizeof(int));
    if (!graph->row_ptr || !graph->in_row_ptr) {
        csr_graph_free(graph);
        return NULL;
    }

    rc = sqlite3_prepare_v2(db, "SELECT source_id, target_id FROM edges", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        csr_graph_free(graph);
        return NULL;
    }

    graph->edge_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int source_id = sqlite3_column_int(stmt, 0);
        int target_id = sqlite3_column_int(stmt, 1);

        int h = hash_int(source_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != source_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int source_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        h = hash_int(target_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != target_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int target_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        if (source_idx >= 0 && target_idx >= 0) {
            graph->row_ptr[source_idx + 1]++;
            graph->in_row_ptr[target_idx + 1]++;
            graph->edge_count++;
        }
    }
    sqlite3_finalize(stmt);

    CYPHER_DEBUG("Loaded %d edges", graph->edge_count);

    /* Convert counts to cumulative offsets */
    for (int i = 1; i <= graph->node_count; i++) {
        graph->row_ptr[i] += graph->row_ptr[i - 1];
        graph->in_row_ptr[i] += graph->in_row_ptr[i - 1];
    }

    /* Step 3: Fill col_idx arrays */
    graph->col_idx = malloc(graph->edge_count * sizeof(int));
    graph->in_col_idx = malloc(graph->edge_count * sizeof(int));
    if (!graph->col_idx || !graph->in_col_idx) {
        csr_graph_free(graph);
        return NULL;
    }

    int *out_count = calloc(graph->node_count, sizeof(int));
    int *in_count = calloc(graph->node_count, sizeof(int));
    if (!out_count || !in_count) {
        free(out_count);
        free(in_count);
        csr_graph_free(graph);
        return NULL;
    }

    rc = sqlite3_prepare_v2(db, "SELECT source_id, target_id FROM edges", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(out_count);
        free(in_count);
        csr_graph_free(graph);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int source_id = sqlite3_column_int(stmt, 0);
        int target_id = sqlite3_column_int(stmt, 1);

        int h = hash_int(source_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != source_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int source_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        h = hash_int(target_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != target_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int target_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        if (source_idx >= 0 && target_idx >= 0) {
            int out_pos = graph->row_ptr[source_idx] + out_count[source_idx]++;
            graph->col_idx[out_pos] = target_idx;

            int in_pos = graph->in_row_ptr[target_idx] + in_count[target_idx]++;
            graph->in_col_idx[in_pos] = source_idx;
        }
    }
    sqlite3_finalize(stmt);

    free(out_count);
    free(in_count);

    CYPHER_DEBUG("CSR graph loaded: %d nodes, %d edges", graph->node_count, graph->edge_count);

    return graph;
}

/* Detect graph algorithm in RETURN clause */
graph_algo_params detect_graph_algorithm(cypher_return *return_clause)
{
    graph_algo_params params = {0};
    params.type = GRAPH_ALGO_NONE;
    params.damping = 0.85;
    params.iterations = 20;
    params.top_k = 0;
    params.source_id = NULL;
    params.target_id = NULL;
    params.weight_prop = NULL;
    params.resolution = 1.0;

    if (!return_clause || !return_clause->items || return_clause->items->count == 0) {
        return params;
    }

    cypher_return_item *item = (cypher_return_item *)return_clause->items->items[0];
    if (!item || !item->expr || item->expr->type != AST_NODE_FUNCTION_CALL) {
        return params;
    }

    cypher_function_call *func = (cypher_function_call *)item->expr;
    if (!func->function_name) {
        return params;
    }

    /* PageRank */
    if (strcasecmp(func->function_name, "pageRank") == 0) {
        params.type = GRAPH_ALGO_PAGERANK;

        if (func->args && func->args->count >= 1) {
            cypher_literal *damp_lit = (cypher_literal *)func->args->items[0];
            if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
                if (damp_lit->literal_type == LITERAL_DECIMAL) {
                    params.damping = damp_lit->value.decimal;
                } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                    params.damping = (double)damp_lit->value.integer;
                }
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[1];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 100) params.iterations = 100;
            }
        }
        return params;
    }

    /* topPageRank */
    if (strcasecmp(func->function_name, "topPageRank") == 0) {
        params.type = GRAPH_ALGO_PAGERANK;
        params.top_k = 10;

        if (func->args && func->args->count >= 1) {
            cypher_literal *k_lit = (cypher_literal *)func->args->items[0];
            if (k_lit && k_lit->base.type == AST_NODE_LITERAL &&
                k_lit->literal_type == LITERAL_INTEGER) {
                params.top_k = k_lit->value.integer;
                if (params.top_k < 1) params.top_k = 1;
                if (params.top_k > 1000) params.top_k = 1000;
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *damp_lit = (cypher_literal *)func->args->items[1];
            if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
                if (damp_lit->literal_type == LITERAL_DECIMAL) {
                    params.damping = damp_lit->value.decimal;
                } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                    params.damping = (double)damp_lit->value.integer;
                }
            }
        }
        if (func->args && func->args->count >= 3) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[2];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 100) params.iterations = 100;
            }
        }
        return params;
    }

    /* Label Propagation */
    if (strcasecmp(func->function_name, "labelPropagation") == 0) {
        params.type = GRAPH_ALGO_LABEL_PROPAGATION;
        params.iterations = 10;

        if (func->args && func->args->count >= 1) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[0];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 100) params.iterations = 100;
            }
        }
        return params;
    }

    /* Shortest Path (Dijkstra) */
    if (strcasecmp(func->function_name, "dijkstra") == 0) {
        params.type = GRAPH_ALGO_DIJKSTRA;

        if (func->args && func->args->count >= 2) {
            cypher_literal *src_lit = (cypher_literal *)func->args->items[0];
            if (src_lit && src_lit->base.type == AST_NODE_LITERAL &&
                src_lit->literal_type == LITERAL_STRING) {
                params.source_id = strdup(src_lit->value.string);
            }
            cypher_literal *tgt_lit = (cypher_literal *)func->args->items[1];
            if (tgt_lit && tgt_lit->base.type == AST_NODE_LITERAL &&
                tgt_lit->literal_type == LITERAL_STRING) {
                params.target_id = strdup(tgt_lit->value.string);
            }
        }
        if (func->args && func->args->count >= 3) {
            cypher_literal *weight_lit = (cypher_literal *)func->args->items[2];
            if (weight_lit && weight_lit->base.type == AST_NODE_LITERAL &&
                weight_lit->literal_type == LITERAL_STRING) {
                params.weight_prop = strdup(weight_lit->value.string);
            }
        }
        return params;
    }

    /* Degree Centrality */
    if (strcasecmp(func->function_name, "degreeCentrality") == 0) {
        params.type = GRAPH_ALGO_DEGREE_CENTRALITY;
        return params;
    }

    /* Weakly Connected Components */
    if (strcasecmp(func->function_name, "wcc") == 0 ||
        strcasecmp(func->function_name, "connectedComponents") == 0 ||
        strcasecmp(func->function_name, "weaklyConnectedComponents") == 0) {
        params.type = GRAPH_ALGO_WCC;
        return params;
    }

    /* Strongly Connected Components */
    if (strcasecmp(func->function_name, "scc") == 0 ||
        strcasecmp(func->function_name, "stronglyConnectedComponents") == 0) {
        params.type = GRAPH_ALGO_SCC;
        return params;
    }

    /* Betweenness Centrality */
    if (strcasecmp(func->function_name, "betweennessCentrality") == 0 ||
        strcasecmp(func->function_name, "betweenness") == 0) {
        params.type = GRAPH_ALGO_BETWEENNESS_CENTRALITY;
        return params;
    }

    /* Closeness Centrality */
    if (strcasecmp(func->function_name, "closenessCentrality") == 0 ||
        strcasecmp(func->function_name, "closeness") == 0) {
        params.type = GRAPH_ALGO_CLOSENESS_CENTRALITY;
        return params;
    }

    /* Louvain Community Detection */
    if (strcasecmp(func->function_name, "louvain") == 0) {
        params.type = GRAPH_ALGO_LOUVAIN;
        params.resolution = 1.0;

        /* Optional resolution parameter */
        if (func->args && func->args->count >= 1) {
            cypher_literal *res_lit = (cypher_literal *)func->args->items[0];
            if (res_lit && res_lit->base.type == AST_NODE_LITERAL) {
                if (res_lit->literal_type == LITERAL_DECIMAL) {
                    params.resolution = res_lit->value.decimal;
                } else if (res_lit->literal_type == LITERAL_INTEGER) {
                    params.resolution = (double)res_lit->value.integer;
                }
            }
        }
        return params;
    }

    /* Triangle Count */
    if (strcasecmp(func->function_name, "triangleCount") == 0 ||
        strcasecmp(func->function_name, "triangles") == 0) {
        params.type = GRAPH_ALGO_TRIANGLE_COUNT;
        return params;
    }

    /* A* Shortest Path */
    if (strcasecmp(func->function_name, "astar") == 0 ||
        strcasecmp(func->function_name, "aStar") == 0) {
        params.type = GRAPH_ALGO_ASTAR;

        /* astar(source, target) or astar(source, target, lat_prop, lon_prop) */
        if (func->args && func->args->count >= 2) {
            cypher_literal *src_lit = (cypher_literal *)func->args->items[0];
            if (src_lit && src_lit->base.type == AST_NODE_LITERAL &&
                src_lit->literal_type == LITERAL_STRING) {
                params.source_id = strdup(src_lit->value.string);
            }
            cypher_literal *tgt_lit = (cypher_literal *)func->args->items[1];
            if (tgt_lit && tgt_lit->base.type == AST_NODE_LITERAL &&
                tgt_lit->literal_type == LITERAL_STRING) {
                params.target_id = strdup(tgt_lit->value.string);
            }
        }
        /* Optional: lat and lon property names for heuristic */
        if (func->args && func->args->count >= 4) {
            cypher_literal *lat_lit = (cypher_literal *)func->args->items[2];
            if (lat_lit && lat_lit->base.type == AST_NODE_LITERAL &&
                lat_lit->literal_type == LITERAL_STRING) {
                params.lat_prop = strdup(lat_lit->value.string);
            }
            cypher_literal *lon_lit = (cypher_literal *)func->args->items[3];
            if (lon_lit && lon_lit->base.type == AST_NODE_LITERAL &&
                lon_lit->literal_type == LITERAL_STRING) {
                params.lon_prop = strdup(lon_lit->value.string);
            }
        }
        return params;
    }

    /* BFS Traversal */
    if (strcasecmp(func->function_name, "bfs") == 0 ||
        strcasecmp(func->function_name, "breadthFirstSearch") == 0) {
        params.type = GRAPH_ALGO_BFS;
        params.max_depth = -1;  /* Unlimited by default */

        if (func->args && func->args->count >= 1) {
            cypher_literal *start_lit = (cypher_literal *)func->args->items[0];
            if (start_lit && start_lit->base.type == AST_NODE_LITERAL &&
                start_lit->literal_type == LITERAL_STRING) {
                params.source_id = strdup(start_lit->value.string);
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *depth_lit = (cypher_literal *)func->args->items[1];
            if (depth_lit && depth_lit->base.type == AST_NODE_LITERAL &&
                depth_lit->literal_type == LITERAL_INTEGER) {
                params.max_depth = (int)depth_lit->value.integer;
            }
        }
        return params;
    }

    /* DFS Traversal */
    if (strcasecmp(func->function_name, "dfs") == 0 ||
        strcasecmp(func->function_name, "depthFirstSearch") == 0) {
        params.type = GRAPH_ALGO_DFS;
        params.max_depth = -1;  /* Unlimited by default */

        if (func->args && func->args->count >= 1) {
            cypher_literal *start_lit = (cypher_literal *)func->args->items[0];
            if (start_lit && start_lit->base.type == AST_NODE_LITERAL &&
                start_lit->literal_type == LITERAL_STRING) {
                params.source_id = strdup(start_lit->value.string);
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *depth_lit = (cypher_literal *)func->args->items[1];
            if (depth_lit && depth_lit->base.type == AST_NODE_LITERAL &&
                depth_lit->literal_type == LITERAL_INTEGER) {
                params.max_depth = (int)depth_lit->value.integer;
            }
        }
        return params;
    }

    /* Node Similarity (Jaccard) */
    if (strcasecmp(func->function_name, "nodeSimilarity") == 0) {
        params.type = GRAPH_ALGO_NODE_SIMILARITY;
        params.threshold = 0.0;  /* Default: return all pairs */
        params.top_k = 0;        /* Default: no limit */
        params.source_id = NULL;
        params.target_id = NULL;

        /* Check for specific pair: nodeSimilarity('node1', 'node2') */
        if (func->args && func->args->count >= 2) {
            cypher_literal *node1_lit = (cypher_literal *)func->args->items[0];
            cypher_literal *node2_lit = (cypher_literal *)func->args->items[1];

            if (node1_lit && node1_lit->base.type == AST_NODE_LITERAL &&
                node1_lit->literal_type == LITERAL_STRING &&
                node2_lit && node2_lit->base.type == AST_NODE_LITERAL &&
                node2_lit->literal_type == LITERAL_STRING) {
                params.source_id = strdup(node1_lit->value.string);
                params.target_id = strdup(node2_lit->value.string);
            }
        }
        /* Check for threshold: nodeSimilarity(0.5) */
        else if (func->args && func->args->count >= 1) {
            cypher_literal *thresh_lit = (cypher_literal *)func->args->items[0];
            if (thresh_lit && thresh_lit->base.type == AST_NODE_LITERAL) {
                if (thresh_lit->literal_type == LITERAL_DECIMAL) {
                    params.threshold = thresh_lit->value.decimal;
                } else if (thresh_lit->literal_type == LITERAL_INTEGER) {
                    params.threshold = (double)thresh_lit->value.integer;
                }
            }
        }

        /* Check for top_k as last argument */
        if (func->args && func->args->count >= 2 && !params.source_id) {
            /* nodeSimilarity(threshold, top_k) */
            cypher_literal *topk_lit = (cypher_literal *)func->args->items[1];
            if (topk_lit && topk_lit->base.type == AST_NODE_LITERAL &&
                topk_lit->literal_type == LITERAL_INTEGER) {
                params.top_k = (int)topk_lit->value.integer;
            }
        }

        return params;
    }

    /* K-Nearest Neighbors */
    if (strcasecmp(func->function_name, "knn") == 0) {
        params.type = GRAPH_ALGO_KNN;
        params.source_id = NULL;
        params.k = 10;  /* Default k */

        /* knn(node_id, k) */
        if (func->args && func->args->count >= 1) {
            cypher_literal *node_lit = (cypher_literal *)func->args->items[0];
            if (node_lit && node_lit->base.type == AST_NODE_LITERAL &&
                node_lit->literal_type == LITERAL_STRING) {
                params.source_id = strdup(node_lit->value.string);
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *k_lit = (cypher_literal *)func->args->items[1];
            if (k_lit && k_lit->base.type == AST_NODE_LITERAL &&
                k_lit->literal_type == LITERAL_INTEGER) {
                params.k = (int)k_lit->value.integer;
            }
        }

        return params;
    }

    /* Eigenvector Centrality */
    if (strcasecmp(func->function_name, "eigenvectorCentrality") == 0) {
        params.type = GRAPH_ALGO_EIGENVECTOR_CENTRALITY;
        params.iterations = 100;  /* Default iterations */

        /* Optional iterations parameter */
        if (func->args && func->args->count >= 1) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[0];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = (int)iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 1000) params.iterations = 1000;
            }
        }
        return params;
    }

    /* All Pairs Shortest Path */
    if (strcasecmp(func->function_name, "allPairsShortestPath") == 0 ||
        strcasecmp(func->function_name, "apsp") == 0) {
        params.type = GRAPH_ALGO_APSP;
        return params;
    }

    return params;
}

/* Free algorithm result */
void graph_algo_result_free(graph_algo_result *result)
{
    if (!result) return;
    free(result->error_message);
    free(result->json_result);
    free(result);
}
