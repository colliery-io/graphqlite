/*
 * test_executor_apsp.c
 *
 * Unit tests for All Pairs Shortest Path algorithm
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
 * All Pairs Shortest Path Tests
 * =============================================================================
 */

static void test_apsp_empty_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN apsp()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_apsp_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN apsp()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Single node has no pairs */
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_apsp_simple_chain(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Chain: a -> b -> c */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");

    char *json = exec_get_json("RETURN apsp()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* a->b: 1, a->c: 2, b->c: 1 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"target\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"target\":\"c\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":1"));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":2"));
        free(json);
    }
}

static void test_apsp_triangle(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Triangle: a -> b, b -> c, a -> c */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");

    char *json = exec_get_json("RETURN apsp()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* a->c should be 1 (direct), not 2 (via b) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"target\":\"c\""));
        /* All direct edges should have distance 1 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":1"));
        free(json);
    }
}

static void test_apsp_disconnected(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Two disconnected components */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (c {id: 'c'}), (d {id: 'd'}) CREATE (c)-[:L]->(d)");

    char *json = exec_get_json("RETURN apsp()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Should only include reachable pairs: a->b and c->d */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"target\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"c\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"target\":\"d\""));
        /* No path from a to c or d */
        CU_ASSERT_PTR_NULL(strstr(json, "\"source\":\"a\",\"target\":\"c\""));
        CU_ASSERT_PTR_NULL(strstr(json, "\"source\":\"a\",\"target\":\"d\""));
        free(json);
    }
}

static void test_apsp_alias(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");

    /* Test allPairsShortestPath alias */
    char *json = exec_get_json("RETURN allPairsShortestPath()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"target\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"distance\":1"));
        free(json);
    }
}

static void test_apsp_cycle(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Cycle: a -> b -> c -> a */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (a {id: 'a'}) CREATE (c)-[:L]->(a)");

    char *json = exec_get_json("RETURN apsp()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes should be reachable from all other nodes */
        /* a->b: 1, a->c: 2, b->c: 1, b->a: 2, c->a: 1, c->b: 2 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"source\":\"c\""));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_apsp_suite(void)
{
    CU_pSuite suite = CU_add_suite("All Pairs Shortest Path", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "APSP empty graph", test_apsp_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "APSP single node", test_apsp_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "APSP simple chain", test_apsp_simple_chain)) return CU_get_error();
    if (!CU_add_test(suite, "APSP triangle", test_apsp_triangle)) return CU_get_error();
    if (!CU_add_test(suite, "APSP disconnected", test_apsp_disconnected)) return CU_get_error();
    if (!CU_add_test(suite, "APSP alias", test_apsp_alias)) return CU_get_error();
    if (!CU_add_test(suite, "APSP cycle", test_apsp_cycle)) return CU_get_error();

    return CUE_SUCCESS;
}
