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

/* Helper function to create test graph for degree centrality */
static void create_degree_test_graph(cypher_executor *executor)
{
    cypher_result *result;

    /* Create a graph:
     *
     *   A --> B --> C
     *   |     ^
     *   v     |
     *   D ----+
     *
     * Degrees:
     *   A: in=0, out=2, total=2  (A->B, A->D)
     *   B: in=2, out=1, total=3  (A->B, D->B, B->C)
     *   C: in=1, out=0, total=1  (B->C)
     *   D: in=1, out=1, total=2  (A->D, D->B)
     */
    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"A\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"B\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"C\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Node {id: \"D\"})");
    if (result) cypher_result_free(result);

    /* Create edges */
    result = cypher_executor_execute(executor,
        "MATCH (a:Node {id: \"A\"}), (b:Node {id: \"B\"}) CREATE (a)-[:CONNECTS]->(b)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (a:Node {id: \"A\"}), (d:Node {id: \"D\"}) CREATE (a)-[:CONNECTS]->(d)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (b:Node {id: \"B\"}), (c:Node {id: \"C\"}) CREATE (b)-[:CONNECTS]->(c)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (d:Node {id: \"D\"}), (b:Node {id: \"B\"}) CREATE (d)-[:CONNECTS]->(b)");
    if (result) cypher_result_free(result);
}

/* Setup function - create executor with test data */
static int setup_degree_suite(void)
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
    create_degree_test_graph(shared_executor);

    return 0;
}

/* Teardown function */
static int teardown_degree_suite(void)
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

/* Helper to check if result contains degree centrality data */
static int result_has_degree_data(cypher_result *result)
{
    if (!result || !result->success) return 0;
    if (result->row_count < 1) return 0;
    if (result->column_count < 1) return 0;
    if (!result->data) return 0;

    /* Check that we got a JSON array result */
    const char *value = result->data[0][0];
    if (!value) return 0;

    /* Should start with [ and contain degree fields */
    return (value[0] == '[' &&
            strstr(value, "node_id") != NULL &&
            strstr(value, "in_degree") != NULL &&
            strstr(value, "out_degree") != NULL &&
            strstr(value, "degree") != NULL);
}

/* Test basic degreeCentrality() function */
static void test_degree_centrality_basic(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN degreeCentrality()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("Degree centrality error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_degree_data(result));

        /* Should have 4 nodes in result */
        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            /* Count occurrences of "node_id" */
            int count = 0;
            const char *p = json;
            while ((p = strstr(p, "node_id")) != NULL) {
                count++;
                p++;
            }
            CU_ASSERT_EQUAL(count, 4);
        }

        cypher_result_free(result);
    }
}

/* Helper to extract degree values for a node by user_id */
static int extract_degrees(const char *json, const char *user_id,
                          int *in_degree, int *out_degree, int *degree)
{
    char search[64];
    snprintf(search, sizeof(search), "\"user_id\":\"%s\"", user_id);
    const char *node_start = strstr(json, search);
    if (!node_start) return 0;

    /* Find in_degree */
    const char *in_ptr = strstr(node_start, "\"in_degree\":");
    if (in_ptr && (in_ptr - node_start) < 100) {
        sscanf(in_ptr, "\"in_degree\":%d", in_degree);
    }

    /* Find out_degree */
    const char *out_ptr = strstr(node_start, "\"out_degree\":");
    if (out_ptr && (out_ptr - node_start) < 100) {
        sscanf(out_ptr, "\"out_degree\":%d", out_degree);
    }

    /* Find degree */
    const char *deg_ptr = strstr(node_start, "\"degree\":");
    if (deg_ptr && (deg_ptr - node_start) < 100) {
        sscanf(deg_ptr, "\"degree\":%d", degree);
    }

    return 1;
}

/* Test degree centrality correctness */
static void test_degree_centrality_correctness(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN degreeCentrality()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            int in_deg, out_deg, deg;

            /* Check node A: in=0, out=2, total=2 */
            if (extract_degrees(json, "A", &in_deg, &out_deg, &deg)) {
                CU_ASSERT_EQUAL(in_deg, 0);
                CU_ASSERT_EQUAL(out_deg, 2);
                CU_ASSERT_EQUAL(deg, 2);
            }

            /* Check node B: in=2, out=1, total=3 */
            if (extract_degrees(json, "B", &in_deg, &out_deg, &deg)) {
                CU_ASSERT_EQUAL(in_deg, 2);
                CU_ASSERT_EQUAL(out_deg, 1);
                CU_ASSERT_EQUAL(deg, 3);
            }

            /* Check node C: in=1, out=0, total=1 */
            if (extract_degrees(json, "C", &in_deg, &out_deg, &deg)) {
                CU_ASSERT_EQUAL(in_deg, 1);
                CU_ASSERT_EQUAL(out_deg, 0);
                CU_ASSERT_EQUAL(deg, 1);
            }

            /* Check node D: in=1, out=1, total=2 */
            if (extract_degrees(json, "D", &in_deg, &out_deg, &deg)) {
                CU_ASSERT_EQUAL(in_deg, 1);
                CU_ASSERT_EQUAL(out_deg, 1);
                CU_ASSERT_EQUAL(deg, 2);
            }
        }

        cypher_result_free(result);
    }
}

/* Test degree centrality on empty graph */
static void test_degree_centrality_empty_graph(void)
{
    /* Create a fresh executor with empty graph */
    sqlite3 *empty_db;
    int rc = sqlite3_open(":memory:", &empty_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    cypher_executor *empty_executor = cypher_executor_create(empty_db);
    CU_ASSERT_PTR_NOT_NULL(empty_executor);

    if (empty_executor) {
        cypher_result *result = cypher_executor_execute(empty_executor,
            "RETURN degreeCentrality()");

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            /* Empty graph should return empty JSON array */
            if (result->success && result->row_count > 0 && result->data) {
                const char *json = result->data[0][0];
                CU_ASSERT_STRING_EQUAL(json, "[]");
            }
            cypher_result_free(result);
        }

        cypher_executor_free(empty_executor);
    }

    sqlite3_close(empty_db);
}

/* Test degree centrality includes user_id field */
static void test_degree_centrality_user_id(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN degreeCentrality()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];

            /* Should contain user_id fields for all nodes */
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"A\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"B\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"C\""));
            CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"D\""));
        }

        cypher_result_free(result);
    }
}

/* Test degree centrality with isolated node */
static void test_degree_centrality_isolated_node(void)
{
    /* Create a fresh executor */
    sqlite3 *isolated_db;
    int rc = sqlite3_open(":memory:", &isolated_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    cypher_executor *isolated_executor = cypher_executor_create(isolated_db);
    CU_ASSERT_PTR_NOT_NULL(isolated_executor);

    if (isolated_executor) {
        cypher_result *result;

        /* Create nodes but no edges */
        result = cypher_executor_execute(isolated_executor, "CREATE (:Node {id: \"X\"})");
        if (result) cypher_result_free(result);

        result = cypher_executor_execute(isolated_executor, "CREATE (:Node {id: \"Y\"})");
        if (result) cypher_result_free(result);

        result = cypher_executor_execute(isolated_executor,
            "RETURN degreeCentrality()");

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);

            if (result->success && result->row_count > 0 && result->data) {
                const char *json = result->data[0][0];

                /* All degrees should be 0 */
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"in_degree\":0"));
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"out_degree\":0"));
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"degree\":0"));
            }
            cypher_result_free(result);
        }

        cypher_executor_free(isolated_executor);
    }

    sqlite3_close(isolated_db);
}

/* Test degree centrality with self-loop */
static void test_degree_centrality_self_loop(void)
{
    /* Create a fresh executor */
    sqlite3 *loop_db;
    int rc = sqlite3_open(":memory:", &loop_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    cypher_executor *loop_executor = cypher_executor_create(loop_db);
    CU_ASSERT_PTR_NOT_NULL(loop_executor);

    if (loop_executor) {
        cypher_result *result;

        /* Create node with self-loop */
        result = cypher_executor_execute(loop_executor, "CREATE (:Node {id: \"Self\"})");
        if (result) cypher_result_free(result);

        result = cypher_executor_execute(loop_executor,
            "MATCH (n:Node {id: \"Self\"}) CREATE (n)-[:LOOPS]->(n)");
        if (result) cypher_result_free(result);

        result = cypher_executor_execute(loop_executor,
            "RETURN degreeCentrality()");

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);

            if (result->success && result->row_count > 0 && result->data) {
                const char *json = result->data[0][0];

                /* Self-loop counts as both in and out degree */
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"in_degree\":1"));
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"out_degree\":1"));
                CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"degree\":2"));
            }
            cypher_result_free(result);
        }

        cypher_executor_free(loop_executor);
    }

    sqlite3_close(loop_db);
}

/* Initialize the Degree Centrality executor test suite */
int init_executor_degree_centrality_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Degree Centrality", setup_degree_suite, teardown_degree_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "Degree centrality basic", test_degree_centrality_basic) ||
        !CU_add_test(suite, "Degree centrality correctness", test_degree_centrality_correctness) ||
        !CU_add_test(suite, "Degree centrality empty graph", test_degree_centrality_empty_graph) ||
        !CU_add_test(suite, "Degree centrality user_id field", test_degree_centrality_user_id) ||
        !CU_add_test(suite, "Degree centrality isolated node", test_degree_centrality_isolated_node) ||
        !CU_add_test(suite, "Degree centrality self-loop", test_degree_centrality_self_loop)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
