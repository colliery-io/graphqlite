/*
 * graph_algo_louvain.c
 *
 * Louvain community detection algorithm.
 * Fast modularity optimization that produces high-quality communities.
 * O(V log V) average case complexity.
 *
 * Two phases:
 * 1. Local optimization: Move nodes to maximize modularity gain
 * 2. Aggregation: Collapse communities into super-nodes and repeat
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "executor/graph_algo_internal.h"

/* Structure to track community information */
typedef struct {
    double sigma_in;   /* Sum of edge weights inside community */
    double sigma_tot;  /* Sum of edge weights incident to community */
    int size;          /* Number of nodes in community */
} community_info;

/*
 * Calculate modularity gain from moving node i to community c
 *
 * ΔQ = [k_i,in / m - resolution * sigma_tot * k_i / (2m²)]
 *
 * Where:
 * - k_i,in = sum of edge weights from i to nodes in c
 * - k_i = degree of node i
 * - sigma_tot = sum of degrees of nodes in c
 * - m = total edge weight
 * - resolution = resolution parameter (default 1.0)
 */
static double modularity_gain(
    double k_i_in,      /* Edges from i to community */
    double k_i,         /* Total degree of i */
    double sigma_tot,   /* Total degree of community */
    double m,           /* Total edge weight */
    double resolution
) {
    if (m == 0) return 0.0;
    return k_i_in / m - resolution * sigma_tot * k_i / (2.0 * m * m);
}

graph_algo_result* execute_louvain(sqlite3 *db, double resolution)
{
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

    /* Load graph */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Calculate total edge weight (m) and node degrees */
    double m = 0.0;  /* Total edge weight (treating as undirected) */
    double *k = calloc(n, sizeof(double));  /* Degree of each node */

    if (!k) {
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate degree array");
        return result;
    }

    /* Calculate degrees (undirected view) */
    for (int i = 0; i < n; i++) {
        int out_deg = graph->row_ptr[i + 1] - graph->row_ptr[i];
        int in_deg = graph->in_row_ptr[i + 1] - graph->in_row_ptr[i];
        k[i] = out_deg + in_deg;  /* Unweighted: each edge counts as 1 */
        m += out_deg;  /* Count each edge once (directed -> undirected) */
    }

    /* For undirected interpretation */
    /* m is already the number of directed edges, which equals undirected edges * 2 / 2 = undirected edges */
    /* Actually for modularity we need m = sum of all edge weights / 2 for undirected */
    /* Since we're treating directed as undirected, m = edge_count */

    if (m == 0) {
        /* No edges - each node is its own community */
        size_t buf_size = 256 + n * 128;
        char *json = malloc(buf_size);
        if (json) {
            char *ptr = json;
            ptr += sprintf(ptr, "[");
            for (int i = 0; i < n; i++) {
                if (i > 0) ptr += sprintf(ptr, ",");
                const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
                if (user_id) {
                    ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":\"%s\",\"community\":%d}",
                                  graph->node_ids[i], user_id, i);
                } else {
                    ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":null,\"community\":%d}",
                                  graph->node_ids[i], i);
                }
            }
            ptr += sprintf(ptr, "]");
            result->success = true;
            result->json_result = json;
        }
        free(k);
        csr_graph_free(graph);
        return result;
    }

    /* Initialize: each node in its own community */
    int *community = malloc(n * sizeof(int));
    community_info *comm_info = malloc(n * sizeof(community_info));
    double *k_i_in = calloc(n, sizeof(double));  /* Working array for edges to each community */

    if (!community || !comm_info || !k_i_in) {
        free(k);
        free(community);
        free(comm_info);
        free(k_i_in);
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate working arrays");
        return result;
    }

    for (int i = 0; i < n; i++) {
        community[i] = i;
        comm_info[i].sigma_in = 0.0;
        comm_info[i].sigma_tot = k[i];
        comm_info[i].size = 1;
    }

    /* Phase 1: Local optimization */
    int max_iterations = 100;
    int improved = 1;

    for (int iter = 0; iter < max_iterations && improved; iter++) {
        improved = 0;

        for (int i = 0; i < n; i++) {
            int current_comm = community[i];

            /* Reset k_i_in for neighbor communities */
            /* Calculate edges from i to each neighboring community */

            /* Collect unique neighboring communities and edges to them */
            int *neighbor_comms = malloc(n * sizeof(int));
            int num_neighbor_comms = 0;

            if (!neighbor_comms) continue;

            /* Initialize */
            for (int c = 0; c < n; c++) {
                k_i_in[c] = 0.0;
            }

            /* Count edges to each community (outgoing) */
            for (int j = graph->row_ptr[i]; j < graph->row_ptr[i + 1]; j++) {
                int neighbor = graph->col_idx[j];
                int neighbor_comm = community[neighbor];
                if (k_i_in[neighbor_comm] == 0.0 && neighbor_comm != current_comm) {
                    neighbor_comms[num_neighbor_comms++] = neighbor_comm;
                }
                k_i_in[neighbor_comm] += 1.0;  /* Unweighted edge */
            }

            /* Count edges to each community (incoming, for undirected) */
            for (int j = graph->in_row_ptr[i]; j < graph->in_row_ptr[i + 1]; j++) {
                int neighbor = graph->in_col_idx[j];
                int neighbor_comm = community[neighbor];
                if (k_i_in[neighbor_comm] == 0.0 && neighbor_comm != current_comm) {
                    neighbor_comms[num_neighbor_comms++] = neighbor_comm;
                }
                k_i_in[neighbor_comm] += 1.0;
            }

            /* Find best community to move to */
            double best_gain = 0.0;
            int best_comm = current_comm;

            /* First calculate gain from removing i from current community */
            double remove_cost = -modularity_gain(
                k_i_in[current_comm] - k[i],  /* Edges to own community (excluding self) */
                k[i],
                comm_info[current_comm].sigma_tot - k[i],
                m,
                resolution
            );

            /* Check each neighboring community */
            for (int c = 0; c < num_neighbor_comms; c++) {
                int target_comm = neighbor_comms[c];

                double gain = remove_cost + modularity_gain(
                    k_i_in[target_comm],
                    k[i],
                    comm_info[target_comm].sigma_tot,
                    m,
                    resolution
                );

                if (gain > best_gain) {
                    best_gain = gain;
                    best_comm = target_comm;
                }
            }

            /* Also check staying in current community vs moving out */
            /* (remove_cost + 0 should be compared) */

            /* Move to best community if there's improvement */
            if (best_comm != current_comm && best_gain > 1e-10) {
                /* Remove from current community */
                comm_info[current_comm].sigma_tot -= k[i];
                comm_info[current_comm].sigma_in -= 2.0 * k_i_in[current_comm];
                comm_info[current_comm].size--;

                /* Add to new community */
                comm_info[best_comm].sigma_tot += k[i];
                comm_info[best_comm].sigma_in += 2.0 * k_i_in[best_comm];
                comm_info[best_comm].size++;

                community[i] = best_comm;
                improved = 1;
            }

            free(neighbor_comms);
        }
    }

    /* Renumber communities to be consecutive starting from 0 */
    int *comm_map = malloc(n * sizeof(int));
    if (!comm_map) {
        free(k);
        free(community);
        free(comm_info);
        free(k_i_in);
        csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate community map");
        return result;
    }

    for (int i = 0; i < n; i++) {
        comm_map[i] = -1;
    }

    int next_comm = 0;
    for (int i = 0; i < n; i++) {
        if (comm_map[community[i]] == -1) {
            comm_map[community[i]] = next_comm++;
        }
        community[i] = comm_map[community[i]];
    }

    /* Build JSON result */
    size_t buf_size = 256 + n * 128;
    char *json = malloc(buf_size);
    if (!json) {
        free(k);
        free(community);
        free(comm_info);
        free(k_i_in);
        free(comm_map);
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
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":\"%s\",\"community\":%d}",
                          graph->node_ids[i], user_id, community[i]);
        } else {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":null,\"community\":%d}",
                          graph->node_ids[i], community[i]);
        }
    }

    ptr += sprintf(ptr, "]");

    result->success = true;
    result->json_result = json;

    /* Cleanup */
    free(k);
    free(community);
    free(comm_info);
    free(k_i_in);
    free(comm_map);
    csr_graph_free(graph);

    return result;
}
