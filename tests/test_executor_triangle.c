/*
 * test_executor_triangle.c
 *
 * Unit tests for Triangle Count algorithm
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
 * Triangle Count Tests
 * =============================================================================
 */

static void test_triangle_empty_graph(void)
{
    /* Fresh DB */
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN triangleCount()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_triangle_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'solo'})");

    char *json = exec_get_json("RETURN triangleCount()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Single node should have 0 triangles */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"solo\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"triangles\":0"));
        free(json);
    }
}

static void test_triangle_pair(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Two connected nodes - no triangle */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:LINK]->(b)");

    char *json = exec_get_json("RETURN triangleCount()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"triangles\":0"));
        free(json);
    }
}

static void test_triangle_single_triangle(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create a triangle: a-b-c-a */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (a {id: 'a'}) CREATE (c)-[:L]->(a)");

    char *json = exec_get_json("RETURN triangleCount()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Each node participates in 1 triangle */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"triangles\":1"));
        /* Clustering coefficient should be 1.0 for all nodes in a triangle */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"clustering_coefficient\":1.0"));
        free(json);
    }
}

static void test_triangle_alias(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (n:Node {id: 'test'})");

    /* Test triangles() alias */
    char *json = exec_get_json("RETURN triangles()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"test\""));
        free(json);
    }
}

static void test_triangle_star_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Star graph: center connected to 4 spokes, no triangles */
    exec_cypher("CREATE (c:Node {id: 'center'})");
    exec_cypher("CREATE (s1:Node {id: 's1'}), (s2:Node {id: 's2'}), (s3:Node {id: 's3'}), (s4:Node {id: 's4'})");
    exec_cypher("MATCH (c {id: 'center'}), (s1 {id: 's1'}) CREATE (c)-[:R]->(s1)");
    exec_cypher("MATCH (c {id: 'center'}), (s2 {id: 's2'}) CREATE (c)-[:R]->(s2)");
    exec_cypher("MATCH (c {id: 'center'}), (s3 {id: 's3'}) CREATE (c)-[:R]->(s3)");
    exec_cypher("MATCH (c {id: 'center'}), (s4 {id: 's4'}) CREATE (c)-[:R]->(s4)");

    char *json = exec_get_json("RETURN triangleCount()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* No triangles in a star graph */
        /* Center has degree 4, clustering should be 0 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"triangles\":0"));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_triangle_suite(void)
{
    CU_pSuite suite = CU_add_suite("Triangle Count", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "Empty graph", test_triangle_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "Single node", test_triangle_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "Node pair (no triangle)", test_triangle_pair)) return CU_get_error();
    if (!CU_add_test(suite, "Single triangle", test_triangle_single_triangle)) return CU_get_error();
    if (!CU_add_test(suite, "triangles() alias", test_triangle_alias)) return CU_get_error();
    if (!CU_add_test(suite, "Star graph (no triangles)", test_triangle_star_graph)) return CU_get_error();

    return CUE_SUCCESS;
}
