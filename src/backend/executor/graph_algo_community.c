/*
 * Community Detection Algorithms
 *
 * Label Propagation and related clustering algorithms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"

/*
 * Execute Label Propagation community detection
 *
 * Each node adopts the most common label among its neighbors.
 * Optimized with sparse label counting for O(E) per iteration.
 *
 * GQLITE-T-0383: updates are asynchronous (a node sees the labels already
 * updated in this round) and a node keeps its label on a tie. Synchronous
 * updates oscillated on bipartite shapes (stars, paths), so a hub and its
 * leaves swapped labels. Self-loops do not vote. A final pass splits a
 * label held by nodes with no path between them, so every community is
 * connected in the undirected graph.
 */
graph_algo_result* execute_label_propagation(sqlite3 *db, csr_graph *cached, int iterations)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based Label Propagation: iterations=%d, cached=%s",
                 iterations, cached ? "yes" : "no");

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

    int *labels = malloc(n * sizeof(int));
    int *new_labels = malloc(n * sizeof(int));

    if (!labels || !new_labels) {
        free(labels);
        free(new_labels);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Initialize: each node has its own label */
    for (int i = 0; i < n; i++) {
        labels[i] = i;
    }

    /* Sparse label counting arrays */
    int *label_counts = calloc(n, sizeof(int));
    int *touched_labels = malloc(n * sizeof(int));

    if (!label_counts || !touched_labels) {
        free(labels);
        free(new_labels);
        free(label_counts);
        free(touched_labels);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Label propagation iterations */
    for (int iter = 0; iter < iterations; iter++) {
        int changes = 0;

        for (int i = 0; i < n; i++) {
            int in_start = graph->in_row_ptr[i];
            int in_end = graph->in_row_ptr[i + 1];
            int out_start = graph->row_ptr[i];
            int out_end = graph->row_ptr[i + 1];

            int neighbor_count = (in_end - in_start) + (out_end - out_start);

            if (neighbor_count == 0) continue;

            int touched_count = 0;

            /* Count incoming neighbor labels */
            for (int j = in_start; j < in_end; j++) {
                if (graph->in_col_idx[j] == i) continue;
                int label = labels[graph->in_col_idx[j]];
                if (label_counts[label] == 0) {
                    touched_labels[touched_count++] = label;
                }
                label_counts[label]++;
            }

            /* Count outgoing neighbor labels */
            for (int j = out_start; j < out_end; j++) {
                if (graph->col_idx[j] == i) continue;
                int label = labels[graph->col_idx[j]];
                if (label_counts[label] == 0) {
                    touched_labels[touched_count++] = label;
                }
                label_counts[label]++;
            }

            /* Find best label: highest count; on a tie keep the current
             * label, else take the smallest. */
            int best_label = labels[i];
            int best_count = label_counts[labels[i]];

            for (int t = 0; t < touched_count; t++) {
                int label = touched_labels[t];
                int count = label_counts[label];
                if (count > best_count || (count == best_count && best_label != labels[i] && label < best_label)) {
                    best_count = count;
                    best_label = label;
                }
            }

            /* Reset touched labels */
            for (int t = 0; t < touched_count; t++) {
                label_counts[touched_labels[t]] = 0;
            }

            if (best_label != labels[i]) {
                labels[i] = best_label;  /* asynchronous update */
                changes++;
            }
        }

        CYPHER_DEBUG("Label propagation iter %d: %d changes", iter, changes);

        if (changes == 0) break;
    }

    free(label_counts);
    free(touched_labels);

    /* Split each label into its connected parts (undirected): a BFS over
     * neighbors with the same label gives each part its own label.
     * new_labels is reused as the part id (-1 = not yet visited). */
    {
        int *queue = malloc((n > 0 ? n : 1) * sizeof(int));
        if (queue) {
            for (int i = 0; i < n; i++) new_labels[i] = -1;
            for (int start = 0; start < n; start++) {
                if (new_labels[start] >= 0) continue;
                int head = 0, tail = 0;
                queue[tail++] = start;
                new_labels[start] = start;
                while (head < tail) {
                    int u = queue[head++];
                    for (int dir = 0; dir < 2; dir++) {
                        const int *ptr = dir == 0 ? graph->row_ptr : graph->in_row_ptr;
                        const int *idx = dir == 0 ? graph->col_idx : graph->in_col_idx;
                        for (int j = ptr[u]; j < ptr[u + 1]; j++) {
                            int v = idx[j];
                            if (new_labels[v] < 0 && labels[v] == labels[start]) {
                                new_labels[v] = start;
                                queue[tail++] = v;
                            }
                        }
                    }
                }
            }
            for (int i = 0; i < n; i++) labels[i] = new_labels[i];
            free(queue);
        }
    }

    /* Map labels to community IDs */
    int *label_to_community = malloc(n * sizeof(int));
    if (!label_to_community) {
        free(labels);
        free(new_labels);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        label_to_community[i] = -1;
    }

    int num_communities = 0;
    for (int i = 0; i < n; i++) {
        int label = labels[i];
        if (label_to_community[label] < 0) {
            label_to_community[label] = num_communities++;
        }
    }

    CYPHER_DEBUG("Label propagation found %d communities", num_communities);

    /* Build JSON output */
    size_t json_capacity = 64 + n * 48;
    char *json = malloc(json_capacity);
    if (!json) {
        free(labels);
        free(new_labels);
        free(label_to_community);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;

    for (int i = 0; i < n; i++) {
        char entry[512];
        int community_id = label_to_community[labels[i]];
        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        int entry_len;

        if (user_id) {
            entry_len = snprintf(entry, sizeof(entry),
                                 "%s{\"node_id\":%d,\"user_id\":\"%s\",\"community\":%d}",
                                 (i > 0) ? "," : "",
                                 graph->node_ids[i], user_id, community_id);
        } else {
            entry_len = snprintf(entry, sizeof(entry),
                                 "%s{\"node_id\":%d,\"user_id\":null,\"community\":%d}",
                                 (i > 0) ? "," : "",
                                 graph->node_ids[i], community_id);
        }

        if (json_len + entry_len >= json_capacity - 2) {
            json_capacity *= 2;
            json = realloc(json, json_capacity);
            if (!json) break;
        }

        strcat(json + json_len, entry);
        json_len += entry_len;
    }

    strcat(json, "]");

    free(labels);
    free(new_labels);
    free(label_to_community);
    if (should_free_graph) csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
