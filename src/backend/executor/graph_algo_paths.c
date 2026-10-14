/*
 * Path Algorithms
 *
 * Dijkstra's shortest path and related algorithms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"
#include "executor/json_builder.h"

/*
 * Shared Dijkstra search over the CSR adjacency from source_idx to
 * target_idx.
 *
 * weights, when non-NULL, holds one weight per col_idx slot; NULL weighs
 * every edge 1.0.  banned_nodes / banned_edges, when non-NULL, are boolean
 * arrays sized node_count / edge_count: a banned node is never settled and
 * a banned edge slot is never relaxed.  Callers must not ban the source or
 * the target.
 *
 * The caller owns dist, prev and prev_edge, each node_count entries.
 * prev_edge records the col_idx slot used to reach each node and may be
 * NULL when the caller does not need slot tracking.
 *
 * Returns 1 when a path exists (dist[target_idx] holds its cost), 0 when
 * it does not, and -1 on allocation failure.
 */
int algo_dijkstra_search(const csr_graph *graph, const double *weights,
                         int source_idx, int target_idx,
                         const char *banned_nodes, const char *banned_edges,
                         double *dist, int *prev, int *prev_edge)
{
    int n = graph->node_count;

    char *visited = calloc(n, sizeof(char));
    if (!visited) return -1;

    for (int i = 0; i < n; i++) {
        dist[i] = 1e308;
        prev[i] = -1;
        if (prev_edge) prev_edge[i] = -1;
    }
    dist[source_idx] = 0.0;

    min_heap *heap = heap_create(n);
    if (!heap) {
        free(visited);
        return -1;
    }

    heap_push(heap, source_idx, 0.0);

    while (heap->size > 0) {
        heap_entry cur = heap_pop(heap);
        int u = cur.node;

        if (visited[u]) continue;
        visited[u] = 1;

        if (banned_nodes && banned_nodes[u]) continue;
        if (u == target_idx) break;

        for (int j = graph->row_ptr[u]; j < graph->row_ptr[u + 1]; j++) {
            if (banned_edges && banned_edges[j]) continue;

            int v = graph->col_idx[j];
            double w = weights ? weights[j] : 1.0;
            double alt = dist[u] + w;

            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                if (prev_edge) prev_edge[v] = j;
                heap_push(heap, v, alt);
            }
        }
    }

    heap_free(heap);
    free(visited);

    return (source_idx == target_idx || prev[target_idx] >= 0) ? 1 : 0;
}

/* One relaxation write of the hop-bounded search, kept in round order for
 * path reconstruction. */
typedef struct {
    int node;
    int parent;
    int slot;
    int round;
} hop_record;

static bool hop_record_push(hop_record **records, int *count, int *capacity,
                            int node, int parent, int slot, int round)
{
    if (*count >= *capacity) {
        int grown_capacity = *capacity > 0 ? *capacity * 2 : 256;
        hop_record *grown = realloc(*records, (size_t)grown_capacity * sizeof(*grown));
        if (!grown) return false;
        *records = grown;
        *capacity = grown_capacity;
    }

    (*records)[*count].node = node;
    (*records)[*count].parent = parent;
    (*records)[*count].slot = slot;
    (*records)[*count].round = round;
    (*count)++;
    return true;
}

/*
 * Hop-bounded Dijkstra search from source_idx to target_idx.
 *
 * Computes the minimum-cost path using at most max_hops edges.  The state
 * is (node, hops): round d relaxes the outgoing edges of the nodes whose
 * distance strictly improved in round d-1 on top of the values carried
 * from round d-1, so a node can hold different costs at different hop
 * counts.  A plain Dijkstra pruned at the hop cap is not exact: a node
 * settled through a cheaper but longer route hides a within-budget route
 * through a costlier but shorter one (cost 5 in 3 hops vs cost 6 in 1 hop
 * to v, v->t costs 2, cap 3: the exact answer is 6+2=8, the pruned
 * Dijkstra settles v at 5, cannot extend it, and misses 8 entirely).
 *
 * Bans work as in algo_dijkstra_search (callers must not ban the source or
 * the target).  The returned path is loopless; a reconstructed walk that
 * revisits a node is spliced at the repeat (the removed segment costs 0,
 * otherwise it would contradict minimality).
 *
 * The caller owns path_nodes (max_hops + 1 entries) and path_slots
 * (max_hops entries).  Returns 1 when a path exists (path_len and
 * path_distance filled), 0 when none fits the budget, and -1 on
 * allocation failure.
 */
int algo_dijkstra_search_bounded(const csr_graph *graph, const double *weights,
                                 int source_idx, int target_idx,
                                 const char *banned_nodes, const char *banned_edges,
                                 int max_hops,
                                 int *path_nodes, int *path_slots, int *path_len,
                                 double *path_distance)
{
    int n = graph->node_count;

    if (source_idx == target_idx) {
        path_nodes[0] = source_idx;
        *path_len = 1;
        *path_distance = 0.0;
        return 1;
    }
    if (max_hops < 1) return 0;

    double *cur = malloc((size_t)n * sizeof(double));
    double *next = malloc((size_t)n * sizeof(double));
    char *in_next = calloc((size_t)n, sizeof(char));
    int *frontier = malloc((size_t)n * sizeof(int));
    int *next_frontier = malloc((size_t)n * sizeof(int));
    hop_record *records = NULL;
    int record_count = 0;
    int record_capacity = 0;

    if (!cur || !next || !in_next || !frontier || !next_frontier) {
        free(cur);
        free(next);
        free(in_next);
        free(frontier);
        free(next_frontier);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        cur[i] = 1e308;
        next[i] = 1e308;
    }
    cur[source_idx] = 0.0;

    int frontier_count = 1;
    frontier[0] = source_idx;
    double best_target = 1e308;
    bool oom = false;

    for (int round = 1; round <= max_hops && frontier_count > 0; round++) {
        /* Carry: anything reachable with <= round-1 hops is legal at round. */
        memcpy(next, cur, (size_t)n * sizeof(double));
        int next_count = 0;

        for (int f = 0; f < frontier_count; f++) {
            int u = frontier[f];
            if (cur[u] >= best_target) continue;

            for (int j = graph->row_ptr[u]; j < graph->row_ptr[u + 1]; j++) {
                if (banned_edges && banned_edges[j]) continue;

                int v = graph->col_idx[j];
                if (banned_nodes && banned_nodes[v]) continue;

                double alt = cur[u] + (weights ? weights[j] : 1.0);
                if (alt >= best_target) continue;

                if (alt < next[v]) {
                    next[v] = alt;
                    if (!hop_record_push(&records, &record_count, &record_capacity,
                                         v, u, j, round)) {
                        oom = true;
                        break;
                    }
                    if (v != target_idx && !in_next[v]) {
                        in_next[v] = 1;
                        next_frontier[next_count++] = v;
                    }
                }
            }
            if (oom) break;
        }

        if (oom) break;

        double *tmp_dist = cur;
        cur = next;
        next = tmp_dist;
        int *tmp_frontier = frontier;
        frontier = next_frontier;
        next_frontier = tmp_frontier;
        frontier_count = next_count;

        if (cur[target_idx] < best_target) best_target = cur[target_idx];

        for (int f = 0; f < frontier_count; f++) in_next[frontier[f]] = 0;
    }

    if (oom) {
        free(cur);
        free(next);
        free(in_next);
        free(frontier);
        free(next_frontier);
        free(records);
        return -1;
    }

    int status = cur[target_idx] < 1e308 ? 1 : 0;

    if (status > 0) {
        /* Walk back: each node's latest record at or below the remaining
         * round limit names its parent; the limit shrinks to that record's
         * round minus one, so the chain uses the recorded hop counts. */
        int count = 0;
        int walk = target_idx;
        int limit = max_hops;

        while (walk != source_idx) {
            int found_record = -1;
            for (int r = record_count - 1; r >= 0; r--) {
                if (records[r].node == walk && records[r].round <= limit) {
                    found_record = r;
                    break;
                }
            }
            if (found_record < 0 || count >= max_hops) {
                CYPHER_DEBUG("bounded search: incomplete predecessor chain for node %d", walk);
                status = 0;
                break;
            }

            path_nodes[count] = walk;
            path_slots[count] = records[found_record].slot;
            count++;
            limit = records[found_record].round - 1;
            walk = records[found_record].parent;
        }

        if (status > 0) {
            path_nodes[count] = source_idx;
            int total_nodes = count + 1;

            /* The walk is target-first; flip it. */
            for (int i = 0; i < total_nodes / 2; i++) {
                int tmp = path_nodes[i];
                path_nodes[i] = path_nodes[total_nodes - 1 - i];
                path_nodes[total_nodes - 1 - i] = tmp;
            }
            for (int i = 0; i < count / 2; i++) {
                int tmp = path_slots[i];
                path_slots[i] = path_slots[count - 1 - i];
                path_slots[count - 1 - i] = tmp;
            }

            /* Splice repeated nodes (only possible through zero-cost
             * segments, otherwise minimality is contradicted). */
            int out_nodes = 1;
            int out_slots = 0;
            for (int i = 1; i <= count; i++) {
                int node = path_nodes[i];
                int repeat = -1;
                for (int k = 0; k < out_nodes; k++) {
                    if (path_nodes[k] == node) {
                        repeat = k;
                        break;
                    }
                }
                if (repeat >= 0) {
                    out_nodes = repeat + 1;
                    out_slots = repeat;
                    continue;
                }
                path_nodes[out_nodes] = node;
                path_slots[out_slots] = path_slots[i - 1];
                out_nodes++;
                out_slots++;
            }

            if (out_slots > max_hops) {
                CYPHER_DEBUG("bounded search: path exceeds the hop budget after splicing");
                status = 0;
            } else {
                double distance = 0.0;
                for (int i = 0; i < out_slots; i++) {
                    distance += weights ? weights[path_slots[i]] : 1.0;
                }
                *path_len = out_nodes;
                *path_distance = distance;
            }
        }
    }

    free(cur);
    free(next);
    free(in_next);
    free(frontier);
    free(next_frontier);
    free(records);
    return status;
}

/*
 * Execute Dijkstra's shortest path algorithm
 *
 * Returns the shortest path from source to target as JSON:
 * {"path": ["node1", "node2", ...], "distance": 3.5, "found": true}
 *
 * If weight_prop is NULL, uses unweighted edges (distance = hop count)
 */
graph_algo_result* execute_dijkstra(sqlite3 *db, csr_graph *cached, const char *source_id, const char *target_id, const char *weight_prop)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing Dijkstra: source=%s, target=%s, weight=%s, cached=%s",
                 source_id ? source_id : "NULL",
                 target_id ? target_id : "NULL",
                 weight_prop ? weight_prop : "NULL",
                 cached ? "yes" : "no");

    if (!source_id || !target_id) {
        result->success = false;
        result->error_message = strdup("shortestPath requires source and target node IDs");
        return result;
    }

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
        result->json_result = strdup("{\"path\":[],\"distance\":null,\"found\":false}");
        return result;
    }

    int n = graph->node_count;

    /* Find source and target nodes */
    int source_idx = find_node_by_user_id(graph, source_id);
    int target_idx = find_node_by_user_id(graph, target_id);

    if (source_idx < 0 || target_idx < 0) {
        if (should_free_graph) csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("{\"path\":[],\"distance\":null,\"found\":false}");
        return result;
    }

    /* Load edge weights if specified */
    double *weights = NULL;
    if (weight_prop) {
        weights = csr_load_edge_weights(db, graph, weight_prop);
    }

    /* Dijkstra's algorithm (shared search core, no bans) */
    double *dist = malloc(n * sizeof(double));
    int *prev = malloc(n * sizeof(int));

    if (!dist || !prev) {
        free(dist);
        free(prev);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    if (algo_dijkstra_search(graph, weights, source_idx, target_idx,
                             NULL, NULL, dist, prev, NULL) < 0) {
        free(dist);
        free(prev);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Check if path was found */
    if (prev[target_idx] < 0 && source_idx != target_idx) {
        free(dist);
        free(prev);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("{\"path\":[],\"distance\":null,\"found\":false}");
        return result;
    }

    /* Reconstruct path */
    int path_len = 0;
    int *path = malloc(n * sizeof(int));
    if (!path) {
        free(dist);
        free(prev);
        free(weights);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    int cur = target_idx;
    while (cur >= 0) {
        path[path_len++] = cur;
        cur = prev[cur];
    }

    /* Reverse path */
    for (int i = 0; i < path_len / 2; i++) {
        int tmp = path[i];
        path[i] = path[path_len - 1 - i];
        path[path_len - 1 - i] = tmp;
    }

    /* Build JSON output */
    json_builder jb;
    jbuf_init(&jb, 128 + (size_t)path_len * 64);
    jbuf_append(&jb, "{\"path\":[");
    for (int i = 0; i < path_len; i++) {
        if (i > 0) jbuf_append(&jb, ",");
        const char *user_id = graph->user_ids ? graph->user_ids[path[i]] : NULL;
        if (user_id) {
            jbuf_add_json_string(&jb, user_id);
        } else {
            jbuf_appendf(&jb, "%d", graph->node_ids[path[i]]);
        }
    }
    jbuf_appendf(&jb, "],\"distance\":%.6g,\"found\":true}", dist[target_idx]);

    char *json = jbuf_ok(&jb) ? jbuf_take(&jb) : NULL;
    jbuf_free(&jb);

    free(dist);
    free(prev);
    free(path);
    free(weights);
    if (should_free_graph) csr_graph_free(graph);

    if (!json) {
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    result->success = true;
    result->json_result = json;
    return result;
}
