/*
 * Graph Algorithms - High-performance C implementations
 *
 * Provides PageRank, Label Propagation, and other graph algorithms
 * using CSR (Compressed Sparse Row) format for efficiency.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "executor/graph_algorithms.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"

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

/* Free CSR graph */
void csr_graph_free(csr_graph *graph)
{
    if (!graph) return;

    free(graph->row_ptr);
    free(graph->col_idx);
    free(graph->node_ids);
    free(graph->node_idx);
    free(graph->in_row_ptr);
    free(graph->in_col_idx);
    free(graph);
}

/* Load graph from SQLite into CSR format */
csr_graph* csr_graph_load(sqlite3 *db)
{
    if (!db) return NULL;

    csr_graph *graph = calloc(1, sizeof(csr_graph));
    if (!graph) return NULL;

    sqlite3_stmt *stmt = NULL;
    int rc;

    /* Step 1: Count nodes and get node IDs */
    rc = sqlite3_prepare_v2(db, "SELECT id FROM nodes ORDER BY id", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to prepare node query: %s", sqlite3_errmsg(db));
        free(graph);
        return NULL;
    }

    /* First pass: count nodes */
    int node_capacity = 1024;
    graph->node_ids = malloc(node_capacity * sizeof(int));
    if (!graph->node_ids) {
        sqlite3_finalize(stmt);
        free(graph);
        return NULL;
    }

    graph->node_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (graph->node_count >= node_capacity) {
            node_capacity *= 2;
            graph->node_ids = realloc(graph->node_ids, node_capacity * sizeof(int));
            if (!graph->node_ids) {
                sqlite3_finalize(stmt);
                csr_graph_free(graph);
                return NULL;
            }
        }
        graph->node_ids[graph->node_count++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (graph->node_count == 0) {
        CYPHER_DEBUG("No nodes found in graph");
        csr_graph_free(graph);
        return NULL;
    }

    CYPHER_DEBUG("Loaded %d nodes", graph->node_count);

    /* Build node ID -> index hash table */
    graph->node_idx_size = HASH_TABLE_SIZE;
    graph->node_idx = malloc(graph->node_idx_size * sizeof(int));
    if (!graph->node_idx) {
        csr_graph_free(graph);
        return NULL;
    }

    /* Initialize hash table with -1 (empty) */
    for (int i = 0; i < graph->node_idx_size; i++) {
        graph->node_idx[i] = -1;
    }

    /* Insert node IDs into hash table (linear probing) */
    for (int i = 0; i < graph->node_count; i++) {
        int node_id = graph->node_ids[i];
        int h = hash_int(node_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1) {
            h = (h + 1) % graph->node_idx_size;
        }
        graph->node_idx[h] = i;  /* Store index, not node_id */
    }

    /* Step 2: Count edges per node (for row_ptr) */
    graph->row_ptr = calloc(graph->node_count + 1, sizeof(int));
    graph->in_row_ptr = calloc(graph->node_count + 1, sizeof(int));
    if (!graph->row_ptr || !graph->in_row_ptr) {
        csr_graph_free(graph);
        return NULL;
    }

    /* Count outgoing and incoming edges per node */
    rc = sqlite3_prepare_v2(db, "SELECT source_id, target_id FROM edges", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        csr_graph_free(graph);
        return NULL;
    }

    graph->edge_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int source_id = sqlite3_column_int(stmt, 0);
        int target_id = sqlite3_column_int(stmt, 1);

        /* Find source index */
        int h = hash_int(source_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != source_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int source_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        /* Find target index */
        h = hash_int(target_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != target_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int target_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        if (source_idx >= 0 && target_idx >= 0) {
            graph->row_ptr[source_idx + 1]++;    /* Outgoing from source */
            graph->in_row_ptr[target_idx + 1]++; /* Incoming to target */
            graph->edge_count++;
        }
    }
    sqlite3_finalize(stmt);

    CYPHER_DEBUG("Loaded %d edges", graph->edge_count);

    /* Convert counts to cumulative offsets */
    for (int i = 1; i <= graph->node_count; i++) {
        graph->row_ptr[i] += graph->row_ptr[i - 1];
        graph->in_row_ptr[i] += graph->in_row_ptr[i - 1];
    }

    /* Step 3: Fill col_idx arrays */
    graph->col_idx = malloc(graph->edge_count * sizeof(int));
    graph->in_col_idx = malloc(graph->edge_count * sizeof(int));
    if (!graph->col_idx || !graph->in_col_idx) {
        csr_graph_free(graph);
        return NULL;
    }

    /* Temporary counters for filling arrays */
    int *out_count = calloc(graph->node_count, sizeof(int));
    int *in_count = calloc(graph->node_count, sizeof(int));
    if (!out_count || !in_count) {
        free(out_count);
        free(in_count);
        csr_graph_free(graph);
        return NULL;
    }

    /* Second pass: fill edge arrays */
    rc = sqlite3_prepare_v2(db, "SELECT source_id, target_id FROM edges", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(out_count);
        free(in_count);
        csr_graph_free(graph);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int source_id = sqlite3_column_int(stmt, 0);
        int target_id = sqlite3_column_int(stmt, 1);

        /* Find indices */
        int h = hash_int(source_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != source_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int source_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        h = hash_int(target_id, graph->node_idx_size);
        while (graph->node_idx[h] != -1 && graph->node_ids[graph->node_idx[h]] != target_id) {
            h = (h + 1) % graph->node_idx_size;
        }
        int target_idx = (graph->node_idx[h] != -1) ? graph->node_idx[h] : -1;

        if (source_idx >= 0 && target_idx >= 0) {
            /* Outgoing edge: source -> target */
            int out_pos = graph->row_ptr[source_idx] + out_count[source_idx]++;
            graph->col_idx[out_pos] = target_idx;

            /* Incoming edge: target <- source */
            int in_pos = graph->in_row_ptr[target_idx] + in_count[target_idx]++;
            graph->in_col_idx[in_pos] = source_idx;
        }
    }
    sqlite3_finalize(stmt);

    free(out_count);
    free(in_count);

    CYPHER_DEBUG("CSR graph loaded: %d nodes, %d edges", graph->node_count, graph->edge_count);

    return graph;
}

/* Detect graph algorithm in RETURN clause */
graph_algo_params detect_graph_algorithm(cypher_return *return_clause)
{
    graph_algo_params params = {0};
    params.type = GRAPH_ALGO_NONE;
    params.damping = 0.85;
    params.iterations = 20;
    params.top_k = 0;

    if (!return_clause || !return_clause->items || return_clause->items->count == 0) {
        return params;
    }

    /* Check first return item for a function call */
    cypher_return_item *item = (cypher_return_item *)return_clause->items->items[0];
    if (!item || !item->expr || item->expr->type != AST_NODE_FUNCTION_CALL) {
        return params;
    }

    cypher_function_call *func = (cypher_function_call *)item->expr;
    if (!func->function_name) {
        return params;
    }

    /* Check for pageRank */
    if (strcasecmp(func->function_name, "pageRank") == 0) {
        params.type = GRAPH_ALGO_PAGERANK;

        /* Parse arguments: pageRank() or pageRank(damping) or pageRank(damping, iterations) */
        if (func->args && func->args->count >= 1) {
            cypher_literal *damp_lit = (cypher_literal *)func->args->items[0];
            if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
                if (damp_lit->literal_type == LITERAL_DECIMAL) {
                    params.damping = damp_lit->value.decimal;
                } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                    params.damping = (double)damp_lit->value.integer;
                }
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[1];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 100) params.iterations = 100;
            }
        }
        return params;
    }

    /* Check for topPageRank */
    if (strcasecmp(func->function_name, "topPageRank") == 0) {
        params.type = GRAPH_ALGO_PAGERANK;
        params.top_k = 10;  /* Default */

        /* Parse arguments: topPageRank(k) or topPageRank(k, damping, iterations) */
        if (func->args && func->args->count >= 1) {
            cypher_literal *k_lit = (cypher_literal *)func->args->items[0];
            if (k_lit && k_lit->base.type == AST_NODE_LITERAL &&
                k_lit->literal_type == LITERAL_INTEGER) {
                params.top_k = k_lit->value.integer;
                if (params.top_k < 1) params.top_k = 1;
                if (params.top_k > 1000) params.top_k = 1000;
            }
        }
        if (func->args && func->args->count >= 2) {
            cypher_literal *damp_lit = (cypher_literal *)func->args->items[1];
            if (damp_lit && damp_lit->base.type == AST_NODE_LITERAL) {
                if (damp_lit->literal_type == LITERAL_DECIMAL) {
                    params.damping = damp_lit->value.decimal;
                } else if (damp_lit->literal_type == LITERAL_INTEGER) {
                    params.damping = (double)damp_lit->value.integer;
                }
            }
        }
        if (func->args && func->args->count >= 3) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[2];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 100) params.iterations = 100;
            }
        }
        return params;
    }

    /* Check for labelPropagation */
    if (strcasecmp(func->function_name, "labelPropagation") == 0) {
        params.type = GRAPH_ALGO_LABEL_PROPAGATION;
        params.iterations = 10;  /* Default for label propagation */

        /* Parse arguments: labelPropagation() or labelPropagation(iterations) */
        if (func->args && func->args->count >= 1) {
            cypher_literal *iter_lit = (cypher_literal *)func->args->items[0];
            if (iter_lit && iter_lit->base.type == AST_NODE_LITERAL &&
                iter_lit->literal_type == LITERAL_INTEGER) {
                params.iterations = iter_lit->value.integer;
                if (params.iterations < 1) params.iterations = 1;
                if (params.iterations > 100) params.iterations = 100;
            }
        }
        return params;
    }

    return params;
}

/* Free algorithm result */
void graph_algo_result_free(graph_algo_result *result)
{
    if (!result) return;
    free(result->error_message);
    free(result->json_result);
    free(result);
}

/* Comparison function for sorting PageRank results */
typedef struct {
    int node_id;
    double score;
} pr_result;

static int compare_pr_desc(const void *a, const void *b)
{
    double diff = ((pr_result *)b)->score - ((pr_result *)a)->score;
    if (diff > 0) return 1;
    if (diff < 0) return -1;
    return 0;
}

/*
 * Execute PageRank algorithm (optimized)
 *
 * Formula: PR(n) = (1-d)/N + d * SUM(PR(m)/out_degree(m)) for all m -> n
 *
 * Optimizations:
 * - Uses float instead of double (2x memory bandwidth)
 * - Pre-computes 1/out_degree to avoid division in inner loop
 * - Early convergence detection (stops if max change < 1e-6)
 * - Push-based approach for better cache locality on outgoing edges
 */
graph_algo_result* execute_pagerank(sqlite3 *db, double damping, int iterations, int top_k)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based PageRank: damping=%.2f, iterations=%d, top_k=%d",
                 damping, iterations, top_k);

    /* Load graph into CSR format */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        /* Empty graph - return empty array (not an error) */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;
    float dampf = (float)damping;

    /* Allocate PageRank arrays - use float for 2x memory bandwidth */
    float *pr = malloc(n * sizeof(float));
    float *pr_new = malloc(n * sizeof(float));
    float *inv_out_degree = malloc(n * sizeof(float));  /* Pre-computed 1/out_degree */

    if (!pr || !pr_new || !inv_out_degree) {
        free(pr);
        free(pr_new);
        free(inv_out_degree);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Pre-compute inverse out-degrees (avoids division in inner loop) */
    for (int i = 0; i < n; i++) {
        int out_deg = graph->row_ptr[i + 1] - graph->row_ptr[i];
        inv_out_degree[i] = (out_deg > 0) ? (1.0f / out_deg) : 0.0f;
    }

    /* Initialize PageRank: uniform distribution */
    float init_pr = 1.0f / n;
    for (int i = 0; i < n; i++) {
        pr[i] = init_pr;
    }

    /* PageRank iterations with convergence detection */
    float teleport = (1.0f - dampf) / n;
    float convergence_threshold = 1e-6f;
    int actual_iters = 0;

    for (int iter = 0; iter < iterations; iter++) {
        actual_iters++;

        /* Initialize new PR with teleport probability */
        for (int i = 0; i < n; i++) {
            pr_new[i] = teleport;
        }

        /* Push-based: each node distributes its rank to neighbors */
        for (int i = 0; i < n; i++) {
            float contribution = dampf * pr[i] * inv_out_degree[i];
            int out_start = graph->row_ptr[i];
            int out_end = graph->row_ptr[i + 1];

            for (int j = out_start; j < out_end; j++) {
                int target = graph->col_idx[j];
                pr_new[target] += contribution;
            }
        }

        /* Check convergence and swap */
        float max_diff = 0.0f;
        for (int i = 0; i < n; i++) {
            float diff = pr_new[i] - pr[i];
            if (diff < 0) diff = -diff;
            if (diff > max_diff) max_diff = diff;
        }

        /* Swap arrays */
        float *tmp = pr;
        pr = pr_new;
        pr_new = tmp;

        /* Early termination if converged */
        if (max_diff < convergence_threshold) {
            CYPHER_DEBUG("PageRank converged at iteration %d (max_diff=%.2e)", iter, max_diff);
            break;
        }
    }

    CYPHER_DEBUG("PageRank completed in %d iterations", actual_iters);

    /* Build results array for sorting */
    pr_result *results = malloc(n * sizeof(pr_result));
    if (!results) {
        free(pr);
        free(pr_new);
        free(inv_out_degree);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        results[i].node_id = graph->node_ids[i];
        results[i].score = (double)pr[i];  /* Convert back to double for output */
    }

    /* Sort by score descending */
    qsort(results, n, sizeof(pr_result), compare_pr_desc);

    /* Determine how many results to return */
    int result_count = (top_k > 0 && top_k < n) ? top_k : n;

    /* Build JSON output */
    size_t json_capacity = 64 + result_count * 64;
    char *json = malloc(json_capacity);
    if (!json) {
        free(results);
        free(pr);
        free(pr_new);
        free(inv_out_degree);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;

    for (int i = 0; i < result_count; i++) {
        char entry[128];
        int entry_len = snprintf(entry, sizeof(entry),
                                 "%s{\"node_id\":%d,\"score\":%.10g}",
                                 (i > 0) ? "," : "",
                                 results[i].node_id,
                                 results[i].score);

        if (json_len + entry_len >= json_capacity - 2) {
            json_capacity *= 2;
            json = realloc(json, json_capacity);
            if (!json) break;
        }

        strcat(json + json_len, entry);
        json_len += entry_len;
    }

    strcat(json, "]");

    /* Cleanup */
    free(results);
    free(pr);
    free(pr_new);
    free(inv_out_degree);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;

    CYPHER_DEBUG("PageRank completed: %d results", result_count);

    return result;
}

/*
 * Execute Label Propagation algorithm (optimized)
 *
 * Each node starts with its own label, then iteratively adopts
 * the most common label among its neighbors.
 *
 * Optimizations:
 * - Uses a counting array instead of O(d^2) nested loops for label counting
 * - Only counts labels that actually appear in neighbors (sparse counting)
 * - Early termination when no changes occur
 */
graph_algo_result* execute_label_propagation(sqlite3 *db, int iterations)
{
    graph_algo_result *result = calloc(1, sizeof(graph_algo_result));
    if (!result) return NULL;

    CYPHER_DEBUG("Executing C-based Label Propagation: iterations=%d", iterations);

    /* Load graph into CSR format */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        /* Empty graph - return empty array (not an error) */
        result->success = true;
        result->json_result = strdup("[]");
        return result;
    }

    int n = graph->node_count;

    /* Allocate label arrays */
    int *labels = malloc(n * sizeof(int));
    int *new_labels = malloc(n * sizeof(int));

    if (!labels || !new_labels) {
        free(labels);
        free(new_labels);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Initialize: each node has its own label (its index) */
    for (int i = 0; i < n; i++) {
        labels[i] = i;
    }

    /* Allocate label counting array - O(n) space but O(d) per node usage */
    int *label_counts = calloc(n, sizeof(int));
    int *touched_labels = malloc(n * sizeof(int));  /* Track which labels we've incremented */

    if (!label_counts || !touched_labels) {
        free(labels);
        free(new_labels);
        free(label_counts);
        free(touched_labels);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    /* Label propagation iterations - O(E) per iteration instead of O(E*d) */
    for (int iter = 0; iter < iterations; iter++) {
        int changes = 0;

        for (int i = 0; i < n; i++) {
            int in_start = graph->in_row_ptr[i];
            int in_end = graph->in_row_ptr[i + 1];
            int out_start = graph->row_ptr[i];
            int out_end = graph->row_ptr[i + 1];

            int neighbor_count = (in_end - in_start) + (out_end - out_start);

            if (neighbor_count == 0) {
                new_labels[i] = labels[i];
                continue;
            }

            /* Count labels using counting array - O(d) instead of O(d^2) */
            int touched_count = 0;

            /* Count incoming neighbor labels */
            for (int j = in_start; j < in_end; j++) {
                int label = labels[graph->in_col_idx[j]];
                if (label_counts[label] == 0) {
                    touched_labels[touched_count++] = label;
                }
                label_counts[label]++;
            }

            /* Count outgoing neighbor labels */
            for (int j = out_start; j < out_end; j++) {
                int label = labels[graph->col_idx[j]];
                if (label_counts[label] == 0) {
                    touched_labels[touched_count++] = label;
                }
                label_counts[label]++;
            }

            /* Find best label (highest count, tie-break by lowest label) */
            int best_label = labels[i];
            int best_count = 0;

            for (int t = 0; t < touched_count; t++) {
                int label = touched_labels[t];
                int count = label_counts[label];
                if (count > best_count || (count == best_count && label < best_label)) {
                    best_count = count;
                    best_label = label;
                }
            }

            /* Reset only touched labels (sparse reset) */
            for (int t = 0; t < touched_count; t++) {
                label_counts[touched_labels[t]] = 0;
            }

            new_labels[i] = best_label;
            if (new_labels[i] != labels[i]) changes++;
        }

        /* Swap arrays */
        int *tmp = labels;
        labels = new_labels;
        new_labels = tmp;

        CYPHER_DEBUG("Label propagation iter %d: %d changes", iter, changes);

        /* Early termination if converged */
        if (changes == 0) break;
    }

    free(label_counts);
    free(touched_labels);

    /* Count communities and collect results */
    /* Map labels to community IDs and count sizes */
    int *community_sizes = calloc(n, sizeof(int));
    int *label_to_community = malloc(n * sizeof(int));

    if (!community_sizes || !label_to_community) {
        free(labels);
        free(new_labels);
        free(community_sizes);
        free(label_to_community);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    for (int i = 0; i < n; i++) {
        label_to_community[i] = -1;
    }

    int num_communities = 0;
    for (int i = 0; i < n; i++) {
        int label = labels[i];
        if (label_to_community[label] < 0) {
            label_to_community[label] = num_communities++;
        }
        community_sizes[label_to_community[label]]++;
    }

    CYPHER_DEBUG("Label propagation found %d communities", num_communities);

    /* Build JSON output: array of {node_id, community_id} objects */
    size_t json_capacity = 64 + n * 48;
    char *json = malloc(json_capacity);
    if (!json) {
        free(labels);
        free(new_labels);
        free(community_sizes);
        free(label_to_community);
        csr_graph_free(graph);
        result->success = false;
        result->error_message = strdup("Memory allocation failed");
        return result;
    }

    strcpy(json, "[");
    size_t json_len = 1;

    for (int i = 0; i < n; i++) {
        char entry[96];
        int community_id = label_to_community[labels[i]];
        int entry_len = snprintf(entry, sizeof(entry),
                                 "%s{\"node_id\":%d,\"community\":%d}",
                                 (i > 0) ? "," : "",
                                 graph->node_ids[i],
                                 community_id);

        if (json_len + entry_len >= json_capacity - 2) {
            json_capacity *= 2;
            json = realloc(json, json_capacity);
            if (!json) break;
        }

        strcat(json + json_len, entry);
        json_len += entry_len;
    }

    strcat(json, "]");

    /* Cleanup */
    free(labels);
    free(new_labels);
    free(community_sizes);
    free(label_to_community);
    csr_graph_free(graph);

    result->success = true;
    result->json_result = json;

    return result;
}
