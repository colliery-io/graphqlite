#ifndef GRAPH_ALGO_INTERNAL_H
#define GRAPH_ALGO_INTERNAL_H

/*
 * Internal header for graph algorithm implementations
 *
 * Shared utilities, data structures, and helper functions
 * used across algorithm modules.
 */

#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"
#include <stdlib.h>
#include <string.h>

/* Hash table size for node ID lookups - should be prime and larger than expected node count */
#define HASH_TABLE_SIZE 1000003

/* Simple hash function for integer keys */
static inline int hash_int(int key, int size)
{
    unsigned int h = (unsigned int)key;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return (int)(h % (unsigned int)size);
}

/* Find internal node index by user-defined ID property */
static inline int find_node_by_user_id(csr_graph *graph, const char *user_id)
{
    return csr_find_user_id(graph, user_id);
}

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
                         double *dist, int *prev, int *prev_edge);

/*
 * Hop-bounded variant of algo_dijkstra_search: the minimum-cost path from
 * source_idx to target_idx using at most max_hops edges.
 *
 * The search state is (node, hops), one round per hop, so a plain
 * Dijkstra pruned at the cap would not be exact - see the implementation
 * for the counterexample shape.  Bans and weights work exactly as in
 * algo_dijkstra_search; callers must not ban the source or the target.
 *
 * The caller owns path_nodes (max_hops + 1 entries) and path_slots
 * (max_hops entries).  Returns 1 when a path exists (path_len and
 * path_distance filled), 0 when none fits the budget, -1 on allocation
 * failure.
 */
int algo_dijkstra_search_bounded(const csr_graph *graph, const double *weights,
                                 int source_idx, int target_idx,
                                 const char *banned_nodes, const char *banned_edges,
                                 int max_hops,
                                 int *path_nodes, int *path_slots, int *path_len,
                                 double *path_distance);

/*
 * Min-heap for priority queue algorithms (Dijkstra, A*, etc.)
 */
typedef struct {
    int node;
    double dist;
} heap_entry;

typedef struct {
    heap_entry *data;
    int size;
    int capacity;
} min_heap;

static inline min_heap* heap_create(int capacity)
{
    min_heap *h = malloc(sizeof(min_heap));
    if (!h) return NULL;
    h->data = malloc(capacity * sizeof(heap_entry));
    if (!h->data) { free(h); return NULL; }
    h->size = 0;
    h->capacity = capacity;
    return h;
}

static inline void heap_free(min_heap *h)
{
    if (h) {
        free(h->data);
        free(h);
    }
}

static inline void heap_push(min_heap *h, int node, double dist)
{
    if (h->size >= h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, h->capacity * sizeof(heap_entry));
    }

    int i = h->size++;
    h->data[i].node = node;
    h->data[i].dist = dist;

    /* Bubble up */
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent].dist <= h->data[i].dist) break;
        heap_entry tmp = h->data[parent];
        h->data[parent] = h->data[i];
        h->data[i] = tmp;
        i = parent;
    }
}

static inline heap_entry heap_pop(min_heap *h)
{
    heap_entry result = h->data[0];
    h->data[0] = h->data[--h->size];

    /* Bubble down */
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < h->size && h->data[left].dist < h->data[smallest].dist)
            smallest = left;
        if (right < h->size && h->data[right].dist < h->data[smallest].dist)
            smallest = right;

        if (smallest == i) break;

        heap_entry tmp = h->data[i];
        h->data[i] = h->data[smallest];
        h->data[smallest] = tmp;
        i = smallest;
    }

    return result;
}

#endif /* GRAPH_ALGO_INTERNAL_H */
