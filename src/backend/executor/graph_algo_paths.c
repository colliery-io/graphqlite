/*
 * Path Algorithms
 *
 * Dijkstra's shortest path and related algorithms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"

/*
 * Execute Dijkstra's shortest path algorithm
 *
 * Returns the shortest path from source to target as JSON:
 * {"path": ["node1", "node2", ...], "distance": 3.5, "found": true}
 *
 * If weight_prop is NULL, uses unweighted edges (distance = hop count)
 */
graph_algo_result* execute_dijkstra(sqlite3 *db, csr_graph *cached, const char *source_id, const char *target_id, const char *weight_prop)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing Dijkstra: source=%s, target=%s, weight=%s, cached=%s",
                 source_id ? source_id : "NULL",
                 target_id ? target_id : "NULL",
                 weight_prop ? weight_prop : "NULL",
                 cached ? "yes" : "no");

    if (!source_id || !target_id) {
        result->success = false;
        result->error_message = strdup("shortestPath requires source and target node IDs");
        return result;
    }

    /* Use cached graph or load from SQLite */
    csr_graph *graph;
    bool should_free_graph = false;

    if (cached) {
        graph = cached;
    } else {
        graph = csr_graph_load(db);
        should_free_graph = true;
    }

    if (!graph) {
        result->success = true;
        result->json_result = strdup("{\"path\":[],\"distance\":null,\"found\":false}");
        return result;
    }

    int n = graph->node_count;

    /* Find source and target nodes */
    int source_idx = find_node_by_user_id(graph, source_id);
    int target_idx = find_node_by_user_id(graph, target_id);

    if (source_idx < 0 || target_idx < 0) {
        if (should_free_graph) csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("{\"path\":[],\"distance\":null,\"found\":false}");
        return result;
    }

    /* Load edge weights if specified */
    double *weights = NULL;
    if (weight_prop) {
        weights = malloc(graph->edge_count * sizeof(double));
        if (weights) {
            for (int i = 0; i < graph->edge_count; i++) {
                weights[i] = 1.0;
            }

            char sql[256];
            snprintf(sql, sizeof(sql),
                "SELECT e.source_id, e.target_id, ep.value FROM edges e "
                "JOIN edge_props_real ep ON ep.edge_id = e.id "
                "JOIN property_keys pk ON pk.id = ep.key_id AND pk.key = '%s'",
                weight_prop);

            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int src_id = sqlite3_column_int(stmt, 0);
                    int tgt_id = sqlite3_column_int(stmt, 1);
                    double weight = sqlite3_column_double(stmt, 2);

                    int h = hash_int(src_id, graph->node_idx_size);
                    while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != src_id) {
                        h = (h + 1) % graph->node_idx_size;
                    }
                    int src_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

                    h = hash_int(tgt_id, graph->node_idx_size);
                    while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != tgt_id) {
                        h = (h + 1) % graph->node_idx_size;
                    }
                    int tgt_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

                    if (src_idx >= 0 && tgt_idx >= 0) {
                        for (int j = graph->row_ptr[src_idx]; j < graph->row_ptr[src_idx + 1]; j++) {
                            if (graph->col_idx[j] == tgt_idx) {
                                weights[j] = weight;
                                break;
                            }
                        }
                    }
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    /* Dijkstra's algorithm */
    double *dist = malloc(n * sizeof(double));
    int *prev = malloc(n * sizeof(int));
    int *visited = calloc(n, sizeof(int));

    if (!dist || !prev || !visited) {
        free(dist);
        free(prev);
        free(visited);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = 1e308;
        prev[i] = -1;
    }
    dist[source_idx] = 0.0;

    min_heap *heap = heap_create(n);
    if (!heap) {
        free(dist);
        free(prev);
        free(visited);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    heap_push(heap, source_idx, 0.0);

    while (heap->size > 0) {
        heap_entry cur = heap_pop(heap);
        int u = cur.node;

        if (visited[u]) continue;
        visited[u] = 1;

        if (u == target_idx) break;

        for (int j = graph->row_ptr[u]; j < graph->row_ptr[u + 1]; j++) {
            int v = graph->col_idx[j];
            double w = weights ? weights[j] : 1.0;
            double alt = dist[u] + w;

            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                heap_push(heap, v, alt);
            }
        }
    }

    heap_free(heap);
    free(visited);

    /* Check if path was found */
    if (prev[target_idx] < 0 && source_idx != target_idx) {
        free(dist);
        free(prev);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("{\"path\":[],\"distance\":null,\"found\":false}");
        return result;
    }

    /* Reconstruct path */
    int path_len = 0;
    int *path = malloc(n * sizeof(int));
    if (!path) {
        free(dist);
        free(prev);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    int cur = target_idx;
    while (cur >= 0) {
        path[path_len++] = cur;
        cur = prev[cur];
    }

    /* Reverse path */
    for (int i = 0; i < path_len / 2; i++) {
        int tmp = path[i];
        path[i] = path[path_len - 1 - i];
        path[path_len - 1 - i] = tmp;
    }

    /* Build JSON output */
    size_t json_capacity = 128 + path_len * 64;
    char *json = malloc(json_capacity);
    if (!json) {
        free(dist);
        free(prev);
        free(path);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "{\"path\":[");
    size_t json_len = strlen(json);

    for (int i = 0; i < path_len; i++) {
        const char *user_id = graph->user_ids ? graph->user_ids[path[i]] : NULL;
        char entry[128];
        int entry_len;

        if (user_id) {
            entry_len = snprintf(entry, sizeof(entry), "%s\"%s\"",
                                 (i > 0) ? "," : "", user_id);
        } else {
            entry_len = snprintf(entry, sizeof(entry), "%s%d",
                                 (i > 0) ? "," : "", graph->node_ids[path[i]]);
        }

        if (json_len + entry_len >= json_capacity - 64) {
            json_capacity *= 2;
            json = realloc(json, json_capacity);
            if (!json) break;
        }
        strcpy(json + json_len, entry);
        json_len += entry_len;
    }

    char suffix[64];
    snprintf(suffix, sizeof(suffix), "],\"distance\":%.6g,\"found\":true}", dist[target_idx]);
    strcat(json, suffix);

    free(dist);
    free(prev);
    free(path);
    free(weights);
    if (should_free_graph) csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
