/*
 * Centrality Algorithms
 *
 * Degree centrality and related measures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"

/*
 * Execute Degree Centrality algorithm
 *
 * Returns degree centrality for all nodes:
 * [{"node_id": 1, "user_id": "alice", "in_degree": 3, "out_degree": 2, "degree": 5}, ...]
 */
graph_algo_result* execute_degree_centrality(sqlite3 *db)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing Degree Centrality");

    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    size_t json_capacity = 64 + n * 96;
    char *json = malloc(json_capacity);
    if (!json) {
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;

    for (int i = 0; i < n; i++) {
        int out_degree = graph->row_ptr[i + 1] - graph->row_ptr[i];
        int in_degree = graph->in_row_ptr[i + 1] - graph->in_row_ptr[i];
        int total_degree = out_degree + in_degree;

        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        char entry[256];
        int entry_len;

        if (user_id) {
            entry_len = snprintf(entry, sizeof(entry),
                "%s{\"node_id\":%d,\"user_id\":\"%s\",\"in_degree\":%d,\"out_degree\":%d,\"degree\":%d}",
                (i > 0) ? "," : "",
                graph->node_ids[i], user_id, in_degree, out_degree, total_degree);
        } else {
            entry_len = snprintf(entry, sizeof(entry),
                "%s{\"node_id\":%d,\"user_id\":null,\"in_degree\":%d,\"out_degree\":%d,\"degree\":%d}",
                (i > 0) ? "," : "",
                graph->node_ids[i], in_degree, out_degree, total_degree);
        }

        if (json_len + entry_len >= json_capacity - 2) {
            json_capacity *= 2;
            json = realloc(json, json_capacity);
            if (!json) break;
        }

        strcpy(json + json_len, entry);
        json_len += entry_len;
    }

    strcat(json, "]");
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
