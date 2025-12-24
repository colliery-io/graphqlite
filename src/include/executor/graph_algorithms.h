#ifndef GRAPH_ALGORITHMS_H
#define GRAPH_ALGORITHMS_H

#include "graphqlite_sqlite.h"
#include <stdbool.h>
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
typedef struct {
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
} csr_graph;

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
    GRAPH_ALGO_LABEL_PROPAGATION
} graph_algo_type;

typedef struct {
    graph_algo_type type;
    double damping;       /* For PageRank (default 0.85) */
    int iterations;       /* Number of iterations */
    int top_k;            /* For topPageRank - return top k nodes (0 = all) */
} graph_algo_params;

/* Check if RETURN clause contains a graph algorithm call and extract parameters */
graph_algo_params detect_graph_algorithm(cypher_return *return_clause);

/* Algorithm implementations */
graph_algo_result* execute_pagerank(sqlite3 *db, double damping, int iterations, int top_k);
graph_algo_result* execute_label_propagation(sqlite3 *db, int iterations);

/* Result management */
void graph_algo_result_free(graph_algo_result *result);

#endif /* GRAPH_ALGORITHMS_H */
