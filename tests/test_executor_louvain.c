/*
 * test_executor_louvain.c
 *
 * Unit tests for Louvain community detection algorithm
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
 * Louvain Community Detection Tests
 * =============================================================================
 */

static void test_louvain_empty_graph(void)
{
    /* Fresh DB */
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_louvain_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'solo'})");

    char *json = exec_get_json("RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Single node should be in its own community */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"solo\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"community\":"));
        free(json);
    }
}

static void test_louvain_disconnected(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create disconnected nodes - each should be its own community */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");

    char *json = exec_get_json("RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

static void test_louvain_connected_pair(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Two connected nodes should be in same community */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");

    char *json = exec_get_json("RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        free(json);
    }
}

static void test_louvain_with_resolution(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");

    /* Test with resolution parameter */
    char *json = exec_get_json("RETURN louvain(1.5)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        free(json);
    }
}

static void test_louvain_triangle(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create a triangle - should be one community */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (a {id: 'a'}) CREATE (c)-[:L]->(a)");

    char *json = exec_get_json("RETURN louvain()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_louvain_suite(void)
{
    CU_pSuite suite = CU_add_suite("Louvain Community Detection", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "Empty graph", test_louvain_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "Single node", test_louvain_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "Disconnected nodes", test_louvain_disconnected)) return CU_get_error();
    if (!CU_add_test(suite, "Connected pair", test_louvain_connected_pair)) return CU_get_error();
    if (!CU_add_test(suite, "With resolution param", test_louvain_with_resolution)) return CU_get_error();
    if (!CU_add_test(suite, "Triangle", test_louvain_triangle)) return CU_get_error();

    return CUE_SUCCESS;
}
