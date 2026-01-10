/*
 * All Pairs Shortest Path Algorithm Implementation
 *
 * Uses Floyd-Warshall algorithm to compute shortest paths between all pairs.
 * O(V³) time complexity, O(V²) space complexity.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"

#define APSP_INF DBL_MAX

/*
 * Execute All Pairs Shortest Path using Floyd-Warshall algorithm.
 *
 * Returns distances between all reachable pairs of nodes.
 * Only includes pairs where a path exists (distance < infinity).
 */
graph_algo_result* execute_apsp(sqlite3 *db, csr_graph *cached)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based All Pairs Shortest Path (Floyd-Warshall): cached=%s",
                 cached ? "yes" : "no");

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
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Warn if graph is too large */
    if (n > 10000) {
        CYPHER_DEBUG("Warning: APSP on %d nodes requires O(n³) = %.0f operations",
                     n, (double)n * n * n);
    }

    /* Allocate distance matrix - O(V²) space */
    double *dist = malloc(n * n * sizeof(double));
    if (!dist) {
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed for distance matrix");
        return result;
    }

    /* Initialize distances */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) {
                dist[i * n + j] = 0.0;
            } else {
                dist[i * n + j] = APSP_INF;
            }
        }
    }

    /* Set direct edge distances to 1 (unweighted) */
    for (int i = 0; i < n; i++) {
        int start = graph->row_ptr[i];
        int end = graph->row_ptr[i + 1];
        for (int e = start; e < end; e++) {
            int j = graph->col_idx[e];
            dist[i * n + j] = 1.0;
        }
    }

    /* Floyd-Warshall main loop - O(V³) */
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            double dist_ik = dist[i * n + k];
            if (dist_ik >= APSP_INF) continue;  /* Optimization: skip if no path to k */

            for (int j = 0; j < n; j++) {
                double dist_kj = dist[k * n + j];
                if (dist_kj >= APSP_INF) continue;  /* Optimization: skip if no path from k */

                double new_dist = dist_ik + dist_kj;
                if (new_dist < dist[i * n + j]) {
                    dist[i * n + j] = new_dist;
                }
            }
        }
    }

    CYPHER_DEBUG("Floyd-Warshall completed for %d nodes", n);

    /* Count reachable pairs (excluding self-loops) */
    int pair_count = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j && dist[i * n + j] < APSP_INF) {
                pair_count++;
            }
        }
    }

    CYPHER_DEBUG("Found %d reachable pairs", pair_count);

    /* Build JSON output */
    /* Estimate: each entry ~80 chars: {"source":"...","target":"...","distance":...} */
    size_t json_capacity = 64 + pair_count * 100;
    char *json = malloc(json_capacity);
    if (!json) {
        free(dist);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed for JSON result");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;
    int first = 1;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;  /* Skip self-loops */

            double d = dist[i * n + j];
            if (d >= APSP_INF) continue;  /* Skip unreachable pairs */

            const char *source_id = graph->user_ids ? graph->user_ids[i] : NULL;
            const char *target_id = graph->user_ids ? graph->user_ids[j] : NULL;

            char entry[256];
            int entry_len;

            if (source_id && target_id) {
                entry_len = snprintf(entry, sizeof(entry),
                                     "%s{\"source\":\"%s\",\"target\":\"%s\",\"distance\":%.10g}",
                                     first ? "" : ",",
                                     source_id, target_id, d);
            } else {
                /* Fallback to node IDs if user_ids not available */
                entry_len = snprintf(entry, sizeof(entry),
                                     "%s{\"source\":%d,\"target\":%d,\"distance\":%.10g}",
                                     first ? "" : ",",
                                     graph->node_ids[i], graph->node_ids[j], d);
            }

            first = 0;

            /* Grow buffer if needed */
            if (json_len + entry_len >= json_capacity - 2) {
                json_capacity *= 2;
                char *new_json = realloc(json, json_capacity);
                if (!new_json) {
                    free(json);
                    free(dist);
                    if (should_free_graph) csr_graph_free(graph);
                    result->success = false;
                    result->error_message = strdup("Memory reallocation failed");
                    return result;
                }
                json = new_json;
            }

            memcpy(json + json_len, entry, entry_len);
            json_len += entry_len;
        }
    }

    json[json_len] = ']';
    json[json_len + 1] = '\0';

    free(dist);
    if (should_free_graph) csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
