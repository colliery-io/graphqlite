/*
 * graph_algo_betweenness.c
 *
 * Betweenness Centrality using Brandes' algorithm.
 * Measures how often a node lies on shortest paths between other nodes.
 * O(VE) complexity for unweighted graphs.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "executor/graph_algo_internal.h"

/*
 * =============================================================================
 * Brandes' Algorithm for Betweenness Centrality
 * =============================================================================
 *
 * For each source node s:
 * 1. BFS to find shortest path counts (sigma) and distances (d)
 * 2. Track predecessors on shortest paths
 * 3. Backtrack to accumulate dependencies (delta)
 * 4. Add delta to betweenness scores
 */

/* Dynamic array for predecessors */
typedef struct {
    int *data;
    int count;
    int capacity;
} pred_list;

static void pred_list_init(pred_list *p) {
    p->data = NULL;
    p->count = 0;
    p->capacity = 0;
}

static void pred_list_add(pred_list *p, int val) {
    if (p->count >= p->capacity) {
        int new_cap = p->capacity == 0 ? 4 : p->capacity * 2;
        int *new_data = realloc(p->data, new_cap * sizeof(int));
        if (!new_data) return;
        p->data = new_data;
        p->capacity = new_cap;
    }
    p->data[p->count++] = val;
}

static void pred_list_clear(pred_list *p) {
    p->count = 0;
}

static void pred_list_free(pred_list *p) {
    free(p->data);
    p->data = NULL;
    p->count = 0;
    p->capacity = 0;
}

graph_algo_result* execute_betweenness_centrality(sqlite3 *db)
{
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

    /* Load graph - NULL means no nodes (empty graph) */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        /* Empty graph - no nodes exist */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Allocate betweenness scores (initialized to 0) */
    double *betweenness = calloc(n, sizeof(double));
    if (!betweenness) {
        result->error_message = strdup("Failed to allocate betweenness array");
        csr_graph_free(graph);
        return result;
    }

    /* Allocate working arrays */
    int *sigma = malloc(n * sizeof(int));       /* Number of shortest paths */
    int *d = malloc(n * sizeof(int));           /* Distance from source */
    double *delta = malloc(n * sizeof(double)); /* Dependency */
    pred_list *P = malloc(n * sizeof(pred_list)); /* Predecessors */
    int *queue = malloc(n * sizeof(int));       /* BFS queue */
    int *stack = malloc(n * sizeof(int));       /* Stack for backtracking */

    if (!sigma || !d || !delta || !P || !queue || !stack) {
        free(betweenness);
        free(sigma);
        free(d);
        free(delta);
        free(P);
        free(queue);
        free(stack);
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate working arrays");
        return result;
    }

    /* Initialize predecessor lists */
    for (int i = 0; i < n; i++) {
        pred_list_init(&P[i]);
    }

    /* Run Brandes' algorithm from each source node */
    for (int s = 0; s < n; s++) {
        /* Initialize for this source */
        for (int i = 0; i < n; i++) {
            sigma[i] = 0;
            d[i] = -1;
            delta[i] = 0.0;
            pred_list_clear(&P[i]);
        }

        sigma[s] = 1;
        d[s] = 0;

        int queue_front = 0, queue_back = 0;
        int stack_top = 0;

        queue[queue_back++] = s;

        /* BFS phase - find shortest paths */
        while (queue_front < queue_back) {
            int v = queue[queue_front++];
            stack[stack_top++] = v;

            /* Iterate over neighbors of v */
            for (int j = graph->row_ptr[v]; j < graph->row_ptr[v + 1]; j++) {
                int w = graph->col_idx[j];

                /* First visit to w? */
                if (d[w] < 0) {
                    d[w] = d[v] + 1;
                    queue[queue_back++] = w;
                }

                /* Shortest path to w via v? */
                if (d[w] == d[v] + 1) {
                    sigma[w] += sigma[v];
                    pred_list_add(&P[w], v);
                }
            }
        }

        /* Backtrack phase - accumulate dependencies */
        while (stack_top > 0) {
            int w = stack[--stack_top];

            for (int i = 0; i < P[w].count; i++) {
                int v = P[w].data[i];
                double contribution = ((double)sigma[v] / (double)sigma[w]) * (1.0 + delta[w]);
                delta[v] += contribution;
            }

            /* Add to betweenness (skip source node) */
            if (w != s) {
                betweenness[w] += delta[w];
            }
        }
    }

    /* For undirected interpretation, divide by 2 */
    /* We treat the graph as directed, so no division needed */

    /* Normalize by (n-1)(n-2) for comparison across graphs of different sizes */
    /* This is optional - we'll return raw scores for now */

    /* Free predecessor lists */
    for (int i = 0; i < n; i++) {
        pred_list_free(&P[i]);
    }

    /* Build JSON result */
    size_t buf_size = 256 + n * 128;
    char *json = malloc(buf_size);
    if (!json) {
        free(betweenness);
        free(sigma);
        free(d);
        free(delta);
        free(P);
        free(queue);
        free(stack);
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate result buffer");
        return result;
    }

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < n; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");

        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        if (user_id) {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":\"%s\",\"score\":%.6f}",
                          graph->node_ids[i], user_id, betweenness[i]);
        } else {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":null,\"score\":%.6f}",
                          graph->node_ids[i], betweenness[i]);
        }
    }

    ptr += sprintf(ptr, "]");

    result->success = true;
    result->json_result = json;

    /* Cleanup */
    free(betweenness);
    free(sigma);
    free(d);
    free(delta);
    free(P);
    free(queue);
    free(stack);
    csr_graph_free(graph);

    return result;
}
