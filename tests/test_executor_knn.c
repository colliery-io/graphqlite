/*
 * test_executor_knn.c
 *
 * Unit tests for K-Nearest Neighbors algorithm
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <string.h>
#include <stdlib.h>
#include "executor/cypher_executor.h"
#include "executor/graph_algorithms.h"

/* Test fixture */
static sqlite3 *test_db = NULL;
static cypher_executor *executor = NULL;

/* Setup/teardown */
static int suite_init(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) return -1;

    executor = cypher_executor_create(test_db);
    if (!executor) return -1;

    return 0;
}

static int suite_cleanup(void)
{
    if (executor) {
        cypher_executor_free(executor);
        executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper to execute cypher and check success */
static int exec_cypher(const char *query)
{
    cypher_result *result = cypher_executor_execute(executor, query);
    int success = result && result->success;
    if (result) cypher_result_free(result);
    return success;
}

/* Helper to execute and get JSON result */
static char* exec_get_json(const char *query)
{
    cypher_result *result = cypher_executor_execute(executor, query);
    if (!result || !result->success || result->row_count == 0) {
        if (result) cypher_result_free(result);
        return NULL;
    }

    char *json = strdup(result->data[0][0]);
    cypher_result_free(result);
    return json;
}

/* =============================================================================
 * KNN Tests
 * =============================================================================
 */

static void test_knn_empty_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN knn('a', 5)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_knn_node_not_found(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN knn('nonexistent', 5)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_knn_single_neighbor(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a and b both connect to c - they should be similar */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");

    char *json = exec_get_json("RETURN knn('a', 5)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* b should be the nearest neighbor with similarity 1.0 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"neighbor\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\":1.0"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"rank\":1"));
        free(json);
    }
}

static void test_knn_multiple_neighbors(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create a graph where:
     * - a connects to c, d
     * - b connects to c, d (similarity 1.0 with a)
     * - e connects to c (similarity 0.5 with a)
     */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'}), (e:Node {id: 'e'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (d {id: 'd'}) CREATE (a)-[:L]->(d)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (d {id: 'd'}) CREATE (b)-[:L]->(d)");
    exec_cypher("MATCH (e {id: 'e'}), (c {id: 'c'}) CREATE (e)-[:L]->(c)");

    char *json = exec_get_json("RETURN knn('a', 5)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* b should be rank 1 (similarity 1.0), e should be rank 2 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"neighbor\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"neighbor\":\"e\""));
        free(json);
    }
}

static void test_knn_limit_k(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create graph with 3 similar nodes to a */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'}), (x:Node {id: 'x'})");
    exec_cypher("MATCH (a {id: 'a'}), (x {id: 'x'}) CREATE (a)-[:L]->(x)");
    exec_cypher("MATCH (b {id: 'b'}), (x {id: 'x'}) CREATE (b)-[:L]->(x)");
    exec_cypher("MATCH (c {id: 'c'}), (x {id: 'x'}) CREATE (c)-[:L]->(x)");
    exec_cypher("MATCH (d {id: 'd'}), (x {id: 'x'}) CREATE (d)-[:L]->(x)");

    /* Request only k=2, should only get 2 neighbors */
    char *json = exec_get_json("RETURN knn('a', 2)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Should have exactly 2 results */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"rank\":1"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"rank\":2"));
        /* Should NOT have rank 3 */
        CU_ASSERT_PTR_NULL(strstr(json, "\"rank\":3"));
        free(json);
    }
}

static void test_knn_no_similar_nodes(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create isolated nodes with no shared neighbors */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (d {id: 'd'}) CREATE (b)-[:L]->(d)");

    char *json = exec_get_json("RETURN knn('a', 5)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* No similar nodes - empty result */
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_knn_suite(void)
{
    CU_pSuite suite = CU_add_suite("K-Nearest Neighbors", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "KNN empty graph", test_knn_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "KNN node not found", test_knn_node_not_found)) return CU_get_error();
    if (!CU_add_test(suite, "KNN single neighbor", test_knn_single_neighbor)) return CU_get_error();
    if (!CU_add_test(suite, "KNN multiple neighbors", test_knn_multiple_neighbors)) return CU_get_error();
    if (!CU_add_test(suite, "KNN limit k", test_knn_limit_k)) return CU_get_error();
    if (!CU_add_test(suite, "KNN no similar nodes", test_knn_no_similar_nodes)) return CU_get_error();

    return CUE_SUCCESS;
}
