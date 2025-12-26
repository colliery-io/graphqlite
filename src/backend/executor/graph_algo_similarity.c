/*
 * graph_algo_similarity.c
 *
 * Node Similarity using Jaccard coefficient.
 * Measures similarity between nodes based on shared neighbors.
 *
 * Jaccard(a, b) = |N(a) ∩ N(b)| / |N(a) ∪ N(b)|
 *
 * Where N(x) is the set of neighbors of node x.
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

    /* Copy neighbors */
    for (int i = 0; i < *count; i++) {
        neighbors[i] = graph->col_idx[start + i];
    }

    /* Sort using simple insertion sort (typically small degree) */
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
            /* Equal - in both sets */
            (*intersection)++;
            (*union_size)++;
            i++;
            j++;
        }
    }

    /* Add remaining elements */
    *union_size += (count_a - i) + (count_b - j);
}

/* Compute Jaccard similarity between two nodes */
static double jaccard_similarity(csr_graph *graph, int node_a, int node_b) {
    int count_a, count_b;
    int *neighbors_a = get_neighbors_sorted(graph, node_a, &count_a);
    int *neighbors_b = get_neighbors_sorted(graph, node_b, &count_b);

    /* Handle edge cases */
    if (count_a == 0 && count_b == 0) {
        /* Both have no neighbors - undefined, return 0 */
        return 0.0;
    }

    if (count_a == 0 || count_b == 0) {
        /* One has no neighbors - no overlap possible */
        if (neighbors_a) free(neighbors_a);
        if (neighbors_b) free(neighbors_b);
        return 0.0;
    }

    int intersection, union_size;
    compute_intersection_union(neighbors_a, count_a, neighbors_b, count_b,
                               &intersection, &union_size);

    free(neighbors_a);
    free(neighbors_b);

    if (union_size == 0) return 0.0;

    return (double)intersection / (double)union_size;
}

/* Structure for storing similarity pairs */
typedef struct {
    int node1;
    int node2;
    double similarity;
} similarity_pair;

/* Comparison function for sorting by similarity descending */
static int compare_similarity(const void *a, const void *b) {
    similarity_pair *pa = (similarity_pair *)a;
    similarity_pair *pb = (similarity_pair *)b;

    if (pb->similarity > pa->similarity) return 1;
    if (pb->similarity < pa->similarity) return -1;
    return 0;
}

graph_algo_result* execute_node_similarity(sqlite3 *db, const char *node1_id,
                                            const char *node2_id, double threshold,
                                            int top_k) {
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        /* Empty graph - return empty array */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    /* Case 1: Specific pair requested */
    if (node1_id && node2_id) {
        int idx1 = -1, idx2 = -1;

        /* Find node indices */
        for (int i = 0; i < graph->node_count; i++) {
            if (graph->user_ids[i] && strcmp(graph->user_ids[i], node1_id) == 0) {
                idx1 = i;
            }
            if (graph->user_ids[i] && strcmp(graph->user_ids[i], node2_id) == 0) {
                idx2 = i;
            }
        }

        if (idx1 < 0 || idx2 < 0) {
            result->success = true;
            result->json_result = strdup("[]");
            csr_graph_free(graph);
            return result;
        }

        double sim = jaccard_similarity(graph, idx1, idx2);

        /* Build JSON result */
        char *json = malloc(256);
        if (json) {
            snprintf(json, 256,
                "[{\"node1\":\"%s\",\"node2\":\"%s\",\"similarity\":%.6f}]",
                node1_id, node2_id, sim);
            result->json_result = json;
            result->success = true;
        } else {
            result->success = false;
            result->error_message = strdup("Out of memory");
        }

        csr_graph_free(graph);
        return result;
    }

    /* Case 2: All pairs above threshold */
    /* Allocate space for pairs - worst case is n*(n-1)/2 */
    int max_pairs = (graph->node_count * (graph->node_count - 1)) / 2;
    if (max_pairs == 0) {
        result->success = true;
        result->json_result = strdup("[]");
        csr_graph_free(graph);
        return result;
    }

    similarity_pair *pairs = malloc(max_pairs * sizeof(similarity_pair));
    if (!pairs) {
        result->success = false;
        result->error_message = strdup("Out of memory");
        csr_graph_free(graph);
        return result;
    }

    int pair_count = 0;

    /* Compute all pairwise similarities */
    for (int i = 0; i < graph->node_count; i++) {
        for (int j = i + 1; j < graph->node_count; j++) {
            double sim = jaccard_similarity(graph, i, j);

            if (sim >= threshold) {
                pairs[pair_count].node1 = i;
                pairs[pair_count].node2 = j;
                pairs[pair_count].similarity = sim;
                pair_count++;
            }
        }
    }

    /* Sort by similarity descending */
    if (pair_count > 0) {
        qsort(pairs, pair_count, sizeof(similarity_pair), compare_similarity);
    }

    /* Apply top_k limit if specified */
    if (top_k > 0 && pair_count > top_k) {
        pair_count = top_k;
    }

    /* Build JSON result */
    size_t json_size = 128 + pair_count * 200;
    char *json = malloc(json_size);
    if (!json) {
        result->success = false;
        result->error_message = strdup("Out of memory");
        free(pairs);
        csr_graph_free(graph);
        return result;
    }

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < pair_count; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");

        const char *id1 = graph->user_ids[pairs[i].node1] ?
                          graph->user_ids[pairs[i].node1] : "";
        const char *id2 = graph->user_ids[pairs[i].node2] ?
                          graph->user_ids[pairs[i].node2] : "";

        ptr += sprintf(ptr, "{\"node1\":\"%s\",\"node2\":\"%s\",\"similarity\":%.6f}",
                       id1, id2, pairs[i].similarity);
    }

    sprintf(ptr, "]");

    result->json_result = json;
    result->success = true;

    free(pairs);
    csr_graph_free(graph);
    return result;
}
