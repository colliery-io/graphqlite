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
#include "parser/cypher_debug.h"

/* Shared executor for all tests in this suite */
static cypher_executor *shared_executor = NULL;
static sqlite3 *test_db = NULL;

/* Helper function to create test graph */
static void create_pagerank_test_graph(cypher_executor *executor)
{
    cypher_result *result;

    /* Create a web-like graph:
     *   A -> B, A -> C
     *   B -> C
     *   C -> A
     *   D -> C (D is dangling - only outgoing)
     *
     * Expected PageRank order: C > A > B > D
     */
    result = cypher_executor_execute(executor, "CREATE (:Page {name: \"A\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Page {name: \"B\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Page {name: \"C\"})");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor, "CREATE (:Page {name: \"D\"})");
    if (result) cypher_result_free(result);

    /* Create edges */
    result = cypher_executor_execute(executor,
        "MATCH (a:Page {name: \"A\"}), (b:Page {name: \"B\"}) CREATE (a)-[:LINKS]->(b)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (a:Page {name: \"A\"}), (c:Page {name: \"C\"}) CREATE (a)-[:LINKS]->(c)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (b:Page {name: \"B\"}), (c:Page {name: \"C\"}) CREATE (b)-[:LINKS]->(c)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (c:Page {name: \"C\"}), (a:Page {name: \"A\"}) CREATE (c)-[:LINKS]->(a)");
    if (result) cypher_result_free(result);

    result = cypher_executor_execute(executor,
        "MATCH (d:Page {name: \"D\"}), (c:Page {name: \"C\"}) CREATE (d)-[:LINKS]->(c)");
    if (result) cypher_result_free(result);
}

/* Setup function - create executor with test data */
static int setup_pagerank_suite(void)
{
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }

    shared_executor = cypher_executor_create(test_db);
    if (!shared_executor) {
        sqlite3_close(test_db);
        return -1;
    }

    /* Create test graph */
    create_pagerank_test_graph(shared_executor);

    return 0;
}

/* Teardown function */
static int teardown_pagerank_suite(void)
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

/* Helper to check if result contains JSON with expected structure */
static int result_has_pagerank_data(cypher_result *result)
{
    if (!result || !result->success) return 0;
    if (result->row_count < 1) return 0;
    if (result->column_count < 1) return 0;
    if (!result->data) return 0;

    /* Check that we got a JSON array result */
    const char *value = result->data[0][0];
    if (!value) return 0;

    /* Should start with [ and contain node_id and score */
    return (value[0] == '[' &&
            strstr(value, "node_id") != NULL &&
            strstr(value, "score") != NULL);
}

/* Test basic pageRank() function */
static void test_pagerank_basic(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN pageRank()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("PageRank error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_pagerank_data(result));

        /* Should have 4 nodes in result */
        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            /* Count occurrences of "node_id" */
            int count = 0;
            const char *p = json;
            while ((p = strstr(p, "node_id")) != NULL) {
                count++;
                p++;
            }
            CU_ASSERT_EQUAL(count, 4);
        }

        cypher_result_free(result);
    }
}

/* Test pageRank with custom damping factor */
static void test_pagerank_custom_damping(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN pageRank(0.5)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result_has_pagerank_data(result));
        cypher_result_free(result);
    }
}

/* Test pageRank with custom iterations */
static void test_pagerank_custom_iterations(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN pageRank(0.85, 5)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result_has_pagerank_data(result));
        cypher_result_free(result);
    }
}

/* Test topPageRank(k) function */
static void test_top_pagerank(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN topPageRank(2)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("topPageRank error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_pagerank_data(result));

        /* Should have exactly 2 nodes in result */
        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            int count = 0;
            const char *p = json;
            while ((p = strstr(p, "node_id")) != NULL) {
                count++;
                p++;
            }
            CU_ASSERT_EQUAL(count, 2);
        }

        cypher_result_free(result);
    }
}

/* Test personalizedPageRank with single seed */
static void test_personalized_pagerank_single_seed(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Use node ID 4 (D) as seed */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN personalizedPageRank(\"[4]\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("personalizedPageRank error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_pagerank_data(result));
        cypher_result_free(result);
    }
}

/* Test personalizedPageRank with multiple seeds */
static void test_personalized_pagerank_multiple_seeds(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Use nodes 1 (A) and 4 (D) as seeds */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN personalizedPageRank(\"[1,4]\")");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result_has_pagerank_data(result));
        cypher_result_free(result);
    }
}

/* Test personalizedPageRank with custom parameters */
static void test_personalized_pagerank_custom_params(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN personalizedPageRank(\"[1]\", 0.9, 10)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result_has_pagerank_data(result));
        cypher_result_free(result);
    }
}

/* Test pageRank on empty graph */
static void test_pagerank_empty_graph(void)
{
    /* Create a fresh executor with empty graph */
    sqlite3 *empty_db;
    int rc = sqlite3_open(":memory:", &empty_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    cypher_executor *empty_executor = cypher_executor_create(empty_db);
    CU_ASSERT_PTR_NOT_NULL(empty_executor);

    if (empty_executor) {
        cypher_result *result = cypher_executor_execute(empty_executor,
            "RETURN pageRank()");

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            /* Empty graph should return empty JSON array */
            if (result->success && result->row_count > 0 && result->data) {
                const char *json = result->data[0][0];
                CU_ASSERT_STRING_EQUAL(json, "[]");
            }
            cypher_result_free(result);
        }

        cypher_executor_free(empty_executor);
    }

    sqlite3_close(empty_db);
}

/* Helper to extract score for a given node_id from JSON result */
static double extract_score_for_node(const char *json, int target_node_id)
{
    const char *p = json;
    while ((p = strstr(p, "node_id")) != NULL) {
        int node_id = 0;
        double score = 0.0;
        /* Try to parse node_id and score */
        if (sscanf(p, "node_id\":%d,\"score\":%lf", &node_id, &score) == 2 ||
            sscanf(p, "node_id\": %d, \"score\": %lf", &node_id, &score) == 2 ||
            sscanf(p, "node_id\":%d,\"score\": %lf", &node_id, &score) == 2) {
            if (node_id == target_node_id) {
                return score;
            }
        }
        p++;
    }
    return -1.0;  /* Not found */
}

/* Test pageRank correctness - verify scores and ranking */
static void test_pagerank_correctness(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Run with enough iterations for convergence */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN pageRank(0.85, 50)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];

            /* Extract scores for each node:
             * A=1, B=2, C=3, D=4 based on creation order */
            double score_a = extract_score_for_node(json, 1);
            double score_b = extract_score_for_node(json, 2);
            double score_c = extract_score_for_node(json, 3);
            double score_d = extract_score_for_node(json, 4);

            /* Verify all scores were found */
            CU_ASSERT_TRUE(score_a >= 0);
            CU_ASSERT_TRUE(score_b >= 0);
            CU_ASSERT_TRUE(score_c >= 0);
            CU_ASSERT_TRUE(score_d >= 0);

            /* Verify scores sum to approximately 1.0 */
            double total = score_a + score_b + score_c + score_d;
            CU_ASSERT_DOUBLE_EQUAL(total, 1.0, 0.01);

            /* Verify ranking order: C > A > B > D
             * C receives most links (from A, B, D)
             * A receives link from high-PR node C
             * B receives link from A
             * D receives no incoming links */
            CU_ASSERT_TRUE(score_c > score_a);  /* C > A */
            CU_ASSERT_TRUE(score_a > score_b);  /* A > B */
            CU_ASSERT_TRUE(score_b > score_d);  /* B > D */

            /* D should have lowest score (only teleport, no incoming links) */
            CU_ASSERT_TRUE(score_d < 0.1);  /* D is low */
            /* C should have highest score */
            CU_ASSERT_TRUE(score_c > 0.3);  /* C is high */
        }

        cypher_result_free(result);
    }
}

/* Test pageRank ranking order verification */
static void test_pagerank_ranking_order(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN pageRank()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        /* Results should be ordered by score descending */
        /* Node C (id 3) should be first as it receives most links */
        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            /* First node_id in the array should be 3 (C) */
            const char *first_node = strstr(json, "node_id");
            CU_ASSERT_PTR_NOT_NULL(first_node);
            if (first_node) {
                /* Parse: "node_id":3 */
                int first_id = 0;
                if (sscanf(first_node, "node_id\":%d", &first_id) == 1 ||
                    sscanf(first_node, "node_id\": %d", &first_id) == 1) {
                    CU_ASSERT_EQUAL(first_id, 3);  /* Node C */
                }
            }
        }

        cypher_result_free(result);
    }
}

/* Initialize the PageRank executor test suite */
int init_executor_pagerank_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor PageRank", setup_pagerank_suite, teardown_pagerank_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "PageRank basic", test_pagerank_basic) ||
        !CU_add_test(suite, "PageRank custom damping", test_pagerank_custom_damping) ||
        !CU_add_test(suite, "PageRank custom iterations", test_pagerank_custom_iterations) ||
        !CU_add_test(suite, "topPageRank(k)", test_top_pagerank) ||
        !CU_add_test(suite, "personalizedPageRank single seed", test_personalized_pagerank_single_seed) ||
        !CU_add_test(suite, "personalizedPageRank multiple seeds", test_personalized_pagerank_multiple_seeds) ||
        !CU_add_test(suite, "personalizedPageRank custom params", test_personalized_pagerank_custom_params) ||
        !CU_add_test(suite, "PageRank empty graph", test_pagerank_empty_graph) ||
        !CU_add_test(suite, "PageRank ranking order", test_pagerank_ranking_order) ||
        !CU_add_test(suite, "PageRank correctness", test_pagerank_correctness)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
