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
 */
graph_algo_result* execute_label_propagation(sqlite3 *db, int iterations)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based Label Propagation: iterations=%d", iterations);

    csr_graph *graph = csr_graph_load(db);
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
        csr_graph_free(graph);
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
        csr_graph_free(graph);
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

            if (neighbor_count == 0) {
                new_labels[i] = labels[i];
                continue;
            }

            int touched_count = 0;

            /* Count incoming neighbor labels */
            for (int j = in_start; j < in_end; j++) {
                int label = labels[graph->in_col_idx[j]];
                if (label_counts[label] == 0) {
                    touched_labels[touched_count++] = label;
                }
                label_counts[label]++;
            }

            /* Count outgoing neighbor labels */
            for (int j = out_start; j < out_end; j++) {
                int label = labels[graph->col_idx[j]];
                if (label_counts[label] == 0) {
                    touched_labels[touched_count++] = label;
                }
                label_counts[label]++;
            }

            /* Find best label */
            int best_label = labels[i];
            int best_count = 0;

            for (int t = 0; t < touched_count; t++) {
                int label = touched_labels[t];
                int count = label_counts[label];
                if (count > best_count || (count == best_count && label < best_label)) {
                    best_count = count;
                    best_label = label;
                }
            }

            /* Reset touched labels */
            for (int t = 0; t < touched_count; t++) {
                label_counts[touched_labels[t]] = 0;
            }

            new_labels[i] = best_label;
            if (new_labels[i] != labels[i]) changes++;
        }

        int *tmp = labels;
        labels = new_labels;
        new_labels = tmp;

        CYPHER_DEBUG("Label propagation iter %d: %d changes", iter, changes);

        if (changes == 0) break;
    }

    free(label_counts);
    free(touched_labels);

    /* Map labels to community IDs */
    int *label_to_community = malloc(n * sizeof(int));
    if (!label_to_community) {
        free(labels);
        free(new_labels);
        csr_graph_free(graph);
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
        csr_graph_free(graph);
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
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
