#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"

/* Test database handle and shared executor */
static sqlite3 *test_db = NULL;
static cypher_executor *shared_executor = NULL;

/* Forward declaration */
static void create_test_graph(cypher_executor *executor);

/* Setup function - create database, schema, and test graph once */
static int setup_varlen_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    /* Initialize schema */
    cypher_schema_manager *schema_mgr = cypher_schema_create_manager(test_db);
    if (!schema_mgr) {
        return -1;
    }

    if (cypher_schema_initialize(schema_mgr) < 0) {
        cypher_schema_free_manager(schema_mgr);
        return -1;
    }

    cypher_schema_free_manager(schema_mgr);

    /* Create shared executor and test graph once */
    shared_executor = cypher_executor_create(test_db);
    if (!shared_executor) {
        return -1;
    }

    create_test_graph(shared_executor);

    return 0;
}

/* Helper to create the test graph: A -> B -> C -> D (chain) */
static void create_test_graph(cypher_executor *executor)
{
    cypher_result *result;

    /* Create nodes */
    result = cypher_executor_execute(executor, "CREATE (:Person {name: \"Alice\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Person {name: \"Bob\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Person {name: \"Charlie\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Person {name: \"Diana\"})");
    if (result) cypher_result_free(result);

    /* Create relationships: Alice -> Bob -> Charlie -> Diana */
    result = cypher_executor_execute(executor,
        "MATCH (a:Person {name: \"Alice\"}), (b:Person {name: \"Bob\"}) CREATE (a)-[:KNOWS]->(b)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (b:Person {name: \"Bob\"}), (c:Person {name: \"Charlie\"}) CREATE (b)-[:KNOWS]->(c)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (c:Person {name: \"Charlie\"}), (d:Person {name: \"Diana\"}) CREATE (c)-[:KNOWS]->(d)");
    if (result) cypher_result_free(result);
}

/* Teardown function */
static int teardown_varlen_suite(void)
{
    if (shared_executor) {
        cypher_executor_free(shared_executor);
        shared_executor = NULL;
    }
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper to count results from a query */
static int count_query_results(cypher_executor *executor, const char *query)
{
    cypher_result *result = cypher_executor_execute(executor, query);
    if (!result || !result->success) {
        if (result) {
            printf("Query error: %s\n", result->error_message);
            cypher_result_free(result);
        }
        return -1;
    }
    int count = result->row_count;
    cypher_result_free(result);
    return count;
}

/* Test exact 1-hop variable-length relationship */
static void test_varlen_exact_1_hop(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*1] from Alice should find Bob (1 hop) */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*1]->(b) RETURN b.name");
    CU_ASSERT_EQUAL(count, 1);
}

/* Test exact 2-hop variable-length relationship */
static void test_varlen_exact_2_hops(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*2] from Alice should find Charlie (2 hops) */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*2]->(c) RETURN c.name");
    CU_ASSERT_EQUAL(count, 1);
}

/* Test exact 3-hop variable-length relationship */
static void test_varlen_exact_3_hops(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*3] from Alice should find Diana (3 hops) */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*3]->(d) RETURN d.name");
    CU_ASSERT_EQUAL(count, 1);
}

/* Test range variable-length relationship [*1..2] */
static void test_varlen_range_1_to_2(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*1..2] from Alice should find Bob (1 hop) and Charlie (2 hops) */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*1..2]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 2);
}

/* Test range variable-length relationship [*1..3] */
static void test_varlen_range_1_to_3(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*1..3] from Alice should find Bob, Charlie, and Diana */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*1..3]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 3);
}

/* Test range variable-length relationship [*2..3] */
static void test_varlen_range_2_to_3(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*2..3] from Alice should find Charlie (2 hops) and Diana (3 hops) */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*2..3]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 2);
}

/* Test variable-length with no matches */
static void test_varlen_no_matches(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*4] from Alice - no node 4 hops away */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*4]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 0);

    /* [*5..10] from Alice - no nodes in that range */
    count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*5..10]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 0);
}

/* Test variable-length from different starting points */
static void test_varlen_different_start(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* From Bob, 1 hop should find Charlie */
    int count = count_query_results(shared_executor,
        "MATCH (b:Person {name: \"Bob\"})-[*1]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 1);

    /* From Bob, 2 hops should find Diana */
    count = count_query_results(shared_executor,
        "MATCH (b:Person {name: \"Bob\"})-[*2]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 1);

    /* From Charlie, 1 hop should find Diana */
    count = count_query_results(shared_executor,
        "MATCH (c:Person {name: \"Charlie\"})-[*1]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 1);

    /* From Diana, no outgoing paths */
    count = count_query_results(shared_executor,
        "MATCH (d:Person {name: \"Diana\"})-[*1]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 0);
}

/* Test variable-length with type filter */
static void test_varlen_with_type_filter(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [:KNOWS*1..3] from Alice should only follow KNOWS relationships */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[:KNOWS*1..3]->(x) RETURN x.name");
    /* Should find Bob (1), Charlie (2), Diana (3) */
    CU_ASSERT_EQUAL(count, 3);
}

/* Test cycle detection with variable-length relationships */
static void test_varlen_cycle_detection(void)
{
    /* Create a separate DB with a cycle */
    sqlite3 *cycle_db;
    int rc = sqlite3_open(":memory:", &cycle_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    if (rc == SQLITE_OK) {
        cypher_schema_manager *schema_mgr = cypher_schema_create_manager(cycle_db);
        cypher_schema_initialize(schema_mgr);
        cypher_schema_free_manager(schema_mgr);

        cypher_executor *executor = cypher_executor_create(cycle_db);
        CU_ASSERT_PTR_NOT_NULL(executor);

        if (executor) {
            /* Create a cycle: A -> B -> C -> A */
            cypher_result *result;
            result = cypher_executor_execute(executor, "CREATE (:Node {name: \"A\"})");
            if (result) cypher_result_free(result);
            result = cypher_executor_execute(executor, "CREATE (:Node {name: \"B\"})");
            if (result) cypher_result_free(result);
            result = cypher_executor_execute(executor, "CREATE (:Node {name: \"C\"})");
            if (result) cypher_result_free(result);

            result = cypher_executor_execute(executor,
                "MATCH (a:Node {name: \"A\"}), (b:Node {name: \"B\"}) CREATE (a)-[:LINK]->(b)");
            if (result) cypher_result_free(result);
            result = cypher_executor_execute(executor,
                "MATCH (b:Node {name: \"B\"}), (c:Node {name: \"C\"}) CREATE (b)-[:LINK]->(c)");
            if (result) cypher_result_free(result);
            result = cypher_executor_execute(executor,
                "MATCH (c:Node {name: \"C\"}), (a:Node {name: \"A\"}) CREATE (c)-[:LINK]->(a)");
            if (result) cypher_result_free(result);

            /* [*1..10] should NOT infinite loop - cycle detection should prevent revisiting */
            int count = count_query_results(executor,
                "MATCH (a:Node {name: \"A\"})-[*1..10]->(x) RETURN x.name");
            /* Should find B (1 hop), C (2 hops), but NOT revisit A */
            CU_ASSERT_TRUE(count >= 2);
            CU_ASSERT_TRUE(count <= 3); /* At most B, C, and maybe A once */

            cypher_executor_free(executor);
        }

        sqlite3_close(cycle_db);
    }
}

/* Test unbounded variable-length [*] */
static void test_varlen_unbounded(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*] from Alice should find all reachable nodes */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*]->(x) RETURN x.name");
    /* Should find Bob, Charlie, Diana */
    CU_ASSERT_TRUE(count >= 3);
}

/* Test min-bounded variable-length [*2..] */
static void test_varlen_min_bounded(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*2..] from Alice should skip 1-hop nodes */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*2..]->(x) RETURN x.name");
    /* Should find Charlie (2 hops) and Diana (3 hops), but not Bob */
    CU_ASSERT_TRUE(count >= 2);
}

/* Test max-bounded variable-length [*..2] */
static void test_varlen_max_bounded(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* [*..2] from Alice should find nodes up to 2 hops */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[*..2]->(x) RETURN x.name");
    /* Should find Bob (1 hop) and Charlie (2 hops), but not Diana */
    CU_ASSERT_EQUAL(count, 2);
}

/* Test variable-length with relationship variable */
static void test_varlen_with_variable(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Named relationship variable with varlen */
    int count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[r*1..2]->(x) RETURN x.name");
    CU_ASSERT_EQUAL(count, 2);
}

/* Test comparing regular vs variable-length results */
static void test_varlen_vs_regular(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Regular 1-hop should match [*1] */
    int regular_count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[:KNOWS]->(b) RETURN b.name");
    int varlen_count = count_query_results(shared_executor,
        "MATCH (a:Person {name: \"Alice\"})-[:KNOWS*1]->(b) RETURN b.name");
    CU_ASSERT_EQUAL(regular_count, varlen_count);
}

/* Initialize the variable-length executor test suite */
int init_executor_varlen_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Variable-Length", setup_varlen_suite, teardown_varlen_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "Varlen exact 1 hop", test_varlen_exact_1_hop) ||
        !CU_add_test(suite, "Varlen exact 2 hops", test_varlen_exact_2_hops) ||
        !CU_add_test(suite, "Varlen exact 3 hops", test_varlen_exact_3_hops) ||
        !CU_add_test(suite, "Varlen range [*1..2]", test_varlen_range_1_to_2) ||
        !CU_add_test(suite, "Varlen range [*1..3]", test_varlen_range_1_to_3) ||
        !CU_add_test(suite, "Varlen range [*2..3]", test_varlen_range_2_to_3) ||
        !CU_add_test(suite, "Varlen no matches", test_varlen_no_matches) ||
        !CU_add_test(suite, "Varlen different start points", test_varlen_different_start) ||
        !CU_add_test(suite, "Varlen with type filter", test_varlen_with_type_filter) ||
        !CU_add_test(suite, "Varlen cycle detection", test_varlen_cycle_detection) ||
        !CU_add_test(suite, "Varlen unbounded [*]", test_varlen_unbounded) ||
        !CU_add_test(suite, "Varlen min bounded [*2..]", test_varlen_min_bounded) ||
        !CU_add_test(suite, "Varlen max bounded [*..2]", test_varlen_max_bounded) ||
        !CU_add_test(suite, "Varlen with variable", test_varlen_with_variable) ||
        !CU_add_test(suite, "Varlen vs regular comparison", test_varlen_vs_regular)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
