/*
 * graph_algo_louvain.c
 *
 * Louvain community detection algorithm.
 * Fast modularity optimization that produces high-quality communities.
 * Each local-move pass is O(E).
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

/*
 * GQLITE-T-0382: the local-move gain subtracted the node's own degree from
 * its edges into its community and counted self-loops as such edges, so
 * nodes left good communities (results below the singleton modularity),
 * and there was no aggregation phase. This is standard Louvain on the
 * undirected weighted graph where each directed edge has weight 1 (a
 * reciprocal pair weighs 2, a self-loop adds 2 to its node's degree).
 */

/* One level of the graph: undirected weighted adjacency in CSR form.
 * Self-loop weight is kept apart: it moves with the node and does not
 * change the gain of a move. */
typedef struct {
    int n;
    int *ptr;        /* n + 1 */
    int *nbr;
    double *w;
    double *selfw;   /* n */
    double *k;       /* weighted degree, self-loops counted twice */
} lv_level;

static void lv_level_free(lv_level *g)
{
    free(g->ptr); free(g->nbr); free(g->w); free(g->selfw); free(g->k);
    memset(g, 0, sizeof(*g));
}

/* Build a level from an edge list (u, v, w); u == v is a self-loop. */
static int lv_level_build(lv_level *g, int n, const int *eu, const int *ev, const double *ew, int ne)
{
    memset(g, 0, sizeof(*g));
    g->n = n;
    g->ptr = calloc((size_t)n + 1, sizeof(int));
    g->selfw = calloc(n > 0 ? n : 1, sizeof(double));
    g->k = calloc(n > 0 ? n : 1, sizeof(double));
    if (!g->ptr || !g->selfw || !g->k) { lv_level_free(g); return -1; }
    int half = 0;
    for (int e = 0; e < ne; e++) {
        if (eu[e] == ev[e]) {
            g->selfw[eu[e]] += ew[e];
            g->k[eu[e]] += 2.0 * ew[e];
        } else {
            g->ptr[eu[e] + 1]++;
            g->ptr[ev[e] + 1]++;
            g->k[eu[e]] += ew[e];
            g->k[ev[e]] += ew[e];
            half++;
        }
    }
    for (int i = 0; i < n; i++) g->ptr[i + 1] += g->ptr[i];
    g->nbr = malloc((size_t)(2 * half > 0 ? 2 * half : 1) * sizeof(int));
    g->w = malloc((size_t)(2 * half > 0 ? 2 * half : 1) * sizeof(double));
    int *fill = calloc(n > 0 ? n : 1, sizeof(int));
    if (!g->nbr || !g->w || !fill) { free(fill); lv_level_free(g); return -1; }
    for (int e = 0; e < ne; e++) {
        if (eu[e] == ev[e]) continue;
        int pu = g->ptr[eu[e]] + fill[eu[e]]++;
        g->nbr[pu] = ev[e]; g->w[pu] = ew[e];
        int pv = g->ptr[ev[e]] + fill[ev[e]]++;
        g->nbr[pv] = eu[e]; g->w[pv] = ew[e];
    }
    free(fill);
    return 0;
}

/* Local moves on one level. comm[i] is the community of node i (starts as
 * i). Returns 1 if any node moved, 0 if none, -1 on OOM. */
static int lv_local_moves(const lv_level *g, int *comm, double m, double resolution)
{
    int n = g->n;
    double *tot = calloc(n, sizeof(double));     /* Σ k over a community */
    double *w_to = calloc(n, sizeof(double));    /* edge weight from i to a community */
    int *touched = malloc(n * sizeof(int));
    if (!tot || !w_to || !touched) { free(tot); free(w_to); free(touched); return -1; }
    for (int i = 0; i < n; i++) tot[comm[i]] += g->k[i];

    int moved_any = 0;
    for (int pass = 0; pass < 100; pass++) {
        int moved = 0;
        for (int i = 0; i < n; i++) {
            int c = comm[i];
            int nt = 0;
            for (int j = g->ptr[i]; j < g->ptr[i + 1]; j++) {
                int d = comm[g->nbr[j]];
                if (w_to[d] == 0.0) touched[nt++] = d;
                w_to[d] += g->w[j];
            }
            /* Gain of i moving from c to d, i removed from c first:
             * (w_to[d] - w_to[c]) / m - res * k_i * (tot[d] - (tot[c] - k_i)) / (2 m^2) */
            double ki = g->k[i];
            double base_c = tot[c] - ki;
            int best = c;
            double best_gain = 0.0;
            for (int t = 0; t < nt; t++) {
                int d = touched[t];
                if (d == c) continue;
                double gain = (w_to[d] - w_to[c]) / m
                            - resolution * ki * (tot[d] - base_c) / (2.0 * m * m);
                if (gain > best_gain + 1e-12 || (gain > 1e-12 && gain == best_gain && d < best)) {
                    best_gain = gain;
                    best = d;
                }
            }
            for (int t = 0; t < nt; t++) w_to[touched[t]] = 0.0;
            if (best != c) {
                tot[c] -= ki;
                tot[best] += ki;
                comm[i] = best;
                moved = 1;
            }
        }
        if (!moved) break;
        moved_any = 1;
    }
    free(tot); free(w_to); free(touched);
    return moved_any;
}

graph_algo_result* execute_louvain(sqlite3 *db, csr_graph *cached, double resolution)
{
    graph_algo_result *result = malloc(sizeof(graph_algo_result));
    if (!result) return NULL;

    result->success = false;
    result->error_message = NULL;
    result->json_result = NULL;

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
    int ne = graph->edge_count;

    /* community[i]: community of original node i, through all levels */
    int *community = malloc((n > 0 ? n : 1) * sizeof(int));
    int *eu = malloc((ne > 0 ? ne : 1) * sizeof(int));
    int *ev = malloc((ne > 0 ? ne : 1) * sizeof(int));
    double *ew = malloc((ne > 0 ? ne : 1) * sizeof(double));
    if (!community || !eu || !ev || !ew) {
        free(community); free(eu); free(ev); free(ew);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate working arrays");
        return result;
    }
    for (int i = 0; i < n; i++) community[i] = i;
    int e = 0;
    for (int u = 0; u < n; u++) {
        for (int j = graph->row_ptr[u]; j < graph->row_ptr[u + 1]; j++) {
            eu[e] = u; ev[e] = graph->col_idx[j]; ew[e] = 1.0; e++;
        }
    }
    double m = (double)ne;

    int level_n = n;
    int level_ne = ne;
    bool oom = false;
    while (m > 0 && level_n > 1) {
        lv_level g;
        if (lv_level_build(&g, level_n, eu, ev, ew, level_ne) < 0) { oom = true; break; }
        int *comm = malloc(level_n * sizeof(int));
        if (!comm) { lv_level_free(&g); oom = true; break; }
        for (int i = 0; i < level_n; i++) comm[i] = i;
        int moved = lv_local_moves(&g, comm, m, resolution);
        lv_level_free(&g);
        if (moved < 0) { free(comm); oom = true; break; }
        if (moved == 0) { free(comm); break; }

        /* Renumber this level's communities and aggregate */
        int *renum = malloc(level_n * sizeof(int));
        if (!renum) { free(comm); oom = true; break; }
        for (int i = 0; i < level_n; i++) renum[i] = -1;
        int next = 0;
        for (int i = 0; i < level_n; i++) {
            if (renum[comm[i]] < 0) renum[comm[i]] = next++;
        }
        for (int i = 0; i < n; i++) community[i] = renum[comm[community[i]]];
        for (int x = 0; x < level_ne; x++) {
            eu[x] = renum[comm[eu[x]]];
            ev[x] = renum[comm[ev[x]]];
        }
        free(renum);
        free(comm);
        level_n = next;
    }
    free(eu); free(ev); free(ew);
    if (oom) {
        free(community);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate working arrays");
        return result;
    }

    /* Renumber communities to be consecutive starting from 0 */
    int *comm_map = malloc(n * sizeof(int));
    if (!comm_map) {
        free(community);
        if (should_free_graph) csr_graph_free(graph);
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
        free(community);
        free(comm_map);
        if (should_free_graph) csr_graph_free(graph);
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
    free(community);
    free(comm_map);
    if (should_free_graph) csr_graph_free(graph);

    return result;
}
