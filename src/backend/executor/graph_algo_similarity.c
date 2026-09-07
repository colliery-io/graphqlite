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

/* Compute intersection and union sizes of two sorted arrays */
static void compute_intersection_union(const int *a, int count_a, const int *b, int count_b,
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

/* Compute Jaccard similarity between two nodes using the pre-sorted
 * adjacency (perf review F4: no per-pair malloc/sort). */
static double jaccard_similarity(const csr_graph *graph, const int *sorted, int node_a, int node_b) {
    int start_a = graph->row_ptr[node_a], count_a = graph->row_ptr[node_a + 1] - start_a;
    int start_b = graph->row_ptr[node_b], count_b = graph->row_ptr[node_b + 1] - start_b;

    /* No neighbours on either side: no overlap possible (undefined -> 0) */
    if (count_a == 0 || count_b == 0 || !sorted) return 0.0;

    int intersection, union_size;
    compute_intersection_union(sorted + start_a, count_a, sorted + start_b, count_b,
                               &intersection, &union_size);

    if (union_size == 0) return 0.0;
    return (double)intersection / (double)union_size;
}

/* Structure for storing similarity pairs */
typedef struct {
    int node1;
    int node2;
    double similarity;
} similarity_pair;

/* Comparison function for sorting by similarity descending. Ties break on
 * (node1, node2) so the output order does not depend on enumeration order. */
static int compare_similarity(const void *a, const void *b) {
    similarity_pair *pa = (similarity_pair *)a;
    similarity_pair *pb = (similarity_pair *)b;

    if (pb->similarity > pa->similarity) return 1;
    if (pb->similarity < pa->similarity) return -1;
    if (pa->node1 != pb->node1) return (pa->node1 > pb->node1) - (pa->node1 < pb->node1);
    return (pa->node2 > pb->node2) - (pa->node2 < pb->node2);
}

/* Append a pair to a growable list; returns 0 on success, -1 on OOM. */
static int push_pair(similarity_pair **pairs, int *count, int *cap, int n1, int n2, double sim) {
    if (*count == *cap) {
        int new_cap = *cap ? *cap * 2 : 1024;
        similarity_pair *grown = realloc(*pairs, (size_t)new_cap * sizeof(similarity_pair));
        if (!grown) return -1;
        *pairs = grown;
        *cap = new_cap;
    }
    (*pairs)[*count].node1 = n1;
    (*pairs)[*count].node2 = n2;
    (*pairs)[*count].similarity = sim;
    (*count)++;
    return 0;
}

graph_algo_result* execute_node_similarity(sqlite3 *db, csr_graph *cached, const char *node1_id,
                                            const char *node2_id, double threshold,
                                            int top_k) {
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

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
            if (should_free_graph) csr_graph_free(graph);
            return result;
        }

        int *sorted = csr_sorted_col_idx(graph);
        double sim = jaccard_similarity(graph, sorted, idx1, idx2);
        free(sorted);

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

        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    /* Case 2: All pairs above threshold.
     * With threshold > 0 only pairs sharing an out-neighbour can qualify, so
     * candidates are enumerated through shared neighbours (perf review F4)
     * and the graph cap can be much higher. With threshold == 0 every pair
     * (including similarity 0.0) is part of the output, which is inherently
     * O(N^2), so the original cap stays. */
    bool sparse_mode = (threshold > 0.0 && graph->in_row_ptr && graph->in_col_idx);
    int node_limit = sparse_mode ? 50000 : 5000;
    if (graph->node_count > node_limit) {
        char error[256];
        snprintf(error, sizeof(error),
                 "nodeSimilarity: graph too large (%d nodes, limit %d%s). "
                 "Use specific node pairs%s or reduce graph size.",
                 graph->node_count, node_limit,
                 sparse_mode ? "" : " for threshold 0",
                 sparse_mode ? "" : ", a threshold above 0,");
        result->success = false;
        result->error_message = strdup(error);
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    if (graph->node_count < 2) {
        result->success = true;
        result->json_result = strdup("[]");
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    /* Growable pair list: the previous n*(n-1)/2 preallocation was 200 MB at
     * 5K nodes even when only a handful of pairs passed the threshold. */
    similarity_pair *pairs = NULL;
    int pair_count = 0, pair_cap = 0;
    bool oom = false;

    /* Sort every adjacency list once, then every pair is a linear merge. */
    int *sorted = csr_sorted_col_idx(graph);

    if (sparse_mode) {
        /* u -> w <- v: every pair with a non-empty intersection is reachable
         * through some shared out-neighbour w. `stamp` dedupes v per u. */
        int *stamp = calloc((size_t)graph->node_count, sizeof(int));
        if (!stamp) oom = true;
        for (int u = 0; u < graph->node_count && !oom; u++) {
            for (int e = graph->row_ptr[u]; e < graph->row_ptr[u + 1] && !oom; e++) {
                int w = graph->col_idx[e];
                for (int f = graph->in_row_ptr[w]; f < graph->in_row_ptr[w + 1]; f++) {
                    int v = graph->in_col_idx[f];
                    if (v <= u || stamp[v] == u + 1) continue;
                    stamp[v] = u + 1;
                    double sim = jaccard_similarity(graph, sorted, u, v);
                    if (sim >= threshold && push_pair(&pairs, &pair_count, &pair_cap, u, v, sim) < 0) {
                        oom = true;
                        break;
                    }
                }
            }
        }
        free(stamp);
    } else {
        /* Compute all pairwise similarities (threshold 0 keeps 0.0 pairs) */
        for (int i = 0; i < graph->node_count && !oom; i++) {
            for (int j = i + 1; j < graph->node_count; j++) {
                double sim = jaccard_similarity(graph, sorted, i, j);
                if (sim >= threshold && push_pair(&pairs, &pair_count, &pair_cap, i, j, sim) < 0) {
                    oom = true;
                    break;
                }
            }
        }
    }

    free(sorted);

    if (oom) {
        result->success = false;
        result->error_message = strdup("Out of memory");
        free(pairs);
        if (should_free_graph) csr_graph_free(graph);
        return result;
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
        if (should_free_graph) csr_graph_free(graph);
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
    if (should_free_graph) csr_graph_free(graph);
    return result;
}
