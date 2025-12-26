/*
 * Eigenvector Centrality Algorithm Implementation
 *
 * Uses power iteration method to compute eigenvector centrality.
 * Similar to PageRank but without damping factor/teleportation.
 *
 * Formula: x[i] = (1/λ) * Σ A[i,j] * x[j]
 * where λ is the largest eigenvalue (computed implicitly via normalization)
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
} ev_result;

static int compare_ev_desc(const void *a, const void *b)
{
    double diff = ((ev_result *)b)->score - ((ev_result *)a)->score;
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}

/*
 * Execute Eigenvector Centrality algorithm
 *
 * Uses power iteration to find the principal eigenvector of the adjacency matrix.
 * The centrality score for each node is proportional to the sum of centrality
 * scores of its neighbors.
 */
graph_algo_result* execute_eigenvector_centrality(sqlite3 *db, int iterations)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based Eigenvector Centrality: iterations=%d", iterations);

    /* Load graph into CSR format */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Allocate eigenvector arrays */
    double *ev = malloc(n * sizeof(double));
    double *ev_new = malloc(n * sizeof(double));

    if (!ev || !ev_new) {
        free(ev);
        free(ev_new);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Initialize eigenvector: uniform values, normalized */
    double init_val = 1.0 / sqrt((double)n);
    for (int i = 0; i < n; i++) {
        ev[i] = init_val;
    }

    /* Power iteration */
    double convergence_threshold = 1e-10;
    int actual_iters = 0;

    for (int iter = 0; iter < iterations; iter++) {
        actual_iters++;

        /* Initialize new eigenvector to zero */
        for (int i = 0; i < n; i++) {
            ev_new[i] = 0.0;
        }

        /* Multiply by adjacency matrix (using incoming edges) */
        /* For each node, sum the eigenvector values of nodes pointing to it */
        for (int i = 0; i < n; i++) {
            int in_start = graph->in_row_ptr[i];
            int in_end = graph->in_row_ptr[i + 1];

            for (int j = in_start; j < in_end; j++) {
                int source = graph->in_col_idx[j];
                ev_new[i] += ev[source];
            }
        }

        /* L2 normalize the new eigenvector */
        double norm = 0.0;
        for (int i = 0; i < n; i++) {
            norm += ev_new[i] * ev_new[i];
        }
        norm = sqrt(norm);

        /* Handle zero norm (disconnected graph) */
        if (norm < 1e-15) {
            /* Fall back to uniform distribution */
            for (int i = 0; i < n; i++) {
                ev_new[i] = init_val;
            }
            norm = 1.0;
        } else {
            for (int i = 0; i < n; i++) {
                ev_new[i] /= norm;
            }
        }

        /* Check convergence */
        double max_diff = 0.0;
        for (int i = 0; i < n; i++) {
            double diff = fabs(ev_new[i] - ev[i]);
            if (diff > max_diff) max_diff = diff;
        }

        /* Swap arrays */
        double *tmp = ev;
        ev = ev_new;
        ev_new = tmp;

        if (max_diff < convergence_threshold) {
            CYPHER_DEBUG("Eigenvector Centrality converged at iteration %d (max_diff=%.2e)", iter, max_diff);
            break;
        }
    }

    CYPHER_DEBUG("Eigenvector Centrality completed in %d iterations", actual_iters);

    /* Build results array for sorting */
    ev_result *results = malloc(n * sizeof(ev_result));
    if (!results) {
        free(ev);
        free(ev_new);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        results[i].node_id = graph->node_ids[i];
        results[i].user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        results[i].score = ev[i];
    }

    qsort(results, n, sizeof(ev_result), compare_ev_desc);

    /* Build JSON output */
    size_t json_capacity = 64 + n * 64;
    char *json = malloc(json_capacity);
    if (!json) {
        free(results);
        free(ev);
        free(ev_new);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;

    for (int i = 0; i < n; i++) {
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
    free(ev);
    free(ev_new);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;
    return result;
}
