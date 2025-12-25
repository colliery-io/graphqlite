/*
 * test_executor_similarity.c
 *
 * Unit tests for Node Similarity (Jaccard) algorithm
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
 * Node Similarity Tests
 * =============================================================================
 */

static void test_similarity_empty_graph(void)
{
    /* Fresh database */
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    char *json = exec_get_json("RETURN nodeSimilarity()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_similarity_single_node(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'})");

    /* Single node - no pairs */
    char *json = exec_get_json("RETURN nodeSimilarity()");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_STRING_EQUAL(json, "[]");
        free(json);
    }
}

static void test_similarity_two_nodes_no_edges(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'})");

    /* Two nodes with no neighbors - similarity is 0 */
    char *json = exec_get_json("RETURN nodeSimilarity('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\":0."));
        free(json);
    }
}

static void test_similarity_identical_neighbors(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a and b both connect to c and d - perfect similarity */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (d {id: 'd'}) CREATE (a)-[:L]->(d)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (d {id: 'd'}) CREATE (b)-[:L]->(d)");

    char *json = exec_get_json("RETURN nodeSimilarity('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Jaccard = |{c,d} ∩ {c,d}| / |{c,d} ∪ {c,d}| = 2/2 = 1.0 */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\":1.0"));
        free(json);
    }
}

static void test_similarity_partial_overlap(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a connects to c, d; b connects to c, e
     * Intersection = {c}, Union = {c, d, e}
     * Jaccard = 1/3 ≈ 0.333 */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'}), (e:Node {id: 'e'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (d {id: 'd'}) CREATE (a)-[:L]->(d)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (e {id: 'e'}) CREATE (b)-[:L]->(e)");

    char *json = exec_get_json("RETURN nodeSimilarity('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\":0.33"));
        free(json);
    }
}

static void test_similarity_all_pairs(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create a small graph */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (d {id: 'd'}) CREATE (a)-[:L]->(d)");

    /* Get all pairs with similarity > 0 */
    char *json = exec_get_json("RETURN nodeSimilarity(0.0)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* Should include the a-b pair since they share neighbor c */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"node1\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"node2\""));
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\""));
        free(json);
    }
}

static void test_similarity_threshold(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* Create graph with varying similarities */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'}), (e:Node {id: 'e'})");
    /* a-b: high similarity (share c, d) */
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (a {id: 'a'}), (d {id: 'd'}) CREATE (a)-[:L]->(d)");
    exec_cypher("MATCH (b {id: 'b'}), (c {id: 'c'}) CREATE (b)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (d {id: 'd'}) CREATE (b)-[:L]->(d)");
    /* a-e: low similarity (only share c) */
    exec_cypher("MATCH (e {id: 'e'}), (c {id: 'c'}) CREATE (e)-[:L]->(c)");

    /* High threshold - should only get perfect matches */
    char *json = exec_get_json("RETURN nodeSimilarity(0.9)");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        /* a-b should be included (similarity = 1.0) */
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\":1.0"));
        free(json);
    }
}

static void test_similarity_no_overlap(void)
{
    cypher_executor_free(executor);
    sqlite3_close(test_db);
    sqlite3_open(":memory:", &test_db);
    executor = cypher_executor_create(test_db);

    /* a connects to c, b connects to d - no overlap */
    exec_cypher("CREATE (a:Node {id: 'a'}), (b:Node {id: 'b'}), "
                "(c:Node {id: 'c'}), (d:Node {id: 'd'})");
    exec_cypher("MATCH (a {id: 'a'}), (c {id: 'c'}) CREATE (a)-[:L]->(c)");
    exec_cypher("MATCH (b {id: 'b'}), (d {id: 'd'}) CREATE (b)-[:L]->(d)");

    /* No shared neighbors - similarity should be 0 */
    char *json = exec_get_json("RETURN nodeSimilarity('a', 'b')");
    CU_ASSERT_PTR_NOT_NULL(json);
    if (json) {
        CU_ASSERT_PTR_NOT_NULL(strstr(json, "\"similarity\":0.0"));
        free(json);
    }
}

/* =============================================================================
 * Test Suite Registration
 * =============================================================================
 */

int init_executor_similarity_suite(void)
{
    CU_pSuite suite = CU_add_suite("Node Similarity (Jaccard)", suite_init, suite_cleanup);
    if (!suite) return CU_get_error();

    if (!CU_add_test(suite, "Similarity empty graph", test_similarity_empty_graph)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity single node", test_similarity_single_node)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity two nodes no edges", test_similarity_two_nodes_no_edges)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity identical neighbors", test_similarity_identical_neighbors)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity partial overlap", test_similarity_partial_overlap)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity all pairs", test_similarity_all_pairs)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity threshold filter", test_similarity_threshold)) return CU_get_error();
    if (!CU_add_test(suite, "Similarity no overlap", test_similarity_no_overlap)) return CU_get_error();

    return CUE_SUCCESS;
}
