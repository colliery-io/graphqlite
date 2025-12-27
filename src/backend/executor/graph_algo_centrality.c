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
#include "executor/json_builder.h"

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

    json_builder jb;
    jbuf_init(&jb, 64 + n * 96);
    if (!jbuf_ok(&jb)) {
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    jbuf_start_array(&jb);

    for (int i = 0; i < n; i++) {
        int out_degree = graph->row_ptr[i + 1] - graph->row_ptr[i];
        int in_degree = graph->in_row_ptr[i + 1] - graph->in_row_ptr[i];
        int total_degree = out_degree + in_degree;

        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;

        if (user_id) {
            jbuf_add_item(&jb,
                "{\"node_id\":%d,\"user_id\":\"%s\",\"in_degree\":%d,\"out_degree\":%d,\"degree\":%d}",
                graph->node_ids[i], user_id, in_degree, out_degree, total_degree);
        } else {
            jbuf_add_item(&jb,
                "{\"node_id\":%d,\"user_id\":null,\"in_degree\":%d,\"out_degree\":%d,\"degree\":%d}",
                graph->node_ids[i], in_degree, out_degree, total_degree);
        }
    }

    jbuf_end_array(&jb);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = jbuf_take(&jb);
    return result;
}
