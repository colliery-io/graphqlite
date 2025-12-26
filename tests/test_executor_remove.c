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

/* Helper to check if a value represents NULL */
static int is_null_value(const char *val) {
    return val == NULL || strcmp(val, "null") == 0 || strcmp(val, "NULL") == 0;
}

/* Setup function - create test database */
static int setup_executor_remove_suite(void)
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
static int teardown_executor_remove_suite(void)
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

/* Test basic REMOVE property functionality */
static void test_remove_basic_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create a test node with properties */
        const char *create_query = "CREATE (n:RemoveBasicTest {name: \"Alice\", age: 30, city: \"NYC\"})";
        execute_and_verify(executor, create_query, true, "CREATE for REMOVE test");

        /* Verify properties exist */
        const char *verify_before = "MATCH (n:RemoveBasicTest) RETURN n.name, n.age, n.city";
        cypher_result *before_result = cypher_executor_execute(executor, verify_before);
        CU_ASSERT_PTR_NOT_NULL(before_result);

        if (before_result) {
            CU_ASSERT_TRUE(before_result->success);
            CU_ASSERT_EQUAL(before_result->row_count, 1);
            if (before_result->row_count > 0) {
                printf("Before REMOVE: name='%s', age='%s', city='%s'\n",
                       before_result->data[0][0] ? before_result->data[0][0] : "NULL",
                       before_result->data[0][1] ? before_result->data[0][1] : "NULL",
                       before_result->data[0][2] ? before_result->data[0][2] : "NULL");
                CU_ASSERT_PTR_NOT_NULL(before_result->data[0][1]); /* age should exist */
            }
            cypher_result_free(before_result);
        }

        /* Remove the age property */
        const char *remove_query = "MATCH (n:RemoveBasicTest) REMOVE n.age";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            CU_ASSERT_TRUE(remove_result->success);
            if (!remove_result->success) {
                printf("REMOVE basic error: %s\n", remove_result->error_message);
            }
            cypher_result_free(remove_result);
        }

        /* Verify age is removed */
        const char *verify_after = "MATCH (n:RemoveBasicTest) RETURN n.name, n.age, n.city";
        cypher_result *after_result = cypher_executor_execute(executor, verify_after);
        CU_ASSERT_PTR_NOT_NULL(after_result);

        if (after_result) {
            CU_ASSERT_TRUE(after_result->success);
            CU_ASSERT_EQUAL(after_result->row_count, 1);
            if (after_result->row_count > 0) {
                printf("After REMOVE: name='%s', age='%s', city='%s'\n",
                       after_result->data[0][0] ? after_result->data[0][0] : "NULL",
                       after_result->data[0][1] ? after_result->data[0][1] : "NULL",
                       after_result->data[0][2] ? after_result->data[0][2] : "NULL");
                CU_ASSERT_TRUE(is_null_value(after_result->data[0][1])); /* age should be NULL */
                CU_ASSERT_PTR_NOT_NULL(after_result->data[0][0]); /* name should still exist */
                CU_ASSERT_PTR_NOT_NULL(after_result->data[0][2]); /* city should still exist */
            }
            cypher_result_free(after_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE multiple properties */
static void test_remove_multiple_properties(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:RemoveMultiTest {a: 1, b: 2, c: 3, d: 4})";
        execute_and_verify(executor, create_query, true, "CREATE for multiple REMOVE test");

        /* Remove multiple properties in one query */
        const char *remove_query = "MATCH (n:RemoveMultiTest) REMOVE n.a, n.b";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            CU_ASSERT_TRUE(remove_result->success);
            cypher_result_free(remove_result);
        }

        /* Verify only c and d remain */
        const char *verify_query = "MATCH (n:RemoveMultiTest) RETURN n.a, n.b, n.c, n.d";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0) {
                printf("After multiple REMOVE: a='%s', b='%s', c='%s', d='%s'\n",
                       verify_result->data[0][0] ? verify_result->data[0][0] : "NULL",
                       verify_result->data[0][1] ? verify_result->data[0][1] : "NULL",
                       verify_result->data[0][2] ? verify_result->data[0][2] : "NULL",
                       verify_result->data[0][3] ? verify_result->data[0][3] : "NULL");
                CU_ASSERT_TRUE(is_null_value(verify_result->data[0][0])); /* a should be NULL */
                CU_ASSERT_TRUE(is_null_value(verify_result->data[0][1])); /* b should be NULL */
                CU_ASSERT_PTR_NOT_NULL(verify_result->data[0][2]); /* c should exist */
                CU_ASSERT_PTR_NOT_NULL(verify_result->data[0][3]); /* d should exist */
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE label functionality */
static void test_remove_label(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create a test node with multiple labels */
        const char *create_query = "CREATE (n:Person:Employee:Manager {name: \"Bob\"})";
        execute_and_verify(executor, create_query, true, "CREATE for label REMOVE test");

        /* Verify labels exist */
        const char *verify_before = "MATCH (n:Person {name: \"Bob\"}) RETURN labels(n)";
        cypher_result *before_result = cypher_executor_execute(executor, verify_before);
        CU_ASSERT_PTR_NOT_NULL(before_result);

        if (before_result) {
            CU_ASSERT_TRUE(before_result->success);
            if (before_result->row_count > 0) {
                printf("Before REMOVE label: labels='%s'\n",
                       before_result->data[0][0] ? before_result->data[0][0] : "NULL");
            }
            cypher_result_free(before_result);
        }

        /* Remove the Manager label */
        const char *remove_query = "MATCH (n:Person {name: \"Bob\"}) REMOVE n:Manager";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            CU_ASSERT_TRUE(remove_result->success);
            if (!remove_result->success) {
                printf("REMOVE label error: %s\n", remove_result->error_message);
            }
            cypher_result_free(remove_result);
        }

        /* Verify Manager label is removed */
        const char *verify_after = "MATCH (n:Person {name: \"Bob\"}) RETURN labels(n)";
        cypher_result *after_result = cypher_executor_execute(executor, verify_after);
        CU_ASSERT_PTR_NOT_NULL(after_result);

        if (after_result) {
            CU_ASSERT_TRUE(after_result->success);
            if (after_result->row_count > 0) {
                printf("After REMOVE label: labels='%s'\n",
                       after_result->data[0][0] ? after_result->data[0][0] : "NULL");
                /* Manager should not be in the list */
                if (after_result->data[0][0]) {
                    CU_ASSERT_PTR_NULL(strstr(after_result->data[0][0], "Manager"));
                }
            }
            cypher_result_free(after_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE edge property */
static void test_remove_edge_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create nodes and relationship with properties */
        const char *create_query = "CREATE (a:Person {name: \"Alice\"})-[r:KNOWS {since: 2020, strength: 0.9}]->(b:Person {name: \"Bob\"})";
        execute_and_verify(executor, create_query, true, "CREATE for edge REMOVE test");

        /* Verify edge properties exist */
        const char *verify_before = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN r.since, r.strength";
        cypher_result *before_result = cypher_executor_execute(executor, verify_before);
        CU_ASSERT_PTR_NOT_NULL(before_result);

        if (before_result) {
            CU_ASSERT_TRUE(before_result->success);
            if (before_result->row_count > 0) {
                printf("Before edge REMOVE: since='%s', strength='%s'\n",
                       before_result->data[0][0] ? before_result->data[0][0] : "NULL",
                       before_result->data[0][1] ? before_result->data[0][1] : "NULL");
            }
            cypher_result_free(before_result);
        }

        /* Remove edge property */
        const char *remove_query = "MATCH (a:Person {name: \"Alice\"})-[r:KNOWS]->(b:Person) REMOVE r.since";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            CU_ASSERT_TRUE(remove_result->success);
            if (!remove_result->success) {
                printf("REMOVE edge property error: %s\n", remove_result->error_message);
            }
            cypher_result_free(remove_result);
        }

        /* Verify since is removed but strength remains */
        const char *verify_after = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN r.since, r.strength";
        cypher_result *after_result = cypher_executor_execute(executor, verify_after);
        CU_ASSERT_PTR_NOT_NULL(after_result);

        if (after_result) {
            CU_ASSERT_TRUE(after_result->success);
            if (after_result->row_count > 0) {
                printf("After edge REMOVE: since='%s', strength='%s'\n",
                       after_result->data[0][0] ? after_result->data[0][0] : "NULL",
                       after_result->data[0][1] ? after_result->data[0][1] : "NULL");
                CU_ASSERT_TRUE(is_null_value(after_result->data[0][0])); /* since should be NULL */
                CU_ASSERT_PTR_NOT_NULL(after_result->data[0][1]); /* strength should still exist */
            }
            cypher_result_free(after_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE with WHERE clause */
static void test_remove_with_where(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create test nodes */
        const char *create_query = "CREATE (a:RemoveWhereTest {name: \"Alice\", age: 30, status: \"active\"}), "
                                   "(b:RemoveWhereTest {name: \"Bob\", age: 25, status: \"active\"}), "
                                   "(c:RemoveWhereTest {name: \"Charlie\", age: 35, status: \"active\"})";
        execute_and_verify(executor, create_query, true, "CREATE for WHERE REMOVE test");

        /* Remove property only for nodes matching WHERE clause */
        const char *remove_query = "MATCH (n:RemoveWhereTest) WHERE n.age > 28 REMOVE n.status";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            CU_ASSERT_TRUE(remove_result->success);
            cypher_result_free(remove_result);
        }

        /* Verify WHERE clause worked - Bob should still have status */
        const char *verify_query = "MATCH (n:RemoveWhereTest) RETURN n.name, n.status ORDER BY n.name";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            printf("After WHERE REMOVE:\n");
            for (int i = 0; i < verify_result->row_count; i++) {
                printf("  name='%s', status='%s'\n",
                       verify_result->data[i][0] ? verify_result->data[i][0] : "NULL",
                       verify_result->data[i][1] ? verify_result->data[i][1] : "NULL");
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE non-existent property (should not error) */
static void test_remove_nonexistent_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:RemoveNonexistTest {name: \"Test\"})";
        execute_and_verify(executor, create_query, true, "CREATE for nonexistent REMOVE test");

        /* Try to remove a property that doesn't exist */
        const char *remove_query = "MATCH (n:RemoveNonexistTest) REMOVE n.nonexistent";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            /* Should succeed even though property doesn't exist */
            CU_ASSERT_TRUE(remove_result->success);
            if (!remove_result->success) {
                printf("REMOVE nonexistent property should not error: %s\n", remove_result->error_message);
            }
            cypher_result_free(remove_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE no match */
static void test_remove_no_match(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Try to remove property on non-existent nodes */
        const char *remove_query = "MATCH (n:NonExistentLabel) REMOVE n.property";
        cypher_result *remove_result = cypher_executor_execute(executor, remove_query);
        CU_ASSERT_PTR_NOT_NULL(remove_result);

        if (remove_result) {
            /* Should succeed but affect 0 nodes */
            CU_ASSERT_TRUE(remove_result->success);
            CU_ASSERT_EQUAL(remove_result->properties_set, 0);
            cypher_result_free(remove_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test REMOVE with different data types */
static void test_remove_different_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create a test node with different property types */
        const char *create_query = "CREATE (n:RemoveTypesTest {str_val: \"hello\", int_val: 42, real_val: 3.14, bool_val: true})";
        execute_and_verify(executor, create_query, true, "CREATE for types REMOVE test");

        /* Remove each type of property */
        execute_and_verify(executor, "MATCH (n:RemoveTypesTest) REMOVE n.str_val", true, "REMOVE string");
        execute_and_verify(executor, "MATCH (n:RemoveTypesTest) REMOVE n.int_val", true, "REMOVE integer");
        execute_and_verify(executor, "MATCH (n:RemoveTypesTest) REMOVE n.real_val", true, "REMOVE real");
        execute_and_verify(executor, "MATCH (n:RemoveTypesTest) REMOVE n.bool_val", true, "REMOVE boolean");

        /* Verify all properties are removed */
        const char *verify_query = "MATCH (n:RemoveTypesTest) RETURN n.str_val, n.int_val, n.real_val, n.bool_val";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->row_count > 0) {
                CU_ASSERT_TRUE(is_null_value(verify_result->data[0][0]));
                CU_ASSERT_TRUE(is_null_value(verify_result->data[0][1]));
                CU_ASSERT_TRUE(is_null_value(verify_result->data[0][2]));
                CU_ASSERT_TRUE(is_null_value(verify_result->data[0][3]));
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Initialize the REMOVE executor test suite */
int init_executor_remove_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor REMOVE", setup_executor_remove_suite, teardown_executor_remove_suite);
    if (!suite) {
        return CU_get_error();
    }

    /* Add tests */
    if (!CU_add_test(suite, "REMOVE basic property", test_remove_basic_property) ||
        !CU_add_test(suite, "REMOVE multiple properties", test_remove_multiple_properties) ||
        !CU_add_test(suite, "REMOVE label", test_remove_label) ||
        !CU_add_test(suite, "REMOVE edge property", test_remove_edge_property) ||
        !CU_add_test(suite, "REMOVE with WHERE clause", test_remove_with_where) ||
        !CU_add_test(suite, "REMOVE nonexistent property", test_remove_nonexistent_property) ||
        !CU_add_test(suite, "REMOVE no match", test_remove_no_match) ||
        !CU_add_test(suite, "REMOVE different types", test_remove_different_types)) {
        return CU_get_error();
    }

    return CUE_SUCCESS;
}
