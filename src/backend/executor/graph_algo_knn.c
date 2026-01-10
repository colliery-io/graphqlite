/*
 * graph_algo_knn.c
 *
 * K-Nearest Neighbors algorithm.
 * Finds the K most similar nodes to a given node using Jaccard similarity.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "executor/graph_algorithms.h"

/* Helper to get neighbors as a sorted array for efficient intersection */
static int* get_neighbors_sorted(csr_graph *graph, int node_idx, int *count) {
    int start = graph->row_ptr[node_idx];
    int end = graph->row_ptr[node_idx + 1];
    *count = end - start;

    if (*count == 0) return NULL;

    int *neighbors = malloc(*count * sizeof(int));
    if (!neighbors) return NULL;

    for (int i = 0; i < *count; i++) {
        neighbors[i] = graph->col_idx[start + i];
    }

    /* Sort using insertion sort (typically small degree) */
    for (int i = 1; i < *count; i++) {
        int key = neighbors[i];
        int j = i - 1;
        while (j >= 0 && neighbors[j] > key) {
            neighbors[j + 1] = neighbors[j];
            j--;
        }
        neighbors[j + 1] = key;
    }

    return neighbors;
}

/* Compute intersection and union sizes of two sorted arrays */
static void compute_intersection_union(int *a, int count_a, int *b, int count_b,
                                        int *intersection, int *union_size) {
    int i = 0, j = 0;
    *intersection = 0;
    *union_size = 0;

    while (i < count_a && j < count_b) {
        if (a[i] < b[j]) {
            (*union_size)++;
            i++;
        } else if (a[i] > b[j]) {
            (*union_size)++;
            j++;
        } else {
            (*intersection)++;
            (*union_size)++;
            i++;
            j++;
        }
    }

    *union_size += (count_a - i) + (count_b - j);
}

/* Compute Jaccard similarity between source node and another node */
static double jaccard_similarity(csr_graph *graph, int node_b,
                                  int *neighbors_a, int count_a) {
    int count_b;
    int *neighbors_b = get_neighbors_sorted(graph, node_b, &count_b);

    if (count_a == 0 && count_b == 0) {
        return 0.0;
    }

    if (count_a == 0 || count_b == 0) {
        if (neighbors_b) free(neighbors_b);
        return 0.0;
    }

    int intersection, union_size;
    compute_intersection_union(neighbors_a, count_a, neighbors_b, count_b,
                               &intersection, &union_size);

    free(neighbors_b);

    if (union_size == 0) return 0.0;

    return (double)intersection / (double)union_size;
}

/* Structure for storing neighbor similarities */
typedef struct {
    int node_idx;
    double similarity;
} neighbor_sim;

/* Comparison function for sorting by similarity descending */
static int compare_neighbors(const void *a, const void *b) {
    neighbor_sim *na = (neighbor_sim *)a;
    neighbor_sim *nb = (neighbor_sim *)b;

    if (nb->similarity > na->similarity) return 1;
    if (nb->similarity < na->similarity) return -1;
    return 0;
}

graph_algo_result* execute_knn(sqlite3 *db, csr_graph *cached, const char *node_id, int k) {
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    if (!node_id || k <= 0) {
        result->success = false;
        result->error_message = strdup("KNN requires a node_id and k > 0");
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
        result->json_result = strdup("[]");
        return result;
    }

    /* Find the source node index */
    int source_idx = -1;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->user_ids[i] && strcmp(graph->user_ids[i], node_id) == 0) {
            source_idx = i;
            break;
        }
    }

    if (source_idx < 0) {
        result->success = true;
        result->json_result = strdup("[]");
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    /* Get source node's neighbors once */
    int source_count;
    int *source_neighbors = get_neighbors_sorted(graph, source_idx, &source_count);

    /* Compute similarity to all other nodes */
    neighbor_sim *similarities = malloc((graph->node_count - 1) * sizeof(neighbor_sim));
    if (!similarities) {
        result->success = false;
        result->error_message = strdup("Out of memory");
        if (source_neighbors) free(source_neighbors);
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    int sim_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (i == source_idx) continue;

        double sim = jaccard_similarity(graph, i, source_neighbors, source_count);

        /* Only include nodes with non-zero similarity */
        if (sim > 0.0) {
            similarities[sim_count].node_idx = i;
            similarities[sim_count].similarity = sim;
            sim_count++;
        }
    }

    if (source_neighbors) free(source_neighbors);

    /* Sort by similarity descending */
    if (sim_count > 0) {
        qsort(similarities, sim_count, sizeof(neighbor_sim), compare_neighbors);
    }

    /* Limit to k neighbors */
    int result_count = (sim_count < k) ? sim_count : k;

    /* Build JSON result */
    size_t json_size = 64 + result_count * 128;
    char *json = malloc(json_size);
    if (!json) {
        result->success = false;
        result->error_message = strdup("Out of memory");
        free(similarities);
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < result_count; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");

        const char *neighbor_id = graph->user_ids[similarities[i].node_idx] ?
                                  graph->user_ids[similarities[i].node_idx] : "";

        ptr += sprintf(ptr, "{\"neighbor\":\"%s\",\"similarity\":%.6f,\"rank\":%d}",
                       neighbor_id, similarities[i].similarity, i + 1);
    }

    sprintf(ptr, "]");

    result->json_result = json;
    result->success = true;

    free(similarities);
    if (should_free_graph) csr_graph_free(graph);
    return result;
}
