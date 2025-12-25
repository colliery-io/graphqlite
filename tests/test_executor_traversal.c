/*
 * test_executor_traversal.c
 *
 * Unit tests for BFS and DFS traversal algorithms
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
 * BFS Tests
 * =============================================================================
 */

static void test_bfs_empty_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN bfs('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_bfs_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN bfs('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"depth\":0"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"order\":0"));
        free(json);
    }
}

static void test_bfs_linear_path(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a -> b -> c */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");

    char *json = exec_get_json("RETURN bfs('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes should be visited */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

static void test_bfs_max_depth(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a -> b -> c -> d */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (d {id: 'd'}) CREATE (c)-[:L]->(d)");

    /* Limit to depth 1 - should only get a and b */
    char *json = exec_get_json("RETURN bfs('a', 1)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        /* c should NOT be in result (depth 2) */
        CU_ASSERT_PTR_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

/* =============================================================================
 * DFS Tests
 * =============================================================================
 */

static void test_dfs_empty_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN dfs('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_dfs_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN dfs('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"depth\":0"));
        free(json);
    }
}

static void test_dfs_linear_path(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a -> b -> c */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");

    char *json = exec_get_json("RETURN dfs('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes should be visited */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

static void test_dfs_max_depth(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a -> b -> c -> d */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (d {id: 'd'}) CREATE (c)-[:L]->(d)");

    /* Limit to depth 1 */
    char *json = exec_get_json("RETURN dfs('a', 1)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        /* c should NOT be in result */
        CU_ASSERT_PTR_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

static void test_bfs_alias(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN breadthFirstSearch('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        free(json);
    }
}

static void test_dfs_alias(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN depthFirstSearch('a')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_traversal_suite(void)
{
    CU_pSuite suite = CU_add_suite("BFS/DFS Traversal", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    /* BFS tests */
    if (!CU_add_test(suite, "BFS empty graph", test_bfs_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "BFS single node", test_bfs_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "BFS linear path", test_bfs_linear_path)) return CU_get_error();
    if (!CU_add_test(suite, "BFS max depth", test_bfs_max_depth)) return CU_get_error();
    if (!CU_add_test(suite, "BFS alias", test_bfs_alias)) return CU_get_error();

    /* DFS tests */
    if (!CU_add_test(suite, "DFS empty graph", test_dfs_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "DFS single node", test_dfs_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "DFS linear path", test_dfs_linear_path)) return CU_get_error();
    if (!CU_add_test(suite, "DFS max depth", test_dfs_max_depth)) return CU_get_error();
    if (!CU_add_test(suite, "DFS alias", test_dfs_alias)) return CU_get_error();

    return CUE_SUCCESS;
}
