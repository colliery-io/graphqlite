/*
 * graph_algo_components.c
 *
 * Connected Components algorithms:
 * - WCC: Weakly Connected Components (Union-Find, treats edges as undirected)
 * - SCC: Strongly Connected Components (Tarjan's algorithm, respects edge direction)
 */

#include <stddef.h>
#include "executor/graph_algo_internal.h"
#include <stdio.h>

/*
 * =============================================================================
 * Union-Find (Disjoint Set) for WCC
 * =============================================================================
 */

typedef struct {
    int *parent;
    int *rank;
    int size;
} union_find;

static union_find* uf_create(int size)
{
    union_find *uf = malloc(sizeof(union_find));
    if (!uf) return NULL;

    uf->parent = malloc(size * sizeof(int));
    uf->rank = malloc(size * sizeof(int));
    uf->size = size;

    if (!uf->parent || !uf->rank) {
        free(uf->parent);
        free(uf->rank);
        free(uf);
        return NULL;
    }

    /* Initialize: each node is its own parent with rank 0 */
    for (int i = 0; i < size; i++) {
        uf->parent[i] = i;
        uf->rank[i] = 0;
    }

    return uf;
}

static void uf_free(union_find *uf)
{
    if (uf) {
        free(uf->parent);
        free(uf->rank);
        free(uf);
    }
}

/* Find with path compression */
static int uf_find(union_find *uf, int x)
{
    if (uf->parent[x] != x) {
        uf->parent[x] = uf_find(uf, uf->parent[x]);
    }
    return uf->parent[x];
}

/* Union by rank */
static void uf_union(union_find *uf, int x, int y)
{
    int rx = uf_find(uf, x);
    int ry = uf_find(uf, y);

    if (rx == ry) return;

    if (uf->rank[rx] < uf->rank[ry]) {
        uf->parent[rx] = ry;
    } else if (uf->rank[rx] > uf->rank[ry]) {
        uf->parent[ry] = rx;
    } else {
        uf->parent[ry] = rx;
        uf->rank[rx]++;
    }
}

/*
 * =============================================================================
 * Weakly Connected Components (WCC)
 * =============================================================================
 *
 * Treats directed graph as undirected and finds connected components.
 * Uses Union-Find for O(V + E * α(V)) complexity where α is inverse Ackermann.
 */
graph_algo_result* execute_wcc(sqlite3 *db, csr_graph *cached)
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
        /* Empty graph - no nodes exist */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    /* Create Union-Find structure */
    union_find *uf = uf_create(graph->node_count);
    if (!uf) {
        result->error_message = strdup("Failed to allocate Union-Find structure");
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    /* Process all edges (treating as undirected) */
    for (int u = 0; u < graph->node_count; u++) {
        for (int j = graph->row_ptr[u]; j < graph->row_ptr[u + 1]; j++) {
            int v = graph->col_idx[j];
            uf_union(uf, u, v);
        }
    }

    /* Normalize component IDs to be contiguous starting from 0 */
    int *component_map = calloc(graph->node_count, sizeof(int));
    int *component = malloc(graph->node_count * sizeof(int));
    int next_component = 0;

    if (!component_map || !component) {
        free(component_map);
        free(component);
        uf_free(uf);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate component arrays");
        return result;
    }

    /* Initialize component_map to -1 */
    for (int i = 0; i < graph->node_count; i++) {
        component_map[i] = -1;
    }

    /* Assign contiguous component IDs */
    for (int i = 0; i < graph->node_count; i++) {
        int root = uf_find(uf, i);
        if (component_map[root] == -1) {
            component_map[root] = next_component++;
        }
        component[i] = component_map[root];
    }

    /* Build JSON result */
    size_t buf_size = 256 + graph->node_count * 128;
    char *json = malloc(buf_size);
    if (!json) {
        free(component_map);
        free(component);
        uf_free(uf);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate result buffer");
        return result;
    }

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < graph->node_count; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");

        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        if (user_id) {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":\"%s\",\"component\":%d}",
                          graph->node_ids[i], user_id, component[i]);
        } else {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":null,\"component\":%d}",
                          graph->node_ids[i], component[i]);
        }
    }

    ptr += sprintf(ptr, "]");

    result->success = true;
    result->json_result = json;

    free(component_map);
    free(component);
    uf_free(uf);
    if (should_free_graph) csr_graph_free(graph);

    return result;
}

/*
 * =============================================================================
 * Strongly Connected Components (SCC) - Tarjan's Algorithm
 * =============================================================================
 *
 * Finds maximal subgraphs where every node is reachable from every other node
 * following edge directions. O(V + E) complexity.
 */

/* Tarjan state for iterative implementation */
typedef struct {
    int *index;       /* Discovery index for each node */
    int *lowlink;     /* Lowest index reachable */
    int *on_stack;    /* Whether node is on stack */
    int *stack;       /* DFS stack */
    int stack_top;
    int current_index;
    int *component;   /* Component assignment */
    int component_count;
} tarjan_state;

static tarjan_state* tarjan_create(int n)
{
    tarjan_state *t = malloc(sizeof(tarjan_state));
    if (!t) return NULL;

    t->index = malloc(n * sizeof(int));
    t->lowlink = malloc(n * sizeof(int));
    t->on_stack = calloc(n, sizeof(int));
    t->stack = malloc(n * sizeof(int));
    t->component = malloc(n * sizeof(int));

    if (!t->index || !t->lowlink || !t->on_stack || !t->stack || !t->component) {
        free(t->index);
        free(t->lowlink);
        free(t->on_stack);
        free(t->stack);
        free(t->component);
        free(t);
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        t->index[i] = -1;  /* -1 means undefined */
        t->component[i] = -1;
    }

    t->stack_top = 0;
    t->current_index = 0;
    t->component_count = 0;

    return t;
}

static void tarjan_free(tarjan_state *t)
{
    if (t) {
        free(t->index);
        free(t->lowlink);
        free(t->on_stack);
        free(t->stack);
        free(t->component);
        free(t);
    }
}

/* Iterative Tarjan's algorithm using explicit call stack */
typedef struct {
    int node;
    int edge_idx;
    int phase;  /* 0 = entering, 1 = returning from neighbor */
    int saved_neighbor;
} call_frame;

static void tarjan_iterative(csr_graph *graph, tarjan_state *t, int start)
{
    int n = graph->node_count;

    /* Allocate call stack */
    call_frame *call_stack = malloc(n * sizeof(call_frame));
    if (!call_stack) return;

    int call_top = 0;

    /* Push initial call */
    call_stack[call_top].node = start;
    call_stack[call_top].edge_idx = graph->row_ptr[start];
    call_stack[call_top].phase = 0;
    call_top++;

    while (call_top > 0) {
        call_frame *frame = &call_stack[call_top - 1];
        int v = frame->node;

        if (frame->phase == 0) {
            /* First visit to this node */
            t->index[v] = t->current_index;
            t->lowlink[v] = t->current_index;
            t->current_index++;
            t->stack[t->stack_top++] = v;
            t->on_stack[v] = 1;
            frame->phase = 1;
        }

        /* Process outgoing edges */
        int found_unvisited = 0;
        while (frame->edge_idx < graph->row_ptr[v + 1]) {
            int w = graph->col_idx[frame->edge_idx];
            frame->edge_idx++;

            if (t->index[w] == -1) {
                /* w not yet visited - recurse */
                frame->saved_neighbor = w;

                /* Push new frame for w */
                call_stack[call_top].node = w;
                call_stack[call_top].edge_idx = graph->row_ptr[w];
                call_stack[call_top].phase = 0;
                call_top++;
                found_unvisited = 1;
                break;
            } else if (t->on_stack[w]) {
                /* w is on stack - back edge */
                if (t->lowlink[v] > t->index[w]) {
                    t->lowlink[v] = t->index[w];
                }
            }
        }

        if (found_unvisited) continue;

        /* All edges processed - check if we returned from a recursive call */
        if (call_top >= 2) {
            call_frame *parent = &call_stack[call_top - 2];
            int parent_node = parent->node;
            if (parent->saved_neighbor == v) {
                if (t->lowlink[parent_node] > t->lowlink[v]) {
                    t->lowlink[parent_node] = t->lowlink[v];
                }
            }
        }

        /* Check if v is a root of SCC */
        if (t->lowlink[v] == t->index[v]) {
            /* Pop nodes from stack until we get v */
            int w;
            do {
                w = t->stack[--t->stack_top];
                t->on_stack[w] = 0;
                t->component[w] = t->component_count;
            } while (w != v);
            t->component_count++;
        }

        call_top--;
    }

    free(call_stack);
}

graph_algo_result* execute_scc(sqlite3 *db, csr_graph *cached)
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
        /* Empty graph - no nodes exist */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    /* Create Tarjan state */
    tarjan_state *t = tarjan_create(graph->node_count);
    if (!t) {
        result->error_message = strdup("Failed to allocate Tarjan state");
        if (should_free_graph) csr_graph_free(graph);
        return result;
    }

    /* Run Tarjan's algorithm from each unvisited node */
    for (int i = 0; i < graph->node_count; i++) {
        if (t->index[i] == -1) {
            tarjan_iterative(graph, t, i);
        }
    }

    /* Build JSON result */
    size_t buf_size = 256 + graph->node_count * 128;
    char *json = malloc(buf_size);
    if (!json) {
        tarjan_free(t);
        if (should_free_graph) csr_graph_free(graph);
        result->error_message = strdup("Failed to allocate result buffer");
        return result;
    }

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (int i = 0; i < graph->node_count; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");

        const char *user_id = graph->user_ids ? graph->user_ids[i] : NULL;
        if (user_id) {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":\"%s\",\"component\":%d}",
                          graph->node_ids[i], user_id, t->component[i]);
        } else {
            ptr += sprintf(ptr, "{\"node_id\":%d,\"user_id\":null,\"component\":%d}",
                          graph->node_ids[i], t->component[i]);
        }
    }

    ptr += sprintf(ptr, "]");

    result->success = true;
    result->json_result = json;

    tarjan_free(t);
    if (should_free_graph) csr_graph_free(graph);

    return result;
}
