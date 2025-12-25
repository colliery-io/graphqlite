/*
 * test_executor_betweenness.c
 *
 * Unit tests for Betweenness Centrality algorithm
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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

/* Helper to check if a score is approximately equal */
static int approx_equal(double a, double b, double tolerance)
{
    return fabs(a - b) < tolerance;
}

/* =============================================================================
 * Betweenness Centrality Tests
 * =============================================================================
 */

static void test_betweenness_empty_graph(void)
{
    /* Fresh DB */
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN betweennessCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_betweenness_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'solo'})");

    char *json = exec_get_json("RETURN betweennessCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Single node should have betweenness 0 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"solo\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"score\":0.0"));
        free(json);
    }
}

static void test_betweenness_chain(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create chain: a -> b -> c */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:LINK]->(c)");

    char *json = exec_get_json("RETURN betweennessCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* b is on the only shortest path from a to c */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        /* a and c should have score 0, b should have score > 0 */
        free(json);
    }
}

static void test_betweenness_diamond(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /*
     * Create diamond graph:
     *       a
     *      / \
     *     b   d
     *      \ /
     *       c
     */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:LINK]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (d {id: 'd'}) CREATE (a)-[:LINK]->(d)");
    exec_cypher("MATCH (d {id: 'd'}), (c {id: 'c'}) CREATE (d)-[:LINK]->(c)");

    char *json = exec_get_json("RETURN betweennessCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* b and d should have equal scores (both on half the shortest paths a->c) */
        /* a and c should have score 0 (endpoints) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"d\""));
        free(json);
    }
}

static void test_betweenness_star(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /*
     * Create star graph (hub and spokes):
     * s1, s2, s3, s4 all connect to hub
     */
    exec_cypher("CREATE (h:Node {id: 'hub'})");
    exec_cypher("CREATE (s1:Node {id: 's1'}), (s2:Node {id: 's2'}), (s3:Node {id: 's3'}), (s4:Node {id: 's4'})");
    exec_cypher("MATCH (s1 {id: 's1'}), (h {id: 'hub'}) CREATE (s1)-[:LINK]->(h)");
    exec_cypher("MATCH (s2 {id: 's2'}), (h {id: 'hub'}) CREATE (s2)-[:LINK]->(h)");
    exec_cypher("MATCH (s3 {id: 's3'}), (h {id: 'hub'}) CREATE (s3)-[:LINK]->(h)");
    exec_cypher("MATCH (s4 {id: 's4'}), (h {id: 'hub'}) CREATE (s4)-[:LINK]->(h)");

    char *json = exec_get_json("RETURN betweennessCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Hub should have high betweenness (all paths go through it) */
        /* Spokes should have 0 betweenness (no paths go through them) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"hub\""));
        free(json);
    }
}

static void test_betweenness_alias(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'test'})");

    /* Test betweenness() alias */
    char *json = exec_get_json("RETURN betweenness()");
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

/* Initialize function for test runner */
int init_executor_betweenness_suite(void)
{
    CU_pSuite suite = CU_add_suite("Betweenness Centrality", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "Empty graph", test_betweenness_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "Single node", test_betweenness_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "Chain graph", test_betweenness_chain)) return CU_get_error();
    if (!CU_add_test(suite, "Diamond graph", test_betweenness_diamond)) return CU_get_error();
    if (!CU_add_test(suite, "Star graph", test_betweenness_star)) return CU_get_error();
    if (!CU_add_test(suite, "Alias betweenness()", test_betweenness_alias)) return CU_get_error();

    return CUE_SUCCESS;
}
