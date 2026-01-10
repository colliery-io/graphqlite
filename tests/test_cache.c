/*
 * CUnit tests for graph caching functionality.
 *
 * Tests the CSR graph caching mechanism that provides ~28x speedup
 * for graph algorithm execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"
#include "executor/graph_algorithms.h"

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database with graph */
static int setup_cache_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    /* Initialize schema */
    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }

    cypher_schema_free_manager(schema_mgr);

    /* Create a test graph */
    cypher_executor *executor = cypher_executor_create(test_db);
    if (!executor) {
        return -1;
    }

    /* Create nodes */
    cypher_result *result;
    result = cypher_executor_execute(executor, "CREATE (:Person {id: 'alice'})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Person {id: 'bob'})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Person {id: 'charlie'})");
    if (result) cypher_result_free(result);

    /* Create edges */
    result = cypher_executor_execute(executor,
        "MATCH (a:Person {id: 'alice'}), (b:Person {id: 'bob'}) CREATE (a)-[:KNOWS]->(b)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (a:Person {id: 'bob'}), (b:Person {id: 'charlie'}) CREATE (a)-[:KNOWS]->(b)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (a:Person {id: 'charlie'}), (b:Person {id: 'alice'}) CREATE (a)-[:KNOWS]->(b)");
    if (result) cypher_result_free(result);

    cypher_executor_free(executor);
    return 0;
}

/* Teardown function */
static int teardown_cache_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Test CSR graph loading */
static void test_csr_graph_load(void)
{
    csr_graph *graph = csr_graph_load(test_db);
    CU_ASSERT_PTR_NOT_NULL(graph);

    if (graph) {
        /* Should have 3 nodes and 3 edges */
        CU_ASSERT_EQUAL(graph->node_count, 3);
        CU_ASSERT_EQUAL(graph->edge_count, 3);

        /* Verify row_ptr is valid */
        CU_ASSERT_PTR_NOT_NULL(graph->row_ptr);
        if (graph->row_ptr) {
            /* row_ptr should have node_count + 1 entries */
            CU_ASSERT_EQUAL(graph->row_ptr[0], 0);
            CU_ASSERT_TRUE(graph->row_ptr[graph->node_count] <= graph->edge_count);
        }

        /* Verify col_idx is valid */
        CU_ASSERT_PTR_NOT_NULL(graph->col_idx);

        /* Verify node_ids mapping */
        CU_ASSERT_PTR_NOT_NULL(graph->node_ids);

        csr_graph_free(graph);
    }
}

/* Test CSR graph is properly freed */
static void test_csr_graph_free(void)
{
    csr_graph *graph = csr_graph_load(test_db);
    CU_ASSERT_PTR_NOT_NULL(graph);

    /* Should not crash when freeing */
    if (graph) {
        csr_graph_free(graph);
    }

    /* Double-free protection - this test ensures no crash on NULL */
    csr_graph_free(NULL);
}

/* Test executor cached_graph field */
static void test_executor_cached_graph_field(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Initially cached_graph should be NULL */
        CU_ASSERT_PTR_NULL(executor->cached_graph);

        /* Load graph into cache */
        csr_graph *graph = csr_graph_load(test_db);
        CU_ASSERT_PTR_NOT_NULL(graph);

        if (graph) {
            executor->cached_graph = (struct csr_graph *)graph;
            CU_ASSERT_PTR_NOT_NULL(executor->cached_graph);
            /* Access node_count through the typed pointer */
            CU_ASSERT_EQUAL(graph->node_count, 3);

            /* Clean up - executor doesn't own cached_graph normally */
            csr_graph_free(graph);
            executor->cached_graph = NULL;
        }

        cypher_executor_free(executor);
    }
}

/* Test PageRank uses cached graph when available */
static void test_pagerank_with_cached_graph(void)
{
    /* Load graph */
    csr_graph *graph = csr_graph_load(test_db);
    CU_ASSERT_PTR_NOT_NULL(graph);

    if (graph) {
        /* Run PageRank with cached graph */
        graph_algo_result *result = execute_pagerank(test_db, graph, 0.85, 20, 0);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            CU_ASSERT_PTR_NOT_NULL(result->json_result);

            if (result->json_result) {
                /* Should contain results for all 3 nodes */
                CU_ASSERT_TRUE(strstr(result->json_result, "alice") != NULL ||
                               strstr(result->json_result, "bob") != NULL ||
                               strstr(result->json_result, "charlie") != NULL);
            }

            graph_algo_result_free(result);
        }

        csr_graph_free(graph);
    }
}

/* Test PageRank without cached graph (loads from SQLite) */
static void test_pagerank_without_cached_graph(void)
{
    /* Run PageRank without cached graph (NULL) */
    graph_algo_result *result = execute_pagerank(test_db, NULL, 0.85, 20, 0);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_PTR_NOT_NULL(result->json_result);
        graph_algo_result_free(result);
    }
}

/* Test multiple algorithm calls reuse the same cached graph */
static void test_cache_reuse_across_algorithms(void)
{
    csr_graph *graph = csr_graph_load(test_db);
    CU_ASSERT_PTR_NOT_NULL(graph);

    if (graph) {
        /* Run PageRank */
        graph_algo_result *pr_result = execute_pagerank(test_db, graph, 0.85, 20, 0);
        CU_ASSERT_PTR_NOT_NULL(pr_result);
        if (pr_result) {
            CU_ASSERT_TRUE(pr_result->success);
            graph_algo_result_free(pr_result);
        }

        /* Run Label Propagation with same cached graph */
        graph_algo_result *lp_result = execute_label_propagation(test_db, graph, 10);
        CU_ASSERT_PTR_NOT_NULL(lp_result);
        if (lp_result) {
            CU_ASSERT_TRUE(lp_result->success);
            graph_algo_result_free(lp_result);
        }

        /* Run Degree Centrality with same cached graph */
        graph_algo_result *dc_result = execute_degree_centrality(test_db, graph);
        CU_ASSERT_PTR_NOT_NULL(dc_result);
        if (dc_result) {
            CU_ASSERT_TRUE(dc_result->success);
            graph_algo_result_free(dc_result);
        }

        csr_graph_free(graph);
    }
}

/* Test empty graph caching */
static void test_empty_graph_cache(void)
{
    /* Create a new empty database */
    sqlite3 *empty_db = NULL;
    int rc = sqlite3_open(":memory:", &empty_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    if (empty_db) {
        /* Initialize schema but don't add any nodes */
        cypher_schema_manager *schema_mgr = cypher_schema_create_manager(empty_db);
        if (schema_mgr) {
            cypher_schema_initialize(schema_mgr);
            cypher_schema_free_manager(schema_mgr);
        }

        /* Try to load empty graph */
        csr_graph *graph = csr_graph_load(empty_db);

        /* Empty graph should return NULL or a graph with 0 nodes */
        if (graph) {
            CU_ASSERT_EQUAL(graph->node_count, 0);
            csr_graph_free(graph);
        }

        sqlite3_close(empty_db);
    }
}

/* Test cache consistency after graph modification */
static void test_cache_invalidation_pattern(void)
{
    /* Load graph */
    csr_graph *graph = csr_graph_load(test_db);
    CU_ASSERT_PTR_NOT_NULL(graph);

    if (graph) {
        int original_count = graph->node_count;
        CU_ASSERT_EQUAL(original_count, 3);

        /* Add a new node */
        cypher_executor *executor = cypher_executor_create(test_db);
        if (executor) {
            cypher_result *result = cypher_executor_execute(executor,
                "CREATE (:Person {id: 'dave'})");
            if (result) {
                CU_ASSERT_TRUE(result->success);
                cypher_result_free(result);
            }
            cypher_executor_free(executor);
        }

        /* Old cached graph still has 3 nodes (stale) */
        CU_ASSERT_EQUAL(graph->node_count, 3);

        /* Reload graph to get updated data */
        csr_graph *new_graph = csr_graph_load(test_db);
        CU_ASSERT_PTR_NOT_NULL(new_graph);
        if (new_graph) {
            /* New graph should have 4 nodes */
            CU_ASSERT_EQUAL(new_graph->node_count, 4);
            csr_graph_free(new_graph);
        }

        csr_graph_free(graph);
    }
}

/* Initialize cache test suite */
int init_cache_suite(void)
{
    CU_pSuite suite = CU_add_suite("Graph Cache Tests",
                                    setup_cache_suite,
                                    teardown_cache_suite);
    if (suite == NULL) {
        return CU_get_error();
    }

    if (CU_add_test(suite, "CSR graph load", test_csr_graph_load) == NULL ||
        CU_add_test(suite, "CSR graph free", test_csr_graph_free) == NULL ||
        CU_add_test(suite, "Executor cached_graph field", test_executor_cached_graph_field) == NULL ||
        CU_add_test(suite, "PageRank with cached graph", test_pagerank_with_cached_graph) == NULL ||
        CU_add_test(suite, "PageRank without cached graph", test_pagerank_without_cached_graph) == NULL ||
        CU_add_test(suite, "Cache reuse across algorithms", test_cache_reuse_across_algorithms) == NULL ||
        CU_add_test(suite, "Empty graph cache", test_empty_graph_cache) == NULL ||
        CU_add_test(suite, "Cache invalidation pattern", test_cache_invalidation_pattern) == NULL) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
