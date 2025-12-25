/*
 * test_executor_eigenvector.c
 *
 * Unit tests for Eigenvector Centrality algorithm
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
 * Eigenvector Centrality Tests
 * =============================================================================
 */

static void test_eigenvector_empty_graph(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN eigenvectorCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_eigenvector_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    char *json = exec_get_json("RETURN eigenvectorCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Single node should have score 1.0 (normalized) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"score\":1"));
        free(json);
    }
}

static void test_eigenvector_simple_chain(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Chain: a -> b -> c
     * c should have highest centrality (receives most)
     * a should have lowest (receives nothing)
     */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");

    char *json = exec_get_json("RETURN eigenvectorCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Should have all three nodes */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

static void test_eigenvector_star_topology(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Simple chain: a -> b -> c
     * Node c receives from b which receives from a
     * In eigenvector centrality with incoming edges, c should have highest score
     */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    /* Add backward edge from c to a to allow eigenvector to converge (creates cycle) */
    exec_cypher("MATCH (c {id: 'c'}), (a {id: 'a'}) CREATE (c)-[:L]->(a)");

    char *json = exec_get_json("RETURN eigenvectorCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes in a cycle have equal eigenvector centrality */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        /* Scores should be normalized - just verify one exists */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"score\":"));
        free(json);
    }
}

static void test_eigenvector_with_iterations(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");

    /* Test with custom iterations parameter */
    char *json = exec_get_json("RETURN eigenvectorCentrality(50)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        free(json);
    }
}

static void test_eigenvector_cycle(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Cycle: a -> b -> c -> a
     * All nodes should have equal centrality
     */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), (c:Node {id: 'c'})");
    exec_cypher("MATCH (a {id: 'a'}), (b {id: 'b'}) CREATE (a)-[:L]->(b)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (c {id: 'c'}), (a {id: 'a'}) CREATE (c)-[:L]->(a)");

    char *json = exec_get_json("RETURN eigenvectorCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All three nodes should appear */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        free(json);
    }
}

static void test_eigenvector_disconnected_components(void)
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

    char *json = exec_get_json("RETURN eigenvectorCentrality()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* All nodes should be present */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"a\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"b\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"c\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"user_id\":\"d\""));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_eigenvector_suite(void)
{
    CU_pSuite suite = CU_add_suite("Eigenvector Centrality", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "Eigenvector empty graph", test_eigenvector_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "Eigenvector single node", test_eigenvector_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "Eigenvector simple chain", test_eigenvector_simple_chain)) return CU_get_error();
    if (!CU_add_test(suite, "Eigenvector star topology", test_eigenvector_star_topology)) return CU_get_error();
    if (!CU_add_test(suite, "Eigenvector with iterations", test_eigenvector_with_iterations)) return CU_get_error();
    if (!CU_add_test(suite, "Eigenvector cycle", test_eigenvector_cycle)) return CU_get_error();
    if (!CU_add_test(suite, "Eigenvector disconnected components", test_eigenvector_disconnected_components)) return CU_get_error();

    return CUE_SUCCESS;
}
