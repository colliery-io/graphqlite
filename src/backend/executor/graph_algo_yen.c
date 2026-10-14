/*
 * Yen's k-shortest loopless paths.
 *
 * Returns up to k loopless source->target paths in order of non-decreasing
 * total cost as JSON:
 *
 *   {"paths":[{"path":["n1","n2"],"edges":[rowid,...],"distance":3.5}, ...],
 *    "found":true}
 *
 * Parallel edges are distinct routes: banning and route identity work on
 * col_idx slots, so two routes that share a node sequence but use different
 * parallel edges are both returned (the edges array names the specific
 * edges table rowids).  The search core is shared with execute_dijkstra, so
 * k = 1 matches dijkstra() exactly when no hop cap is set.  An optional hop
 * cap bounds every search of the algorithm (initial and per-spur), which
 * keeps large graphs cheap when only short routes are wanted.  Candidate
 * ordering is deterministic - shortest distance first, then discovery
 * order.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/graph_algorithms.h"
#include "executor/graph_algo_internal.h"
#include "executor/json_builder.h"

/* Upper bound on k, matching the topPageRank clamp. */
#define YEN_MAX_K 1000

/* Upper bound on max_hops, keeping the per-search buffers bounded. */
#define YEN_MAX_HOPS 1000

/* One path: internal node indices plus the col_idx slots of its edges. */
typedef struct {
    int *nodes;     /* len entries */
    int *slots;     /* len - 1 entries, NULL when len == 1 */
    int len;
    double distance;
} yen_path;

/* Growable list of paths, used for both accepted paths and candidates. */
typedef struct {
    yen_path **items;
    int count;
    int cap;
} yen_path_list;

static yen_path *yen_path_create(int len)
{
    if (len < 1) return NULL;

    yen_path *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->nodes = malloc((size_t)len * sizeof(int));
    p->slots = (len > 1) ? malloc((size_t)(len - 1) * sizeof(int)) : NULL;
    if (!p->nodes || (len > 1 && !p->slots)) {
        free(p->nodes);
        free(p->slots);
        free(p);
        return NULL;
    }
    p->len = len;
    return p;
}

static void yen_path_free(yen_path *p)
{
    if (!p) return;
    free(p->nodes);
    free(p->slots);
    free(p);
}

/* Route identity: same node sequence and same edge slots. */
static bool yen_path_equal(const yen_path *a, const yen_path *b)
{
    if (a->len != b->len) return false;
    if (memcmp(a->nodes, b->nodes, (size_t)a->len * sizeof(int)) != 0) return false;
    if (a->len > 1 &&
        memcmp(a->slots, b->slots, (size_t)(a->len - 1) * sizeof(int)) != 0) {
        return false;
    }
    return true;
}

/*
 * Build a path from the predecessor chains left by algo_dijkstra_search.
 * Only valid when the search reported a path; distance is the search's
 * dist[target_idx].
 */
static yen_path *yen_path_from_prev(int target_idx, const int *prev, const int *prev_edge,
                                    double distance)
{
    int len = 0;
    for (int cur = target_idx; cur >= 0; cur = prev[cur]) len++;

    yen_path *p = yen_path_create(len);
    if (!p) return NULL;
    p->distance = distance;

    int cur = target_idx;
    for (int i = len - 1; i >= 0; i--) {
        p->nodes[i] = cur;
        if (i > 0) {
            p->slots[i - 1] = prev_edge[cur];
            cur = prev[cur];
        }
    }
    return p;
}

/* Build a path from a caller-owned node/slot array pair. */
static yen_path *yen_path_from_arrays(int len, const int *nodes, const int *slots,
                                      double distance)
{
    yen_path *p = yen_path_create(len);
    if (!p) return NULL;

    memcpy(p->nodes, nodes, (size_t)len * sizeof(int));
    if (len > 1) memcpy(p->slots, slots, (size_t)(len - 1) * sizeof(int));
    p->distance = distance;
    return p;
}

static void yen_list_free(yen_path_list *list)
{
    for (int i = 0; i < list->count; i++) yen_path_free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool yen_list_reserve(yen_path_list *list, int needed)
{
    if (needed <= list->cap) return true;

    int new_cap = list->cap > 0 ? list->cap * 2 : 16;
    while (new_cap < needed) new_cap *= 2;

    yen_path **items = realloc(list->items, (size_t)new_cap * sizeof(*items));
    if (!items) return false;
    list->items = items;
    list->cap = new_cap;
    return true;
}

static bool yen_list_append(yen_path_list *list, yen_path *p)
{
    if (!yen_list_reserve(list, list->count + 1)) return false;
    list->items[list->count++] = p;
    return true;
}

/*
 * Insert keeping the list ordered by distance, ties in discovery order.
 * A new path is always the latest discovery, so equal distances go after
 * the existing entries.
 */
static bool yen_list_insert(yen_path_list *list, yen_path *p)
{
    if (!yen_list_reserve(list, list->count + 1)) return false;

    int i = list->count - 1;
    while (i >= 0 && list->items[i]->distance > p->distance) i--;
    i++;
    memmove(&list->items[i + 1], &list->items[i],
            (size_t)(list->count - i) * sizeof(*list->items));
    list->items[i] = p;
    list->count++;
    return true;
}

static yen_path *yen_list_pop_front(yen_path_list *list)
{
    if (list->count == 0) return NULL;

    yen_path *p = list->items[0];
    memmove(&list->items[0], &list->items[1],
            (size_t)(list->count - 1) * sizeof(*list->items));
    list->count--;
    return p;
}

static bool yen_list_contains(const yen_path_list *list, const yen_path *p)
{
    for (int i = 0; i < list->count; i++) {
        if (yen_path_equal(list->items[i], p)) return true;
    }
    return false;
}

/* All state one execute_yen() call needs beyond the CSR graph itself. */
typedef struct {
    const csr_graph *graph;
    double *weights;
    int n;
    int m;
    int target_idx;
    int max_hops;       /* 0 = unbounded */
    double *dist;
    int *prev;
    int *prev_edge;
    char *banned_nodes;
    char *banned_edges;
    int *hop_nodes;     /* bounded-search result, max_hops + 1 entries */
    int *hop_slots;     /* bounded-search result, max_hops entries */
    yen_path_list accepted;
    yen_path_list candidates;
} yen_ctx;

static void yen_ctx_free(yen_ctx *ctx)
{
    free(ctx->weights);
    free(ctx->dist);
    free(ctx->prev);
    free(ctx->prev_edge);
    free(ctx->banned_nodes);
    free(ctx->banned_edges);
    free(ctx->hop_nodes);
    free(ctx->hop_slots);
    yen_list_free(&ctx->accepted);
    yen_list_free(&ctx->candidates);
    memset(ctx, 0, sizeof(*ctx));
}

/* Allocate the weights, search scratch, ban arrays, and both path lists. */
static bool yen_ctx_init(yen_ctx *ctx, const csr_graph *graph, sqlite3 *db,
                         const char *weight_prop, int target_idx, int max_hops)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->graph = graph;
    ctx->n = graph->node_count;
    ctx->m = graph->edge_count;
    ctx->target_idx = target_idx;
    ctx->max_hops = max_hops;

    ctx->weights = csr_load_edge_weights(db, graph, weight_prop);
    ctx->dist = malloc((size_t)ctx->n * sizeof(double));
    ctx->prev = malloc((size_t)ctx->n * sizeof(int));
    ctx->prev_edge = malloc((size_t)ctx->n * sizeof(int));
    ctx->banned_nodes = calloc((size_t)ctx->n, sizeof(char));
    ctx->banned_edges = calloc(ctx->m > 0 ? (size_t)ctx->m : 1, sizeof(char));
    if (max_hops > 0) {
        ctx->hop_nodes = malloc((size_t)(max_hops + 1) * sizeof(int));
        ctx->hop_slots = malloc((size_t)max_hops * sizeof(int));
    }

    if (!ctx->weights || !ctx->dist || !ctx->prev || !ctx->prev_edge ||
        !ctx->banned_nodes || !ctx->banned_edges ||
        (max_hops > 0 && (!ctx->hop_nodes || !ctx->hop_slots))) {
        yen_ctx_free(ctx);
        return false;
    }
    return true;
}

/*
 * Queue one candidate built from an accepted path's root prefix and a spur
 * path.  Candidates that are already accepted or queued are dropped.
 * Returns false only on allocation failure.
 */
static bool yen_emit_candidate(yen_ctx *ctx, const yen_path *last, int i,
                               const yen_path *spur, double total_distance)
{
    yen_path *cand = yen_path_create(i + spur->len);
    if (!cand) return false;

    memcpy(cand->nodes, last->nodes, (size_t)(i + 1) * sizeof(int));
    memcpy(cand->nodes + i + 1, spur->nodes + 1, (size_t)(spur->len - 1) * sizeof(int));
    for (int j = 0; j < i; j++) cand->slots[j] = last->slots[j];
    for (int j = 0; j < spur->len - 1; j++) cand->slots[i + j] = spur->slots[j];
    cand->distance = total_distance;

    if (yen_list_contains(&ctx->accepted, cand) ||
        yen_list_contains(&ctx->candidates, cand)) {
        yen_path_free(cand);
        return true;
    }
    if (!yen_list_insert(&ctx->candidates, cand)) {
        yen_path_free(cand);
        return false;
    }
    return true;
}

/*
 * Run one search from from_idx to ctx->target_idx with the current bans
 * and the given hop budget (0 = unbounded).  Returns the path, NULL when
 * none exists, and sets *oom on allocation failure.
 */
static yen_path *yen_search(yen_ctx *ctx, int from_idx, int budget, bool *oom)
{
    if (ctx->max_hops > 0) {
        int len = 0;
        double distance = 0.0;
        int status = algo_dijkstra_search_bounded(ctx->graph, ctx->weights, from_idx,
                                                  ctx->target_idx,
                                                  ctx->banned_nodes, ctx->banned_edges,
                                                  budget, ctx->hop_nodes, ctx->hop_slots,
                                                  &len, &distance);
        if (status < 0) {
            *oom = true;
            return NULL;
        }
        if (status == 0) return NULL;
        return yen_path_from_arrays(len, ctx->hop_nodes, ctx->hop_slots, distance);
    }

    int status = algo_dijkstra_search(ctx->graph, ctx->weights, from_idx, ctx->target_idx,
                                      ctx->banned_nodes, ctx->banned_edges,
                                      ctx->dist, ctx->prev, ctx->prev_edge);
    if (status < 0) {
        *oom = true;
        return NULL;
    }
    if (status == 0) return NULL;
    return yen_path_from_prev(ctx->target_idx, ctx->prev, ctx->prev_edge,
                              ctx->dist[ctx->target_idx]);
}

/*
 * Execute Yen's k-shortest loopless paths algorithm.
 *
 * k is clamped to [1, YEN_MAX_K].  max_hops is an optional hop cap on every
 * returned route (0 = unbounded); it is clamped to [1, YEN_MAX_HOPS] and
 * ignored when it cannot exclude anything (>= node_count - 1).  With
 * weight_prop NULL every edge weighs 1.0; otherwise the named edge property
 * is the weight (missing edges weigh 1.0), matching execute_dijkstra.
 * Negative weights are not supported.
 */
graph_algo_result* execute_yen(sqlite3 *db, csr_graph *cached, const char *source_id,
                               const char *target_id, int k, const char *weight_prop,
                               int max_hops)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing Yen k-shortest: source=%s, target=%s, k=%d, weight=%s, "
                 "max_hops=%d, cached=%s",
                 source_id ? source_id : "NULL",
                 target_id ? target_id : "NULL",
                 k,
                 weight_prop ? weight_prop : "NULL",
                 max_hops,
                 cached ? "yes" : "no");

    if (!source_id || !target_id) {
        result->success = false;
        result->error_message = strdup("yen requires source and target node IDs");
        return result;
    }

    /* k is clamped to [1, YEN_MAX_K], like the other k-style parameters. */
    if (k < 1) k = 1;
    if (k > YEN_MAX_K) k = YEN_MAX_K;

    /* Negative hop caps mean unbounded; cap the value like k. */
    if (max_hops < 0) max_hops = 0;
    if (max_hops > YEN_MAX_HOPS) max_hops = YEN_MAX_HOPS;

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
        result->json_result = strdup("{\"paths\":[],\"found\":false}");
        return result;
    }

    int source_idx = find_node_by_user_id(graph, source_id);
    int target_idx = find_node_by_user_id(graph, target_id);

    if (source_idx < 0 || target_idx < 0) {
        if (should_free_graph) csr_graph_free(graph);
        result->success = true;
        result->json_result = strdup("{\"paths\":[],\"found\":false}");
        return result;
    }

    /* A simple path never exceeds node_count - 1 edges; a cap at least that
     * large excludes nothing, so the cheaper unbounded search applies. */
    if (max_hops >= graph->node_count - 1) max_hops = 0;

    yen_ctx ctx;
    if (!yen_ctx_init(&ctx, graph, db, weight_prop, target_idx, max_hops)) {
        yen_ctx_free(&ctx);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* First path: the plain shortest path (hop-bounded when max_hops is
     * set), identical to dijkstra() when unbounded. */
    bool oom = false;
    yen_path *first = yen_search(&ctx, source_idx, ctx.max_hops, &oom);
    if (first && !yen_list_append(&ctx.accepted, first)) {
        yen_path_free(first);
        oom = true;
    }

    /* Yen's deviations: for every spur position of the last accepted path,
     * search again with the root-path nodes banned and the edge at that
     * position of every accepted path sharing the root banned, then queue
     * whichever deviations are new. */
    while (!oom && ctx.accepted.count > 0 && ctx.accepted.count < k) {
        const yen_path *last = ctx.accepted.items[ctx.accepted.count - 1];
        double prefix_cost = 0.0;

        for (int i = 0; i < last->len - 1; i++) {
            memset(ctx.banned_nodes, 0, (size_t)ctx.n);
            memset(ctx.banned_edges, 0, ctx.m > 0 ? (size_t)ctx.m : 1);

            for (int j = 0; j < i; j++) ctx.banned_nodes[last->nodes[j]] = 1;

            for (int a = 0; a < ctx.accepted.count; a++) {
                const yen_path *p = ctx.accepted.items[a];
                if (p->len <= i + 1) continue;   /* no edge at position i */
                bool same_root = true;
                for (int j = 0; j < i; j++) {
                    if (p->slots[j] != last->slots[j]) {
                        same_root = false;
                        break;
                    }
                }
                if (same_root) ctx.banned_edges[p->slots[i]] = 1;
            }

            int budget = ctx.max_hops > 0 ? ctx.max_hops - i : 0;
            yen_path *spur = yen_search(&ctx, last->nodes[i], budget, &oom);
            if (oom) break;

            if (spur) {
                if (!yen_emit_candidate(&ctx, last, i, spur,
                                        prefix_cost + spur->distance)) {
                    yen_path_free(spur);
                    oom = true;
                    break;
                }
                yen_path_free(spur);
            }

            prefix_cost += ctx.weights[last->slots[i]];
        }

        if (oom) break;

        yen_path *next = yen_list_pop_front(&ctx.candidates);
        if (!next) break;   /* no further distinct paths */
        if (!yen_list_append(&ctx.accepted, next)) {
            yen_path_free(next);
            oom = true;
            break;
        }
    }

    if (oom) {
        yen_ctx_free(&ctx);
        if (should_free_graph) csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Build JSON:
     * {"paths":[{"path":[...],"edges":[...],"distance":...}, ...],
     *  "found":bool} */
    json_builder jb;
    jbuf_init(&jb, 128 + (size_t)ctx.accepted.count * 64);
    jbuf_append(&jb, "{\"paths\":[");
    for (int a = 0; a < ctx.accepted.count; a++) {
        const yen_path *p = ctx.accepted.items[a];
        if (a > 0) jbuf_append(&jb, ",");

        jbuf_append(&jb, "{\"path\":[");
        for (int i = 0; i < p->len; i++) {
            if (i > 0) jbuf_append(&jb, ",");
            const char *user_id = graph->user_ids ? graph->user_ids[p->nodes[i]] : NULL;
            if (user_id) {
                jbuf_add_json_string(&jb, user_id);
            } else {
                jbuf_appendf(&jb, "%d", graph->node_ids[p->nodes[i]]);
            }
        }

        jbuf_append(&jb, "],\"edges\":[");
        for (int i = 0; i < p->len - 1; i++) {
            if (i > 0) jbuf_append(&jb, ",");
            jbuf_appendf(&jb, "%d", graph->edge_ids[p->slots[i]]);
        }

        jbuf_appendf(&jb, "],\"distance\":%.6g}", p->distance);
    }
    jbuf_appendf(&jb, "],\"found\":%s}", ctx.accepted.count > 0 ? "true" : "false");

    char *json = jbuf_ok(&jb) ? jbuf_take(&jb) : NULL;
    jbuf_free(&jb);

    yen_ctx_free(&ctx);
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
