/*
 * PageRank Algorithm Implementation
 *
 * Optimized push-based PageRank with early convergence detection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"

/* Result structure for sorting */
typedef struct {
    int node_id;
    const char *user_id;
    double score;
} pr_result;

static int compare_pr_desc(const void *a, const void *b)
{
    double diff = ((pr_result *)b)->score - ((pr_result *)a)->score;
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}

/*
 * Execute PageRank algorithm (optimized)
 *
 * Formula: PR(n) = (1-d)/N + d * SUM(PR(m)/out_degree(m)) for all m -> n
 *
 * Optimizations:
 * - Uses float instead of double (2x memory bandwidth)
 * - Pre-computes 1/out_degree to avoid division in inner loop
 * - Early convergence detection (stops if max change < 1e-6)
 * - Push-based approach for better cache locality on outgoing edges
 */
graph_algo_result* execute_pagerank(sqlite3 *db, double damping, int iterations, int top_k)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based PageRank: damping=%.2f, iterations=%d, top_k=%d",
                 damping, iterations, top_k);

    /* Load graph into CSR format */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;
    float dampf = (float)damping;

    /* Allocate PageRank arrays - use float for 2x memory bandwidth */
    float *pr = malloc(n * sizeof(float));
    float *pr_new = malloc(n * sizeof(float));
    float *inv_out_degree = malloc(n * sizeof(float));

    if (!pr || !pr_new || !inv_out_degree) {
        free(pr);
        free(pr_new);
        free(inv_out_degree);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Pre-compute inverse out-degrees */
    for (int i = 0; i < n; i++) {
        int out_deg = graph->row_ptr[i + 1] - graph->row_ptr[i];
        inv_out_degree[i] = (out_deg > 0) ? (1.0f / out_deg) : 0.0f;
    }

    /* Initialize PageRank: uniform distribution */
    float init_pr = 1.0f / n;
    for (int i = 0; i < n; i++) {
        pr[i] = init_pr;
    }

    /* PageRank iterations with convergence detection */
    float teleport = (1.0f - dampf) / n;
    float convergence_threshold = 1e-6f;
    int actual_iters = 0;

    for (int iter = 0; iter < iterations; iter++) {
        actual_iters++;

        /* Initialize new PR with teleport probability */
        for (int i = 0; i < n; i++) {
            pr_new[i] = teleport;
        }

        /* Push-based: each node distributes its rank to neighbors */
        for (int i = 0; i < n; i++) {
            float contribution = dampf * pr[i] * inv_out_degree[i];
            int out_start = graph->row_ptr[i];
            int out_end = graph->row_ptr[i + 1];

            for (int j = out_start; j < out_end; j++) {
                int target = graph->col_idx[j];
                pr_new[target] += contribution;
            }
        }

        /* Check convergence and swap */
        float max_diff = 0.0f;
        for (int i = 0; i < n; i++) {
            float diff = pr_new[i] - pr[i];
            if (diff < 0) diff = -diff;
            if (diff > max_diff) max_diff = diff;
        }

        float *tmp = pr;
        pr = pr_new;
        pr_new = tmp;

        if (max_diff < convergence_threshold) {
            CYPHER_DEBUG("PageRank converged at iteration %d (max_diff=%.2e)", iter, max_diff);
            break;
        }
    }

    CYPHER_DEBUG("PageRank completed in %d iterations", actual_iters);

    /* Build results array for sorting */
    pr_result *results = malloc(n * sizeof(pr_result));
    if (!results) {
        free(pr);
        free(pr_new);
        free(inv_out_degree);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        results[i].node_id = graph->node_ids[i];
        results[i].user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        results[i].score = (double)pr[i];
    }

    qsort(results, n, sizeof(pr_result), compare_pr_desc);

    int result_count = (top_k > 0 && top_k < n) ? top_k : n;

    /* Build JSON output */
    size_t json_capacity = 64 + result_count * 64;
    char *json = malloc(json_capacity);
    if (!json) {
        free(results);
        free(pr);
        free(pr_new);
        free(inv_out_degree);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;

    for (int i = 0; i < result_count; i++) {
        char entry[512];
        int entry_len;
        if (results[i].user_id) {
            entry_len = snprintf(entry, sizeof(entry),
                                 "%s{\"node_id\":%d,\"user_id\":\"%s\",\"score\":%.10g}",
                                 (i > 0) ? "," : "",
                                 results[i].node_id,
                                 results[i].user_id,
                                 results[i].score);
        } else {
            entry_len = snprintf(entry, sizeof(entry),
                                 "%s{\"node_id\":%d,\"user_id\":null,\"score\":%.10g}",
                                 (i > 0) ? "," : "",
                                 results[i].node_id,
                                 results[i].score);
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

    free(results);
    free(pr);
    free(pr_new);
    free(inv_out_degree);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
