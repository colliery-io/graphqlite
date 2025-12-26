/*
 * graph_algo_triangle.c
 *
 * Triangle Count Algorithm Implementation
 *
 * Counts triangles each node participates in and computes local clustering coefficients.
 * A triangle is a set of 3 nodes that are all connected to each other.
 *
 * Algorithm: Node-iterator approach (treats graph as undirected)
 * For each node u:
 *   For each pair of neighbors (v, w) where v < w:
 *     If edge (v, w) exists, increment triangle count for u, v, and w
 *
 * Clustering coefficient for node u = 2 * triangles[u] / (degree[u] * (degree[u] - 1))
 *
 * Complexity: O(d_max * E) where d_max is max degree
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "executor/graph_algorithms.h"

/* Check if edge exists between two nodes (undirected) */
static int edge_exists(csr_graph *graph, int u, int v) {
    /* Check outgoing edges from u */
    for (int i = graph->row_ptr[u]; i < graph->row_ptr[u + 1]; i++) {
        if (graph->col_idx[i] == v) return 1;
    }
    /* Check incoming edges to u (for undirected treatment) */
    for (int i = graph->in_row_ptr[u]; i < graph->in_row_ptr[u + 1]; i++) {
        if (graph->in_col_idx[i] == v) return 1;
    }
    return 0;
}

/* Get all neighbors of a node (both directions for undirected) */
static int* get_neighbors(csr_graph *graph, int node, int *neighbor_count) {
    /* Count total neighbors */
    int out_count = graph->row_ptr[node + 1] - graph->row_ptr[node];
    int in_count = graph->in_row_ptr[node + 1] - graph->in_row_ptr[node];
    int max_neighbors = out_count + in_count;

    int *neighbors = malloc(max_neighbors * sizeof(int));
    if (!neighbors) {
        *neighbor_count = 0;
        return NULL;
    }

    int count = 0;

    /* Add outgoing neighbors */
    for (int i = graph->row_ptr[node]; i < graph->row_ptr[node + 1]; i++) {
        neighbors[count++] = graph->col_idx[i];
    }

    /* Add incoming neighbors (avoid duplicates) */
    for (int i = graph->in_row_ptr[node]; i < graph->in_row_ptr[node + 1]; i++) {
        int neighbor = graph->in_col_idx[i];
        int is_dup = 0;
        for (int j = 0; j < count; j++) {
            if (neighbors[j] == neighbor) {
                is_dup = 1;
                break;
            }
        }
        if (!is_dup) {
            neighbors[count++] = neighbor;
        }
    }

    *neighbor_count = count;
    return neighbors;
}

graph_algo_result* execute_triangle_count(sqlite3 *db) {
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

    /* Load graph */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        /* Empty graph - return empty result */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Handle empty graph */
    if (n == 0) {
        csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    /* Allocate triangle counts and degrees */
    int *triangles = calloc(n, sizeof(int));
    int *degrees = calloc(n, sizeof(int));

    if (!triangles || !degrees) {
        free(triangles);
        free(degrees);
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate memory");
        return result;
    }

    /* Calculate degrees (undirected) */
    for (int u = 0; u < n; u++) {
        int neighbor_count;
        int *neighbors = get_neighbors(graph, u, &neighbor_count);
        degrees[u] = neighbor_count;
        free(neighbors);
    }

    /* Count triangles using node-iterator algorithm */
    /* For each node u, check all pairs of neighbors (v, w) */
    for (int u = 0; u < n; u++) {
        int neighbor_count;
        int *neighbors = get_neighbors(graph, u, &neighbor_count);

        if (!neighbors) continue;

        /* For each pair of neighbors */
        for (int i = 0; i < neighbor_count; i++) {
            int v = neighbors[i];
            for (int j = i + 1; j < neighbor_count; j++) {
                int w = neighbors[j];

                /* Check if v and w are connected (forming a triangle) */
                if (edge_exists(graph, v, w)) {
                    /* Found triangle (u, v, w) - count for u only here
                     * Each triangle will be counted once per participating node */
                    triangles[u]++;
                }
            }
        }

        free(neighbors);
    }

    /* Build JSON result */
    size_t buf_size = 256 + n * 200;
    char *json = malloc(buf_size);
    if (!json) {
        free(triangles);
        free(degrees);
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate JSON buffer");
        return result;
    }

    strcpy(json, "[");
    size_t pos = 1;

    for (int i = 0; i < n; i++) {
        /* Calculate clustering coefficient */
        double clustering = 0.0;
        int d = degrees[i];
        if (d >= 2) {
            /* Max possible triangles = d*(d-1)/2 */
            /* clustering = triangles / max_possible */
            clustering = (2.0 * triangles[i]) / (d * (d - 1));
        }

        /* Get user_id */
        const char *user_id = graph->user_ids[i] ? graph->user_ids[i] : "";

        if (i > 0) {
            json[pos++] = ',';
        }

        int written = snprintf(json + pos, buf_size - pos,
            "{\"node_id\":%d,\"user_id\":\"%s\",\"triangles\":%d,\"clustering_coefficient\":%.6f}",
            graph->node_ids[i], user_id, triangles[i], clustering);

        if (written < 0 || (size_t)written >= buf_size - pos) {
            /* Buffer overflow, reallocate */
            buf_size *= 2;
            char *new_json = realloc(json, buf_size);
            if (!new_json) {
                free(json);
                free(triangles);
                free(degrees);
                csr_graph_free(graph);
                result->error_message = strdup("Failed to reallocate JSON buffer");
                return result;
            }
            json = new_json;
            written = snprintf(json + pos, buf_size - pos,
                "{\"node_id\":%d,\"user_id\":\"%s\",\"triangles\":%d,\"clustering_coefficient\":%.6f}",
                graph->node_ids[i], user_id, triangles[i], clustering);
        }
        pos += written;
    }

    json[pos++] = ']';
    json[pos] = '\0';

    /* Cleanup */
    free(triangles);
    free(degrees);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
