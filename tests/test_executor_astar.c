/*
 * test_executor_astar.c
 *
 * Unit tests for A* shortest path algorithm
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
 * A* Tests
 * =============================================================================
 */

static void test_astar_empty_graph(void)
{
    /* Fresh DB */
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN astar('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":false"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"path\":[]"));
        free(json);
    }
}

static void test_astar_no_path(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create disconnected nodes */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");

    char *json = exec_get_json("RETURN astar('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":false"));
        free(json);
    }
}

static void test_astar_direct_path(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create simple path a -> b */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");

    char *json = exec_get_json("RETURN astar('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"b\""));
        free(json);
    }
}

static void test_astar_multi_hop(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create path a -> b -> c -> d */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (d {id: 'd'}) CREATE (c)-[:L]->(d)");

    char *json = exec_get_json("RETURN astar('a', 'd')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
        /* Path should be a, b, c, d */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"c\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"d\""));
        /* Distance should be 3 hops */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":3.0"));
        free(json);
    }
}

static void test_astar_alias(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");

    /* Test aStar alias */
    char *json = exec_get_json("RETURN aStar('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"found\":true"));
        free(json);
    }
}

static void test_astar_nodes_explored(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Simple path */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");

    char *json = exec_get_json("RETURN astar('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Should have nodes_explored field */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"nodes_explored\":"));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_astar_suite(void)
{
    CU_pSuite suite = CU_add_suite("A* Shortest Path", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "Empty graph", test_astar_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "No path exists", test_astar_no_path)) return CU_get_error();
    if (!CU_add_test(suite, "Direct path", test_astar_direct_path)) return CU_get_error();
    if (!CU_add_test(suite, "Multi-hop path", test_astar_multi_hop)) return CU_get_error();
    if (!CU_add_test(suite, "aStar() alias", test_astar_alias)) return CU_get_error();
    if (!CU_add_test(suite, "nodes_explored field", test_astar_nodes_explored)) return CU_get_error();

    return CUE_SUCCESS;
}
