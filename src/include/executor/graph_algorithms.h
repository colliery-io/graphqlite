#ifndef GRAPH_ALGORITHMS_H
#define GRAPH_ALGORITHMS_H

#include "graphqlite_sqlite.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "parser/cypher_ast.h"

/*
 * Graph Algorithms Module
 *
 * Provides high-performance C implementations of graph algorithms
 * that would be too slow to implement in pure SQL.
 *
 * Uses Compressed Sparse Row (CSR) format for efficient graph traversal.
 */

/* CSR Graph representation for efficient algorithm execution */
typedef struct csr_graph {
    int node_count;       /* Number of nodes */
    int edge_count;       /* Number of edges */

    int *row_ptr;         /* Size: node_count + 1. row_ptr[i] = start of node i's edges in col_idx */
    int *col_idx;         /* Size: edge_count. Target node IDs for each edge */

    int *node_ids;        /* Size: node_count. Maps internal index -> original node ID (rowid) */
    char **user_ids;      /* Size: node_count. Maps internal index -> user-defined 'id' property */
    int *node_idx;        /* Hash table: original node ID -> internal index (for reverse lookup) */
    int node_idx_size;    /* Size of node_idx hash table */

    /* For algorithms needing incoming edges (like PageRank) */
    int *in_row_ptr;      /* Size: node_count + 1. Incoming edge offsets */
    int *in_col_idx;      /* Size: edge_count. Source node IDs for incoming edges */

    /* Perf review (smaller items): open-addressing hash from user id string
     * to internal index, built at load, so dijkstra/astar/bfs/dfs/knn/
     * nodeSimilarity endpoints cost O(1) instead of a strcmp scan of every
     * node. -1 marks an empty slot. */
    int *user_hash;
    int user_hash_size;
} csr_graph;

static inline unsigned int csr_hash_str(const char *s)
{
    unsigned int h = 5381u;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
    return h;
}

/* O(1) lookup of a node's internal index by its user-defined id property.
 * Falls back to a linear scan if the hash could not be built. */
static inline int csr_find_user_id(const csr_graph *graph, const char *user_id)
{
    if (!graph || !graph->user_ids || !user_id) return -1;
    if (graph->user_hash && graph->user_hash_size > 0) {
        unsigned int mask = (unsigned int)graph->user_hash_size - 1u;
        unsigned int h = csr_hash_str(user_id) & mask;
        for (int probes = 0; probes < graph->user_hash_size; probes++) {
            int idx = graph->user_hash[h];
            if (idx < 0) return -1;
            if (graph->user_ids[idx] && strcmp(graph->user_ids[idx], user_id) == 0) return idx;
            h = (h + 1u) & mask;
        }
        return -1;
    }
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->user_ids[i] && strcmp(graph->user_ids[i], user_id) == 0) return i;
    }
    return -1;
}

/* Perf review F4: one sorted copy of col_idx, with each node's adjacency
 * segment sorted ascending, so set operations on neighbour lists (Jaccard
 * intersection/union) need no per-pair allocation or sort. Segment i is
 * sorted[row_ptr[i] .. row_ptr[i+1]). Returns NULL when the graph has no
 * edges (callers treat every degree as 0 in that case). Caller frees. */
static inline int csr_cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static inline int *csr_sorted_col_idx(const csr_graph *graph)
{
    if (!graph || graph->edge_count <= 0 || !graph->col_idx) return NULL;
    int *sorted = (int *)malloc((size_t)graph->edge_count * sizeof(int));
    if (!sorted) return NULL;
    memcpy(sorted, graph->col_idx, (size_t)graph->edge_count * sizeof(int));
    for (int i = 0; i < graph->node_count; i++) {
        int start = graph->row_ptr[i], deg = graph->row_ptr[i + 1] - start;
        if (deg > 1) qsort(sorted + start, (size_t)deg, sizeof(int), csr_cmp_int_asc);
    }
    return sorted;
}

/* Graph algorithm result */
typedef struct {
    bool success;
    char *error_message;
    char *json_result;    /* JSON-formatted result string */
} graph_algo_result;

/* Graph loading */
csr_graph* csr_graph_load(sqlite3 *db);
void csr_graph_free(csr_graph *graph);

/* Algorithm detection - check if a RETURN clause contains a graph algorithm function */
typedef enum {
    GRAPH_ALGO_NONE = 0,
    GRAPH_ALGO_PAGERANK,
    GRAPH_ALGO_LABEL_PROPAGATION,
    GRAPH_ALGO_DIJKSTRA,
    GRAPH_ALGO_DEGREE_CENTRALITY,
    GRAPH_ALGO_WCC,
    GRAPH_ALGO_SCC,
    GRAPH_ALGO_BETWEENNESS_CENTRALITY,
    GRAPH_ALGO_CLOSENESS_CENTRALITY,
    GRAPH_ALGO_LOUVAIN,
    GRAPH_ALGO_TRIANGLE_COUNT,
    GRAPH_ALGO_ASTAR,
    GRAPH_ALGO_BFS,
    GRAPH_ALGO_DFS,
    GRAPH_ALGO_NODE_SIMILARITY,
    GRAPH_ALGO_KNN,
    GRAPH_ALGO_EIGENVECTOR_CENTRALITY,
    GRAPH_ALGO_APSP
} graph_algo_type;

typedef struct {
    graph_algo_type type;
    double damping;       /* For PageRank (default 0.85) */
    int iterations;       /* Number of iterations */
    int top_k;            /* For topPageRank - return top k nodes (0 = all) */
    char *source_id;      /* For Dijkstra - source node user ID */
    char *target_id;      /* For Dijkstra - target node user ID */
    char *weight_prop;    /* For Dijkstra - optional edge weight property */
    double resolution;    /* For Louvain - resolution parameter (default 1.0) */
    char *lat_prop;       /* For A* - latitude/y property name */
    char *lon_prop;       /* For A* - longitude/x property name */
    int max_depth;        /* For BFS/DFS - max traversal depth (-1 = unlimited) */
    double threshold;     /* For Node Similarity - minimum similarity threshold (default 0.0) */
    int k;                /* For KNN - number of neighbors to return */
} graph_algo_params;

/* Check if RETURN clause contains a graph algorithm call and extract parameters */
graph_algo_params detect_graph_algorithm(cypher_return *return_clause, const char *params_json);

/* Algorithm implementations
 * All algorithms accept an optional cached CSR graph parameter.
 * If cached is non-NULL, uses it directly (fast path).
 * If cached is NULL, loads graph from SQLite (original behavior).
 */
graph_algo_result* execute_pagerank(sqlite3 *db, csr_graph *cached, double damping, int iterations, int top_k);
graph_algo_result* execute_label_propagation(sqlite3 *db, csr_graph *cached, int iterations);
graph_algo_result* execute_dijkstra(sqlite3 *db, csr_graph *cached, const char *source_id, const char *target_id, const char *weight_prop);
graph_algo_result* execute_degree_centrality(sqlite3 *db, csr_graph *cached);
graph_algo_result* execute_wcc(sqlite3 *db, csr_graph *cached);
graph_algo_result* execute_scc(sqlite3 *db, csr_graph *cached);
graph_algo_result* execute_betweenness_centrality(sqlite3 *db, csr_graph *cached);
graph_algo_result* execute_closeness_centrality(sqlite3 *db, csr_graph *cached);
graph_algo_result* execute_louvain(sqlite3 *db, csr_graph *cached, double resolution);
graph_algo_result* execute_triangle_count(sqlite3 *db, csr_graph *cached);
graph_algo_result* execute_astar(sqlite3 *db, csr_graph *cached, const char *source_id, const char *target_id,
                                  const char *weight_prop, const char *lat_prop, const char *lon_prop);
graph_algo_result* execute_bfs(sqlite3 *db, csr_graph *cached, const char *start_id, int max_depth);
graph_algo_result* execute_dfs(sqlite3 *db, csr_graph *cached, const char *start_id, int max_depth);
graph_algo_result* execute_node_similarity(sqlite3 *db, csr_graph *cached, const char *node1_id, const char *node2_id, double threshold, int top_k);
graph_algo_result* execute_knn(sqlite3 *db, csr_graph *cached, const char *node_id, int k);
graph_algo_result* execute_eigenvector_centrality(sqlite3 *db, csr_graph *cached, int iterations);
graph_algo_result* execute_apsp(sqlite3 *db, csr_graph *cached);

/* Result management */
void graph_algo_result_free(graph_algo_result *result);

#endif /* GRAPH_ALGORITHMS_H */
