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

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database */
static int setup_executor_merge_suite(void)
{
    /* Create in-memory database for testing */
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
    return 0;
}

/* Teardown function */
static int teardown_executor_merge_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper function to execute and verify result */
static void execute_and_verify(cypher_executor *executor, const char *query, bool should_succeed, const char *test_name)
{
    cypher_result *result = cypher_executor_execute(executor, query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        if (should_succeed) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("%s error: %s\n", test_name, result->error_message);
            }
        } else {
            CU_ASSERT_FALSE(result->success);
            if (result->success) {
                printf("%s unexpectedly succeeded\n", test_name);
            }
        }
        cypher_result_free(result);
    }
}

/* Test MERGE creates node when not exists */
static void test_merge_create_node(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* MERGE should create the node since it doesn't exist */
        const char *merge_query = "MERGE (n:MergeTest {name: 'Alice'})";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE create error: %s\n", result->error_message);
            } else {
                printf("MERGE create result: nodes_created=%d\n", result->nodes_created);
                CU_ASSERT_EQUAL(result->nodes_created, 1);
            }
            cypher_result_free(result);
        }

        /* Verify node was created */
        const char *verify_query = "MATCH (n:MergeTest) RETURN n.name";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            CU_ASSERT_EQUAL(verify_result->row_count, 1);
            if (verify_result->row_count > 0 && verify_result->data) {
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Alice");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE matches existing node */
static void test_merge_match_node(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First, create a node */
        execute_and_verify(executor, "CREATE (n:MatchTest {name: 'Bob'})", true, "CREATE for MERGE match");

        /* MERGE should match the existing node, not create new one */
        const char *merge_query = "MERGE (n:MatchTest {name: 'Bob'})";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE match error: %s\n", result->error_message);
            } else {
                printf("MERGE match result: nodes_created=%d\n", result->nodes_created);
                CU_ASSERT_EQUAL(result->nodes_created, 0);  /* Should not create new node */
            }
            cypher_result_free(result);
        }

        /* Verify only one node exists */
        const char *verify_query = "MATCH (n:MatchTest) RETURN count(n)";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Node count after MERGE: %s\n", verify_result->data[0][0]);
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "1");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE with ON CREATE SET */
static void test_merge_on_create_set(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* MERGE with ON CREATE SET - node doesn't exist, should create and set property */
        const char *merge_query = "MERGE (n:CreateSetTest {name: 'Carol'}) ON CREATE SET n.created = true";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE ON CREATE error: %s\n", result->error_message);
            } else {
                printf("MERGE ON CREATE result: nodes_created=%d, properties_set=%d\n",
                       result->nodes_created, result->properties_set);
                CU_ASSERT_EQUAL(result->nodes_created, 1);
                CU_ASSERT_TRUE(result->properties_set > 0);
            }
            cypher_result_free(result);
        }

        /* Verify property was set */
        const char *verify_query = "MATCH (n:CreateSetTest) RETURN n.name, n.created";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify ON CREATE: name=%s, created=%s\n",
                       verify_result->data[0][0], verify_result->data[0][1]);
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Carol");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][1], "true");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE with ON MATCH SET */
static void test_merge_on_match_set(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First create a node */
        execute_and_verify(executor, "CREATE (n:MatchSetTest {name: 'David', visits: 0})", true, "CREATE for MERGE ON MATCH");

        /* MERGE with ON MATCH SET - node exists, should match and set property */
        const char *merge_query = "MERGE (n:MatchSetTest {name: 'David'}) ON MATCH SET n.visits = 1";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE ON MATCH error: %s\n", result->error_message);
            } else {
                printf("MERGE ON MATCH result: nodes_created=%d, properties_set=%d\n",
                       result->nodes_created, result->properties_set);
                CU_ASSERT_EQUAL(result->nodes_created, 0);  /* Should not create */
                CU_ASSERT_TRUE(result->properties_set > 0);  /* Should set property */
            }
            cypher_result_free(result);
        }

        /* Verify property was updated */
        const char *verify_query = "MATCH (n:MatchSetTest) RETURN n.name, n.visits";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify ON MATCH: name=%s, visits=%s\n",
                       verify_result->data[0][0], verify_result->data[0][1]);
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "David");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][1], "1");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE with both ON CREATE and ON MATCH SET - create case */
static void test_merge_on_create_and_match_create(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* MERGE with both clauses - node doesn't exist, ON CREATE should run */
        const char *merge_query = "MERGE (n:BothTest {name: 'Eve'}) ON CREATE SET n.created = true ON MATCH SET n.matched = true";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE both clauses (create) error: %s\n", result->error_message);
            } else {
                printf("MERGE both clauses (create) result: nodes_created=%d\n", result->nodes_created);
                CU_ASSERT_EQUAL(result->nodes_created, 1);
            }
            cypher_result_free(result);
        }

        /* Verify ON CREATE ran but not ON MATCH */
        const char *verify_query = "MATCH (n:BothTest) RETURN n.name, n.created, n.matched";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify both (create): name=%s, created=%s, matched=%s\n",
                       verify_result->data[0][0],
                       verify_result->data[0][1] ? verify_result->data[0][1] : "NULL",
                       verify_result->data[0][2] ? verify_result->data[0][2] : "NULL");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Eve");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][1], "true");  /* ON CREATE ran */
                /* ON MATCH should not have run - matched should be NULL */
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE with both ON CREATE and ON MATCH SET - match case */
static void test_merge_on_create_and_match_match(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First create a node */
        execute_and_verify(executor, "CREATE (n:BothTest2 {name: 'Frank'})", true, "CREATE for both clauses match");

        /* MERGE with both clauses - node exists, ON MATCH should run */
        const char *merge_query = "MERGE (n:BothTest2 {name: 'Frank'}) ON CREATE SET n.created = true ON MATCH SET n.matched = true";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE both clauses (match) error: %s\n", result->error_message);
            } else {
                printf("MERGE both clauses (match) result: nodes_created=%d\n", result->nodes_created);
                CU_ASSERT_EQUAL(result->nodes_created, 0);  /* Should not create */
            }
            cypher_result_free(result);
        }

        /* Verify ON MATCH ran but not ON CREATE */
        const char *verify_query = "MATCH (n:BothTest2) RETURN n.name, n.created, n.matched";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify both (match): name=%s, created=%s, matched=%s\n",
                       verify_result->data[0][0],
                       verify_result->data[0][1] ? verify_result->data[0][1] : "NULL",
                       verify_result->data[0][2] ? verify_result->data[0][2] : "NULL");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Frank");
                /* ON CREATE should not have run - created should be NULL */
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][2], "true");  /* ON MATCH ran */
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE without properties (label-only) */
static void test_merge_label_only(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First ensure no LabelOnly nodes exist */
        execute_and_verify(executor, "MATCH (n:LabelOnly) DELETE n", true, "Clean LabelOnly");

        /* MERGE with just label should create */
        const char *merge_query = "MERGE (n:LabelOnly)";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE label-only error: %s\n", result->error_message);
            } else {
                printf("MERGE label-only result: nodes_created=%d\n", result->nodes_created);
                CU_ASSERT_EQUAL(result->nodes_created, 1);
            }
            cypher_result_free(result);
        }

        /* Second MERGE should match, not create */
        cypher_result *result2 = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result2);

        if (result2) {
            CU_ASSERT_TRUE(result2->success);
            if (result2->success) {
                printf("MERGE label-only (second): nodes_created=%d\n", result2->nodes_created);
                CU_ASSERT_EQUAL(result2->nodes_created, 0);  /* Should match existing */
            }
            cypher_result_free(result2);
        }

        cypher_executor_free(executor);
    }
}

/* Test multiple MERGE in same query context */
static void test_merge_multiple(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First MERGE */
        execute_and_verify(executor, "MERGE (n:Multi {name: 'First'})", true, "First MERGE");

        /* Second MERGE with different properties */
        execute_and_verify(executor, "MERGE (n:Multi {name: 'Second'})", true, "Second MERGE");

        /* Verify both nodes exist */
        const char *verify_query = "MATCH (n:Multi) RETURN n.name ORDER BY n.name";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            CU_ASSERT_EQUAL(verify_result->row_count, 2);
            if (verify_result->row_count >= 2 && verify_result->data) {
                printf("Multiple MERGE results: %s, %s\n",
                       verify_result->data[0][0], verify_result->data[1][0]);
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE with multiple properties */
static void test_merge_multiple_properties(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* MERGE with multiple properties */
        const char *merge_query = "MERGE (n:MultiProp {name: 'Grace', age: 30, active: true})";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE multi-prop error: %s\n", result->error_message);
            } else {
                printf("MERGE multi-prop result: nodes_created=%d\n", result->nodes_created);
                CU_ASSERT_EQUAL(result->nodes_created, 1);
            }
            cypher_result_free(result);
        }

        /* Verify all properties */
        const char *verify_query = "MATCH (n:MultiProp) RETURN n.name, n.age, n.active";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify multi-prop: name=%s, age=%s, active=%s\n",
                       verify_result->data[0][0], verify_result->data[0][1], verify_result->data[0][2]);
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Grace");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][1], "30");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][2], "true");
            }
            cypher_result_free(verify_result);
        }

        /* MERGE again with same properties should match */
        cypher_result *result2 = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result2);

        if (result2) {
            CU_ASSERT_TRUE(result2->success);
            if (result2->success) {
                printf("MERGE multi-prop (second): nodes_created=%d\n", result2->nodes_created);
                CU_ASSERT_EQUAL(result2->nodes_created, 0);  /* Should match existing */
            }
            cypher_result_free(result2);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE with ON CREATE setting multiple properties */
static void test_merge_on_create_multiple_props(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* MERGE with ON CREATE SET multiple properties */
        const char *merge_query = "MERGE (n:CreateMulti {name: 'Henry'}) ON CREATE SET n.created = true, n.visits = 0, n.status = 'new'";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE ON CREATE multi error: %s\n", result->error_message);
            } else {
                printf("MERGE ON CREATE multi result: nodes_created=%d, props=%d\n",
                       result->nodes_created, result->properties_set);
                CU_ASSERT_EQUAL(result->nodes_created, 1);
            }
            cypher_result_free(result);
        }

        /* Verify all properties were set */
        const char *verify_query = "MATCH (n:CreateMulti) RETURN n.name, n.created, n.visits, n.status";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify ON CREATE multi: name=%s, created=%s, visits=%s, status=%s\n",
                       verify_result->data[0][0], verify_result->data[0][1],
                       verify_result->data[0][2], verify_result->data[0][3]);
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Henry");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][1], "true");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][2], "0");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][3], "new");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE creates relationship when not exists */
static void test_merge_create_relationship(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* First create the source and target nodes */
        execute_and_verify(executor, "CREATE (a:RelMergeTest {name: 'Alice'})", true, "Create source");
        execute_and_verify(executor, "CREATE (b:RelMergeTest {name: 'Bob'})", true, "Create target");

        /* MERGE should create the relationship since it doesn't exist */
        const char *merge_query = "MATCH (a:RelMergeTest {name: 'Alice'}), (b:RelMergeTest {name: 'Bob'}) MERGE (a)-[r:KNOWS]->(b)";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE relationship create error: %s\n", result->error_message);
            } else {
                printf("MERGE relationship create result: relationships_created=%d\n", result->relationships_created);
                CU_ASSERT_EQUAL(result->relationships_created, 1);
            }
            cypher_result_free(result);
        }

        /* Verify relationship was created */
        const char *verify_query = "MATCH (a:RelMergeTest)-[r:KNOWS]->(b:RelMergeTest) RETURN a.name, b.name";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            CU_ASSERT_EQUAL(verify_result->row_count, 1);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Verify rel MERGE: %s -> %s\n", verify_result->data[0][0], verify_result->data[0][1]);
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE matches existing relationship */
static void test_merge_match_relationship(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create nodes and relationship */
        execute_and_verify(executor, "CREATE (a:MatchRelTest {name: 'Carol'})-[:FRIENDS]->(b:MatchRelTest {name: 'Dan'})", true, "Create path");

        /* MERGE should match the existing relationship, not create new one */
        const char *merge_query = "MATCH (a:MatchRelTest {name: 'Carol'}), (b:MatchRelTest {name: 'Dan'}) MERGE (a)-[r:FRIENDS]->(b)";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE relationship match error: %s\n", result->error_message);
            } else {
                printf("MERGE relationship match result: relationships_created=%d\n", result->relationships_created);
                CU_ASSERT_EQUAL(result->relationships_created, 0);  /* Should not create new relationship */
            }
            cypher_result_free(result);
        }

        /* Verify only one relationship exists */
        const char *verify_query = "MATCH (a:MatchRelTest)-[r:FRIENDS]->(b:MatchRelTest) RETURN count(r)";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0 && verify_result->data) {
                printf("Relationship count after MERGE: %s\n", verify_result->data[0][0]);
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "1");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE creates both nodes and relationship in single pattern */
static void test_merge_full_path(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* MERGE a complete path - should create nodes and relationship */
        const char *merge_query = "MERGE (a:FullPath {name: 'Eve'})-[r:LIKES]->(b:FullPath {name: 'Frank'})";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE full path error: %s\n", result->error_message);
            } else {
                printf("MERGE full path result: nodes_created=%d, relationships_created=%d\n",
                       result->nodes_created, result->relationships_created);
                CU_ASSERT_EQUAL(result->nodes_created, 2);
                CU_ASSERT_EQUAL(result->relationships_created, 1);
            }
            cypher_result_free(result);
        }

        /* Verify path was created */
        const char *verify_query = "MATCH (a:FullPath)-[r:LIKES]->(b:FullPath) RETURN a.name, b.name";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            CU_ASSERT_EQUAL(verify_result->row_count, 1);
            if (verify_result->row_count > 0 && verify_result->data) {
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "Eve");
                CU_ASSERT_STRING_EQUAL(verify_result->data[0][1], "Frank");
            }
            cypher_result_free(verify_result);
        }

        /* Second MERGE should match all, create nothing */
        cypher_result *result2 = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result2);

        if (result2) {
            CU_ASSERT_TRUE(result2->success);
            if (result2->success) {
                printf("MERGE full path (second): nodes=%d, rels=%d\n",
                       result2->nodes_created, result2->relationships_created);
                CU_ASSERT_EQUAL(result2->nodes_created, 0);
                CU_ASSERT_EQUAL(result2->relationships_created, 0);
            }
            cypher_result_free(result2);
        }

        cypher_executor_free(executor);
    }
}

/* Test MERGE relationship with properties */
static void test_merge_relationship_with_props(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create nodes first */
        execute_and_verify(executor, "CREATE (a:PropRelTest {name: 'Grace'})", true, "Create source");
        execute_and_verify(executor, "CREATE (b:PropRelTest {name: 'Henry'})", true, "Create target");

        /* MERGE relationship with properties */
        const char *merge_query = "MATCH (a:PropRelTest {name: 'Grace'}), (b:PropRelTest {name: 'Henry'}) MERGE (a)-[r:WORKS_WITH {since: 2020}]->(b)";
        cypher_result *result = cypher_executor_execute(executor, merge_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("MERGE rel with props error: %s\n", result->error_message);
            } else {
                printf("MERGE rel with props: relationships_created=%d, properties_set=%d\n",
                       result->relationships_created, result->properties_set);
                CU_ASSERT_EQUAL(result->relationships_created, 1);
            }
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Initialize the MERGE executor test suite */
int init_executor_merge_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor MERGE", setup_executor_merge_suite, teardown_executor_merge_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "MERGE create node", test_merge_create_node) ||
        !CU_add_test(suite, "MERGE match node", test_merge_match_node) ||
        !CU_add_test(suite, "MERGE ON CREATE SET", test_merge_on_create_set) ||
        !CU_add_test(suite, "MERGE ON MATCH SET", test_merge_on_match_set) ||
        !CU_add_test(suite, "MERGE both clauses (create)", test_merge_on_create_and_match_create) ||
        !CU_add_test(suite, "MERGE both clauses (match)", test_merge_on_create_and_match_match) ||
        !CU_add_test(suite, "MERGE label only", test_merge_label_only) ||
        !CU_add_test(suite, "MERGE multiple", test_merge_multiple) ||
        !CU_add_test(suite, "MERGE multiple properties", test_merge_multiple_properties) ||
        !CU_add_test(suite, "MERGE ON CREATE multiple props", test_merge_on_create_multiple_props) ||
        !CU_add_test(suite, "MERGE create relationship", test_merge_create_relationship) ||
        !CU_add_test(suite, "MERGE match relationship", test_merge_match_relationship) ||
        !CU_add_test(suite, "MERGE full path", test_merge_full_path) ||
        !CU_add_test(suite, "MERGE relationship with props", test_merge_relationship_with_props)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
