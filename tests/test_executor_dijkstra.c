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

/* Shared executor for all tests in this suite */
static cypher_executor *shared_executor = NULL;
static sqlite3 *test_db = NULL;

/* Helper function to create test graph for Dijkstra */
static void create_dijkstra_test_graph(cypher_executor *executor)
{
    cypher_result *result;

    /* Create a graph with weighted edges:
     *
     *   A --2--> B --1--> C
     *   |        |
     *   3        4
     *   |        |
     *   v        v
     *   D --1--> E
     *
     * Shortest paths:
     *   A->C: A->B->C (distance 3)
     *   A->E: A->D->E (distance 4) is shorter than A->B->E (distance 6)
     */
    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"A\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"B\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"C\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"D\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"E\"})");
    if (result) cypher_result_free(result);

    /* Create edges with weights */
    result = cypher_executor_execute(executor,
        "MATCH (a:Node {id: \"A\"}), (b:Node {id: \"B\"}) CREATE (a)-[:CONNECTS {weight: 2}]->(b)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (b:Node {id: \"B\"}), (c:Node {id: \"C\"}) CREATE (b)-[:CONNECTS {weight: 1}]->(c)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (a:Node {id: \"A\"}), (d:Node {id: \"D\"}) CREATE (a)-[:CONNECTS {weight: 3}]->(d)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (b:Node {id: \"B\"}), (e:Node {id: \"E\"}) CREATE (b)-[:CONNECTS {weight: 4}]->(e)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (d:Node {id: \"D\"}), (e:Node {id: \"E\"}) CREATE (d)-[:CONNECTS {weight: 1}]->(e)");
    if (result) cypher_result_free(result);
}

/* Setup function - create executor with test data */
static int setup_dijkstra_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    shared_executor = cypher_executor_create(test_db);
    if (!shared_executor) {
        sqlite3_close(test_db);
        return -1;
    }

    /* Create test graph */
    create_dijkstra_test_graph(shared_executor);

    return 0;
}

/* Teardown function */
static int teardown_dijkstra_suite(void)
{
    if (shared_executor) {
        cypher_executor_free(shared_executor);
        shared_executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Test basic dijkstra() function - direct path */
static void test_dijkstra_basic(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"A\", \"B\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("Dijkstra error: %s\n", result->error_message);
        }

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(json);

            /* Should find path and distance */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"A\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"B\""));
        }

        cypher_result_free(result);
    }
}

/* Test dijkstra with multi-hop path */
static void test_dijkstra_multi_hop(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"A\", \"C\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(json);

            /* Should find path A->B->C */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"A\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"B\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"C\""));
            /* Distance should be 2 (unweighted) */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":2"));
        }

        cypher_result_free(result);
    }
}

/* Test dijkstra path to same node */
static void test_dijkstra_same_node(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"A\", \"A\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(json);

            /* Should find trivial path with distance 0 */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":0"));
        }

        cypher_result_free(result);
    }
}

/* Test dijkstra with no path */
static void test_dijkstra_no_path(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* C has no outgoing edges, so C->A should fail */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"C\", \"A\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(json);

            /* Should not find path */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":false"));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"path\":[]"));
        }

        cypher_result_free(result);
    }
}

/* Test dijkstra with non-existent node */
static void test_dijkstra_nonexistent_node(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"A\", \"Z\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(json);

            /* Should not find path */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":false"));
        }

        cypher_result_free(result);
    }
}

/* Test dijkstra on empty graph */
static void test_dijkstra_empty_graph(void)
{
    /* Create a fresh executor with empty graph */
    sqlite3 *empty_db;
    int rc = sqlite3_open(":memory:", &empty_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    cypher_executor *empty_executor = cypher_executor_create(empty_db);
    CU_ASSERT_PTR_NOT_NULL(empty_executor);

    if (empty_executor) {
        cypher_result *result = cypher_executor_execute(empty_executor,
            "RETURN dijkstra(\"A\", \"B\")");

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            /* Empty graph should return not found */
            if (result->success && result->row_count > 0 && result->data) {
                const char *json = result->data[0][0];
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":false"));
            }
            cypher_result_free(result);
        }

        cypher_executor_free(empty_executor);
    }

    sqlite3_close(empty_db);
}

/* Test dijkstra verifies correct shortest path choice */
static void test_dijkstra_shortest_path_choice(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* A->E has two paths:
     * A->B->E (distance 2)
     * A->D->E (distance 2)
     * Both are valid shortest paths */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"A\", \"E\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(json);

            /* Should find path with distance 2 */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":2"));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"A\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"E\""));
        }

        cypher_result_free(result);
    }
}

/* Run one query on a fresh in-memory graph built by setup_queries.
 * Returns the first cell (caller frees) or NULL. */
static char *dijkstra_fresh_query(const char **setup_queries, const char *query)
{
    sqlite3 *db = NULL;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return NULL;
    cypher_executor *ex = cypher_executor_create(db);
    char *out = NULL;
    if (ex) {
        for (int i = 0; setup_queries[i]; i++) {
            cypher_result *r = cypher_executor_execute(ex, setup_queries[i]);
            if (r) cypher_result_free(r);
        }
        cypher_result *r = cypher_executor_execute(ex, query);
        if (r) {
            if (r->success && r->row_count > 0 && r->data && r->data[0][0]) {
                out = strdup(r->data[0][0]);
            } else if (!r->success) {
                printf("Dijkstra error: %s\n", r->error_message);
            }
            cypher_result_free(r);
        }
        cypher_executor_free(ex);
    }
    sqlite3_close(db);
    return out;
}

/* #122: integer weights must be read, not treated as 1. */
static void test_dijkstra_integer_weights(void)
{
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN dijkstra(\"A\", \"E\", \"weight\")");
    CU_ASSERT_PTR_NOT_NULL_FATAL(result);
    CU_ASSERT_TRUE(result->success);
    if (result->success && result->row_count > 0 && result->data) {
        const char *json = result->data[0][0];
        /* A->D->E = 3+1 = 4 beats A->B->E = 2+4 = 6 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":4"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "[\"A\",\"D\",\"E\"]"));
    }
    cypher_result_free(result);
}

/* #122 reproduction: the cheap two-hop path beats the expensive direct edge. */
static void test_dijkstra_integer_weights_issue_122(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'}), (:Node {id: 'b'}), (:Node {id: 'c'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 1}]->(b)",
        "MATCH (b:Node {id: 'b'}), (c:Node {id: 'c'}) CREATE (b)-[:LINK {cost: 2}]->(c)",
        "MATCH (a:Node {id: 'a'}), (c:Node {id: 'c'}) CREATE (a)-[:LINK {cost: 10}]->(c)",
        NULL
    };
    char *json = dijkstra_fresh_query(setup, "RETURN dijkstra('a', 'c', 'cost')");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":3"));
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "[\"a\",\"b\",\"c\"]"));
    free(json);
}

/* Integer and float weights in one graph. */
static void test_dijkstra_mixed_int_float_weights(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'}), (:Node {id: 'b'}), (:Node {id: 'c'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 1}]->(b)",
        "MATCH (b:Node {id: 'b'}), (c:Node {id: 'c'}) CREATE (b)-[:LINK {cost: 0.5}]->(c)",
        "MATCH (a:Node {id: 'a'}), (c:Node {id: 'c'}) CREATE (a)-[:LINK {cost: 2}]->(c)",
        NULL
    };
    char *json = dijkstra_fresh_query(setup, "RETURN dijkstra('a', 'c', 'cost')");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":1.5"));
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "[\"a\",\"b\",\"c\"]"));
    free(json);
}

/* #122: parallel edges keep their own weights; the cheapest one wins,
 * whichever order the edges were created in. */
static void test_dijkstra_parallel_edges(void)
{
    const char *slow_first[] = {
        "CREATE (:Node {id: 'a'}), (:Node {id: 'b'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 2.0}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 0.5}]->(b)",
        NULL
    };
    const char *fast_first[] = {
        "CREATE (:Node {id: 'a'}), (:Node {id: 'b'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 0.5}]->(b)",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK {cost: 2.0}]->(b)",
        NULL
    };
    char *json = dijkstra_fresh_query(slow_first, "RETURN dijkstra('a', 'b', 'cost')");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":0.5"));
    free(json);

    json = dijkstra_fresh_query(fast_first, "RETURN dijkstra('a', 'b', 'cost')");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":0.5"));
    free(json);
}

/* Edges without the weight property weigh 1. */
static void test_dijkstra_missing_weight_defaults_to_one(void)
{
    const char *setup[] = {
        "CREATE (:Node {id: 'a'}), (:Node {id: 'b'}), (:Node {id: 'c'})",
        "MATCH (a:Node {id: 'a'}), (b:Node {id: 'b'}) CREATE (a)-[:LINK]->(b)",
        "MATCH (b:Node {id: 'b'}), (c:Node {id: 'c'}) CREATE (b)-[:LINK]->(c)",
        "MATCH (a:Node {id: 'a'}), (c:Node {id: 'c'}) CREATE (a)-[:LINK {cost: 5}]->(c)",
        NULL
    };
    char *json = dijkstra_fresh_query(setup, "RETURN dijkstra('a', 'c', 'cost')");
    CU_ASSERT_PTR_NOT_NULL_FATAL(json);
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":2"));
    CU_ASSERT_PTR_NOT_NULL(strstr(json, "[\"a\",\"b\",\"c\"]"));
    free(json);
}

/* Initialize the Dijkstra executor test suite */
int init_executor_dijkstra_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Dijkstra", setup_dijkstra_suite, teardown_dijkstra_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "Dijkstra basic", test_dijkstra_basic) ||
        !CU_add_test(suite, "Dijkstra multi-hop path", test_dijkstra_multi_hop) ||
        !CU_add_test(suite, "Dijkstra same node", test_dijkstra_same_node) ||
        !CU_add_test(suite, "Dijkstra no path", test_dijkstra_no_path) ||
        !CU_add_test(suite, "Dijkstra non-existent node", test_dijkstra_nonexistent_node) ||
        !CU_add_test(suite, "Dijkstra empty graph", test_dijkstra_empty_graph) ||
        !CU_add_test(suite, "Dijkstra shortest path choice", test_dijkstra_shortest_path_choice) ||
        !CU_add_test(suite, "Dijkstra integer weights", test_dijkstra_integer_weights) ||
        !CU_add_test(suite, "Dijkstra integer weights (#122)", test_dijkstra_integer_weights_issue_122) ||
        !CU_add_test(suite, "Dijkstra mixed int/float weights", test_dijkstra_mixed_int_float_weights) ||
        !CU_add_test(suite, "Dijkstra parallel edges (#122)", test_dijkstra_parallel_edges) ||
        !CU_add_test(suite, "Dijkstra missing weight defaults to 1", test_dijkstra_missing_weight_defaults_to_one)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
