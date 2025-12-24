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

/* Helper function to create test graph with two disconnected triangles */
static void create_two_triangles_graph(cypher_executor *executor)
{
    cypher_result *result;

    /* Create first triangle: A-B-C (nodes 1,2,3) */
    result = cypher_executor_execute(executor, "CREATE (:Node {name: \"A\"})");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor, "CREATE (:Node {name: \"B\"})");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor, "CREATE (:Node {name: \"C\"})");
    if (result) cypher_result_free(result);

    /* Create second triangle: D-E-F (nodes 4,5,6) */
    result = cypher_executor_execute(executor, "CREATE (:Node {name: \"D\"})");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor, "CREATE (:Node {name: \"E\"})");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor, "CREATE (:Node {name: \"F\"})");
    if (result) cypher_result_free(result);

    /* Connect first triangle */
    result = cypher_executor_execute(executor,
        "MATCH (a:Node {name: \"A\"}), (b:Node {name: \"B\"}) CREATE (a)-[:KNOWS]->(b)");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor,
        "MATCH (b:Node {name: \"B\"}), (c:Node {name: \"C\"}) CREATE (b)-[:KNOWS]->(c)");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor,
        "MATCH (c:Node {name: \"C\"}), (a:Node {name: \"A\"}) CREATE (c)-[:KNOWS]->(a)");
    if (result) cypher_result_free(result);

    /* Connect second triangle */
    result = cypher_executor_execute(executor,
        "MATCH (d:Node {name: \"D\"}), (e:Node {name: \"E\"}) CREATE (d)-[:KNOWS]->(e)");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor,
        "MATCH (e:Node {name: \"E\"}), (f:Node {name: \"F\"}) CREATE (e)-[:KNOWS]->(f)");
    if (result) cypher_result_free(result);
    result = cypher_executor_execute(executor,
        "MATCH (f:Node {name: \"F\"}), (d:Node {name: \"D\"}) CREATE (f)-[:KNOWS]->(d)");
    if (result) cypher_result_free(result);
}

/* Setup function */
static int setup_label_propagation_suite(void)
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

    create_two_triangles_graph(shared_executor);
    return 0;
}

/* Teardown function */
static int teardown_label_propagation_suite(void)
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

/* Helper to check if result has community data */
static int result_has_community_data(cypher_result *result)
{
    if (!result || !result->success) return 0;
    if (result->row_count < 1) return 0;
    if (result->column_count < 1) return 0;
    if (!result->data) return 0;

    const char *value = result->data[0][0];
    if (!value) return 0;

    return (value[0] == '[' &&
            strstr(value, "node_id") != NULL &&
            strstr(value, "community") != NULL);
}

/* Test basic labelPropagation() function */
static void test_label_propagation_basic(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN labelPropagation()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success) {
            printf("labelPropagation error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_community_data(result));

        /* Should have 6 nodes in result */
        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];
            int count = 0;
            const char *p = json;
            while ((p = strstr(p, "node_id")) != NULL) {
                count++;
                p++;
            }
            CU_ASSERT_EQUAL(count, 6);
        }

        cypher_result_free(result);
    }
}

/* Test labelPropagation with custom iterations */
static void test_label_propagation_custom_iterations(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN labelPropagation(5)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result_has_community_data(result));
        cypher_result_free(result);
    }
}

/* Test communities() alias */
static void test_communities_alias(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN communities()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        CU_ASSERT_TRUE(result_has_community_data(result));
        cypher_result_free(result);
    }
}

/* Test communityCount() */
static void test_community_count(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN communityCount()");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        /* Should detect 2 communities (two disconnected triangles) */
        if (result->success && result->row_count > 0 && result->data) {
            const char *value = result->data[0][0];
            CU_ASSERT_PTR_NOT_NULL(value);
            if (value) {
                int count = atoi(value);
                CU_ASSERT_EQUAL(count, 2);
            }
        }

        cypher_result_free(result);
    }
}

/* Test communityOf() */
static void test_community_of(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Node 1 and Node 2 should be in the same community */
    cypher_result *result1 = cypher_executor_execute(shared_executor,
        "RETURN communityOf(1)");
    cypher_result *result2 = cypher_executor_execute(shared_executor,
        "RETURN communityOf(2)");

    CU_ASSERT_PTR_NOT_NULL(result1);
    CU_ASSERT_PTR_NOT_NULL(result2);

    if (result1 && result2 && result1->success && result2->success) {
        const char *comm1 = result1->data[0][0];
        const char *comm2 = result2->data[0][0];
        CU_ASSERT_PTR_NOT_NULL(comm1);
        CU_ASSERT_PTR_NOT_NULL(comm2);
        if (comm1 && comm2) {
            CU_ASSERT_STRING_EQUAL(comm1, comm2);  /* Same community */
        }
    }

    if (result1) cypher_result_free(result1);
    if (result2) cypher_result_free(result2);

    /* Node 1 and Node 4 should be in different communities */
    result1 = cypher_executor_execute(shared_executor, "RETURN communityOf(1)");
    cypher_result *result4 = cypher_executor_execute(shared_executor, "RETURN communityOf(4)");

    if (result1 && result4 && result1->success && result4->success) {
        const char *comm1 = result1->data[0][0];
        const char *comm4 = result4->data[0][0];
        if (comm1 && comm4) {
            CU_ASSERT_STRING_NOT_EQUAL(comm1, comm4);  /* Different communities */
        }
    }

    if (result1) cypher_result_free(result1);
    if (result4) cypher_result_free(result4);
}

/* Test communityMembers() */
static void test_community_members(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Get community of node 1 */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN communityOf(1)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result && result->success && result->data) {
        int community_id = atoi(result->data[0][0]);
        cypher_result_free(result);

        /* Get members of that community */
        char query[100];
        snprintf(query, sizeof(query), "RETURN communityMembers(%d)", community_id);
        result = cypher_executor_execute(shared_executor, query);

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (result->success && result->data) {
                const char *members = result->data[0][0];
                /* Should contain nodes 1, 2, 3 */
                CU_ASSERT_PTR_NOT_NULL(strstr(members, "1"));
                CU_ASSERT_PTR_NOT_NULL(strstr(members, "2"));
                CU_ASSERT_PTR_NOT_NULL(strstr(members, "3"));
            }
            cypher_result_free(result);
        }
    }
}

/* Test label propagation on empty graph */
static void test_label_propagation_empty_graph(void)
{
    sqlite3 *empty_db;
    int rc = sqlite3_open(":memory:", &empty_db);
    CU_ASSERT_EQUAL(rc, SQLITE_OK);

    cypher_executor *empty_executor = cypher_executor_create(empty_db);
    CU_ASSERT_PTR_NOT_NULL(empty_executor);

    if (empty_executor) {
        cypher_result *result = cypher_executor_execute(empty_executor,
            "RETURN labelPropagation()");

        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
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

/* Test label propagation correctness - verify communities are correct */
static void test_label_propagation_correctness(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN labelPropagation(15)");

    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);

        if (result->success && result->row_count > 0 && result->data) {
            const char *json = result->data[0][0];

            /* Parse communities for each node */
            int communities[7] = {0};  /* 1-indexed */
            const char *p = json;
            while ((p = strstr(p, "node_id")) != NULL) {
                int node_id = 0, community = 0;
                if (sscanf(p, "node_id\":%d,\"community\":%d", &node_id, &community) == 2 ||
                    sscanf(p, "node_id\": %d, \"community\": %d", &node_id, &community) == 2) {
                    if (node_id >= 1 && node_id <= 6) {
                        communities[node_id] = community;
                    }
                }
                p++;
            }

            /* Nodes 1,2,3 should be in same community */
            CU_ASSERT_EQUAL(communities[1], communities[2]);
            CU_ASSERT_EQUAL(communities[2], communities[3]);

            /* Nodes 4,5,6 should be in same community */
            CU_ASSERT_EQUAL(communities[4], communities[5]);
            CU_ASSERT_EQUAL(communities[5], communities[6]);

            /* The two groups should be in different communities */
            CU_ASSERT_NOT_EQUAL(communities[1], communities[4]);
        }

        cypher_result_free(result);
    }
}

/* Test checkpoint-based batching with higher iteration counts */
static void test_label_propagation_checkpoints(void)
{
    CU_ASSERT_PTR_NOT_NULL(shared_executor);

    /* Test 16 iterations (2 batches with BATCH_SIZE=10) */
    cypher_result *result = cypher_executor_execute(shared_executor,
        "RETURN labelPropagation(16)");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success && result->error_message) {
            printf("16 iterations error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_community_data(result));
        cypher_result_free(result);
    }

    /* Test 20 iterations (2 batches) */
    result = cypher_executor_execute(shared_executor,
        "RETURN labelPropagation(20)");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success && result->error_message) {
            printf("20 iterations error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_community_data(result));
        cypher_result_free(result);
    }

    /* Test 50 iterations (5 batches) */
    result = cypher_executor_execute(shared_executor,
        "RETURN labelPropagation(50)");
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        CU_ASSERT_TRUE(result->success);
        if (!result->success && result->error_message) {
            printf("50 iterations error: %s\n", result->error_message);
        }
        CU_ASSERT_TRUE(result_has_community_data(result));
        cypher_result_free(result);
    }
}

/* Initialize the Label Propagation test suite */
int init_executor_label_propagation_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Label Propagation",
        setup_label_propagation_suite, teardown_label_propagation_suite);
    if (!suite) {
        return CU_get_error();
    }

    if (!CU_add_test(suite, "labelPropagation basic", test_label_propagation_basic) ||
        !CU_add_test(suite, "labelPropagation custom iterations", test_label_propagation_custom_iterations) ||
        !CU_add_test(suite, "communities() alias", test_communities_alias) ||
        !CU_add_test(suite, "communityCount()", test_community_count) ||
        !CU_add_test(suite, "communityOf()", test_community_of) ||
        !CU_add_test(suite, "communityMembers()", test_community_members) ||
        !CU_add_test(suite, "labelPropagation empty graph", test_label_propagation_empty_graph) ||
        !CU_add_test(suite, "labelPropagation correctness", test_label_propagation_correctness) ||
        !CU_add_test(suite, "labelPropagation checkpoints", test_label_propagation_checkpoints)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
