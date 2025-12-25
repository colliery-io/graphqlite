/*
 * test_executor_components.c
 *
 * Unit tests for WCC (Weakly Connected Components) and SCC (Strongly Connected Components)
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
 * WCC Tests
 * =============================================================================
 */

static void test_wcc_empty_graph(void)
{
    /* Fresh DB */
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN wcc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_wcc_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'solo'})");

    char *json = exec_get_json("RETURN wcc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"solo\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":0"));
        free(json);
    }
}

static void test_wcc_connected_chain(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create chain: a -> b -> c */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:LINK]->(c)");

    char *json = exec_get_json("RETURN wcc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes should be in the same component */
        int count_comp0 = 0;
        char *p = json;
        while ((p = strstr(p, "\"component\":0")) != NULL) {
            count_comp0++;
            p++;
        }
        CU_ASSERT_EQUAL(count_comp0, 3);
        free(json);
    }
}

static void test_wcc_multiple_components(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create two disconnected components: a-b and c-d */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("CREATE (c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");
    exec_cypher("MATCH (c {id: 'c'}), (d {id: 'd'}) CREATE (c)-[:LINK]->(d)");

    char *json = exec_get_json("RETURN wcc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Should have two components (0 and 1) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":0"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":1"));
        free(json);
    }
}

static void test_wcc_alias_connectedComponents(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'test'})");

    char *json = exec_get_json("RETURN connectedComponents()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"test\""));
        free(json);
    }
}

/* =============================================================================
 * SCC Tests
 * =============================================================================
 */

static void test_scc_empty_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN scc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_scc_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'solo'})");

    char *json = exec_get_json("RETURN scc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"solo\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":0"));
        free(json);
    }
}

static void test_scc_directed_chain(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create directed chain: a -> b -> c (no cycles) */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:LINK]->(c)");

    char *json = exec_get_json("RETURN scc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Each node should be in its own SCC (no back edges) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":0"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":1"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"component\":2"));
        free(json);
    }
}

static void test_scc_cycle(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create cycle: a -> b -> c -> a */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:LINK]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (a {id: 'a'}) CREATE (c)-[:LINK]->(a)");

    char *json = exec_get_json("RETURN scc()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes should be in the same SCC (it's a cycle) */
        int count_comp0 = 0;
        char *p = json;
        while ((p = strstr(p, "\"component\":0")) != NULL) {
            count_comp0++;
            p++;
        }
        CU_ASSERT_EQUAL(count_comp0, 3);
        free(json);
    }
}

static void test_scc_alias_stronglyConnectedComponents(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'test'})");

    char *json = exec_get_json("RETURN stronglyConnectedComponents()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"test\""));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

CU_TestInfo components_tests[] = {
    /* WCC tests */
    {"WCC empty graph", test_wcc_empty_graph},
    {"WCC single node", test_wcc_single_node},
    {"WCC connected chain", test_wcc_connected_chain},
    {"WCC multiple components", test_wcc_multiple_components},
    {"WCC alias connectedComponents", test_wcc_alias_connectedComponents},
    /* SCC tests */
    {"SCC empty graph", test_scc_empty_graph},
    {"SCC single node", test_scc_single_node},
    {"SCC directed chain", test_scc_directed_chain},
    {"SCC cycle", test_scc_cycle},
    {"SCC alias stronglyConnectedComponents", test_scc_alias_stronglyConnectedComponents},
    CU_TEST_INFO_NULL
};

CU_SuiteInfo components_suite = {
    "Connected Components Tests",
    suite_init,
    suite_cleanup,
    NULL,
    NULL,
    components_tests
};

/* Initialize function for test runner */
int init_executor_components_suite(void)
{
    CU_pSuite suite = CU_add_suite("Connected Components", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "WCC empty graph", test_wcc_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "WCC single node", test_wcc_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "WCC connected chain", test_wcc_connected_chain)) return CU_get_error();
    if (!CU_add_test(suite, "WCC multiple components", test_wcc_multiple_components)) return CU_get_error();
    if (!CU_add_test(suite, "WCC alias connectedComponents", test_wcc_alias_connectedComponents)) return CU_get_error();
    if (!CU_add_test(suite, "SCC empty graph", test_scc_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "SCC single node", test_scc_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "SCC directed chain", test_scc_directed_chain)) return CU_get_error();
    if (!CU_add_test(suite, "SCC cycle", test_scc_cycle)) return CU_get_error();
    if (!CU_add_test(suite, "SCC alias stronglyConnectedComponents", test_scc_alias_stronglyConnectedComponents)) return CU_get_error();

    return CUE_SUCCESS;
}
