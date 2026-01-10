/*
 * graph_algo_closeness.c
 *
 * Closeness Centrality using harmonic centrality variant.
 * Measures how close a node is to all other reachable nodes.
 * O(V * (V + E)) complexity - BFS from each node.
 *
 * Uses harmonic centrality to handle disconnected graphs:
 * H(v) = sum of 1/d(v,u) for all reachable nodes u
 * Normalized by (n-1) to produce values in [0,1]
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "executor/graph_algo_internal.h"

graph_algo_result* execute_closeness_centrality(sqlite3 *db, csr_graph *cached)
{
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

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
        /* Empty graph - no nodes exist */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Allocate closeness scores (initialized to 0) */
    double *closeness = calloc(n, sizeof(double));
    if (!closeness) {
        result->error_message = strdup("Failed to allocate closeness array");
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    /* Allocate working arrays for BFS */
    int *dist = malloc(n * sizeof(int));
    int *queue = malloc(n * sizeof(int));

    if (!dist || !queue) {
        free(closeness);
        free(dist);
        free(queue);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate working arrays");
        return result;
    }

    /* Calculate harmonic closeness for each node */
    for (int s = 0; s < n; s++) {
        /* Initialize distances to -1 (unvisited) */
        for (int i = 0; i < n; i++) {
            dist[i] = -1;
        }

        dist[s] = 0;
        int queue_front = 0, queue_back = 0;
        queue[queue_back++] = s;

        double harmonic_sum = 0.0;

        /* BFS from source node s */
        while (queue_front < queue_back) {
            int u = queue[queue_front++];

            /* Explore outgoing edges */
            for (int j = graph->row_ptr[u]; j < graph->row_ptr[u + 1]; j++) {
                int v = graph->col_idx[j];

                if (dist[v] < 0) {
                    dist[v] = dist[u] + 1;
                    queue[queue_back++] = v;

                    /* Add contribution to harmonic sum */
                    harmonic_sum += 1.0 / (double)dist[v];
                }
            }

            /* Also explore incoming edges (treat as undirected for closeness) */
            for (int j = graph->in_row_ptr[u]; j < graph->in_row_ptr[u + 1]; j++) {
                int v = graph->in_col_idx[j];

                if (dist[v] < 0) {
                    dist[v] = dist[u] + 1;
                    queue[queue_back++] = v;

                    /* Add contribution to harmonic sum */
                    harmonic_sum += 1.0 / (double)dist[v];
                }
            }
        }

        /* Normalize by (n-1) to get value in [0,1] */
        if (n > 1) {
            closeness[s] = harmonic_sum / (double)(n - 1);
        } else {
            closeness[s] = 0.0;
        }
    }

    /* Build JSON result */
    size_t buf_size = 256 + n * 128;
    char *json = malloc(buf_size);
    if (!json) {
        free(closeness);
        free(dist);
        free(queue);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate result buffer");
        return result;
    }

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < n; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");

        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        if (user_id) {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":\"%s\",\"score\":%.6f}",
                          graph->node_ids[i], user_id, closeness[i]);
        } else {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":null,\"score\":%.6f}",
                          graph->node_ids[i], closeness[i]);
        }
    }

    ptr += sprintf(ptr, "]");

    result->success = true;
    result->json_result = json;

    /* Cleanup */
    free(closeness);
    free(dist);
    free(queue);
    if (should_free_graph) csr_graph_free(graph);

    return result;
}
