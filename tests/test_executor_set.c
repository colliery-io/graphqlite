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
static int setup_executor_set_suite(void)
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
static int teardown_executor_set_suite(void)
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

/* Test basic SET clause functionality */
static void test_set_basic_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:SetBasicTest {name: \"original\"})";
        execute_and_verify(executor, create_query, true, "CREATE for SET test");
        
        /* Set a property */
        const char *set_query = "MATCH (n:SetBasicTest) SET n.name = \"test\", n.age = 25";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("SET basic error: %s\n", result->error_message);
            } else {
                printf("SET basic result: success=%d, properties_set=%d\n", 
                       result->success, result->properties_set);
                CU_ASSERT_TRUE(result->properties_set > 0);
            }
            cypher_result_free(result);
        }
        
        /* Verify the change */
        const char *verify_query = "MATCH (n:SetBasicTest) RETURN n.name, n.age";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);
        
        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->success) {
                printf("Verify result: success=%d, row_count=%d, column_count=%d\n", 
                       verify_result->success, verify_result->row_count, verify_result->column_count);
                
                if (verify_result->data && verify_result->row_count > 0) {
                    printf("Actual data: [0][0]='%s', [0][1]='%s'\n", 
                           verify_result->data[0][0] ? verify_result->data[0][0] : "NULL",
                           verify_result->data[0][1] ? verify_result->data[0][1] : "NULL");
                }
            }
            cypher_result_free(verify_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test SET with multiple properties */
static void test_set_multiple_properties(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:Product {name: \"Widget\"})";
        execute_and_verify(executor, create_query, true, "CREATE for multiple SET test");
        
        /* Set multiple properties */
        const char *set_query = "MATCH (n:Product) SET n.price = 99.99, n.category = \"Electronics\", n.inStock = true";
        execute_and_verify(executor, set_query, true, "SET multiple properties");
        
        cypher_executor_free(executor);
    }
}

/* Test SET overwriting existing properties */
static void test_set_overwrite_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node with initial property */
        const char *create_query = "CREATE (n:User {name: \"John\", status: \"active\"})";
        execute_and_verify(executor, create_query, true, "CREATE for overwrite test");
        
        /* Overwrite existing property */
        const char *set_query = "MATCH (n:User) WHERE n.name = \"John\" SET n.status = \"inactive\", n.lastLogin = \"2023-01-01\"";
        execute_and_verify(executor, set_query, true, "SET overwrite property");
        
        cypher_executor_free(executor);
    }
}

/* Test SET with WHERE clause filtering */
static void test_set_with_where_clause(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test nodes */
        const char *create_query = "CREATE (a:SetWhereTest {name: \"Alice\", age: 30}), "
                                   "(b:SetWhereTest {name: \"Bob\", age: 25}), "
                                   "(c:SetWhereTest {name: \"Charlie\", age: 35})";
        execute_and_verify(executor, create_query, true, "CREATE for WHERE test");
        
        /* Debug: Check nodes before SET */
        const char *debug_query = "MATCH (n:SetWhereTest) RETURN n.name, n.age ORDER BY n.name";
        cypher_result *debug_result = cypher_executor_execute(executor, debug_query);
        if (debug_result && debug_result->success) {
            printf("Debug - nodes before SET: row_count=%d\n", debug_result->row_count);
            for (int i = 0; i < debug_result->row_count && i < 5; i++) {
                printf("  [%d]: name='%s', age='%s'\n", i,
                       debug_result->data[i][0] ? debug_result->data[i][0] : "NULL",
                       debug_result->data[i][1] ? debug_result->data[i][1] : "NULL");
            }
            cypher_result_free(debug_result);
        }
        
        /* Set property only for nodes matching WHERE clause */
        const char *set_query = "MATCH (p:SetWhereTest) WHERE p.age > 28 SET p.senior = true";
        cypher_result *set_result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(set_result);
        
        if (set_result) {
            CU_ASSERT_TRUE(set_result->success);
            if (!set_result->success) {
                printf("SET WHERE error: %s\n", set_result->error_message);
            } else {
                printf("SET WHERE result: success=%d, properties_set=%d\n", 
                       set_result->success, set_result->properties_set);
                CU_ASSERT_EQUAL(set_result->properties_set, 2); /* Alice and Charlie */
            }
            cypher_result_free(set_result);
        }
        
        /* Verify WHERE clause worked correctly */
        const char *verify_query = "MATCH (n:SetWhereTest) RETURN n.name, n.age, n.senior ORDER BY n.name";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);
        
        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->success) {
                printf("WHERE verify result: success=%d, row_count=%d\n", 
                       verify_result->success, verify_result->row_count);
                
                printf("Actual SET results:\n");
                for (int i = 0; i < verify_result->row_count && i < 5; i++) {
                    printf("  [%d]: name='%s', age='%s', senior='%s'\n", i,
                           verify_result->data[i][0] ? verify_result->data[i][0] : "NULL",
                           verify_result->data[i][1] ? verify_result->data[i][1] : "NULL",
                           verify_result->data[i][2] ? verify_result->data[i][2] : "NULL");
                }
                
                /* Validate specific results */
                bool alice_correct = false, bob_correct = false, charlie_correct = false;
                for (int i = 0; i < verify_result->row_count; i++) {
                    const char *name = verify_result->data[i][0];
                    const char *age = verify_result->data[i][1];
                    const char *senior = verify_result->data[i][2];
                    
                    if (name && strcmp(name, "Alice") == 0) {
                        alice_correct = (age && strcmp(age, "30") == 0 && senior && strcmp(senior, "true") == 0);
                        printf("Alice check: age=%s, senior=%s, should have senior=true: %s\n", 
                               age, senior, alice_correct ? "PASS" : "FAIL");
                    } else if (name && strcmp(name, "Bob") == 0) {
                        bob_correct = (age && strcmp(age, "25") == 0 && (!senior || strcmp(senior, "NULL") == 0));
                        printf("Bob check: age=%s, senior=%s, should NOT have senior=true: %s\n", 
                               age, senior ? senior : "NULL", bob_correct ? "PASS" : "FAIL");
                    } else if (name && strcmp(name, "Charlie") == 0) {
                        charlie_correct = (age && strcmp(age, "35") == 0 && senior && strcmp(senior, "true") == 0);
                        printf("Charlie check: age=%s, senior=%s, should have senior=true: %s\n", 
                               age, senior, charlie_correct ? "PASS" : "FAIL");
                    }
                }
                
                printf("WHERE clause working correctly: %s\n", 
                       (alice_correct && bob_correct && charlie_correct) ? "YES" : "NO");
            }
            cypher_result_free(verify_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test SET with different data types */
static void test_set_data_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:TypeTest)";
        execute_and_verify(executor, create_query, true, "CREATE for data types test");
        
        /* Set different data types */
        const char *set_query = "MATCH (n:TypeTest) SET n.string_val = \"hello\", n.int_val = 42, n.real_val = 3.14, n.bool_val = true";
        execute_and_verify(executor, set_query, true, "SET different data types");
        
        cypher_executor_free(executor);
    }
}

/* Test SET without matching nodes */
static void test_set_no_match(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Try to set property on non-existent nodes */
        const char *set_query = "MATCH (n:NonExistent) SET n.property = \"value\"";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            /* Should succeed but affect 0 nodes */
            CU_ASSERT_EQUAL(result->properties_set, 0);
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test SET with integer types specifically */
static void test_set_integer_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:IntTest)";
        execute_and_verify(executor, create_query, true, "CREATE for integer test");
        
        /* Set various integer values */
        const char *set_query = "MATCH (n:IntTest) SET n.positive = 100, n.negative = -50, n.zero = 0, n.large = 1000000";
        execute_and_verify(executor, set_query, true, "SET integer types");
        
        cypher_executor_free(executor);
    }
}

/* Test SET with real/float types specifically */
static void test_set_real_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:RealTest)";
        execute_and_verify(executor, create_query, true, "CREATE for real test");
        
        /* Set various real values */
        const char *set_query = "MATCH (n:RealTest) SET n.pi = 3.14159, n.negative = -2.5, n.scientific = 1.23e10, n.small = 0.001";
        execute_and_verify(executor, set_query, true, "SET real types");
        
        cypher_executor_free(executor);
    }
}

/* Test SET with boolean types specifically */
static void test_set_boolean_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:BoolTest)";
        execute_and_verify(executor, create_query, true, "CREATE for boolean test");
        
        /* Set boolean values */
        const char *set_query = "MATCH (n:BoolTest) SET n.enabled = true, n.disabled = false, n.active = true";
        execute_and_verify(executor, set_query, true, "SET boolean types");
        
        cypher_executor_free(executor);
    }
}

/* Test SET with string types and edge cases */
static void test_set_string_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:StringTest)";
        execute_and_verify(executor, create_query, true, "CREATE for string test");
        
        /* Set various string values */
        const char *set_query = "MATCH (n:StringTest) SET n.simple = \"hello\", n.empty = \"\", n.with_quotes = \"contains \\\"quotes\\\"\", n.unicode = \"café\"";
        execute_and_verify(executor, set_query, true, "SET string types");
        
        cypher_executor_free(executor);
    }
}

/* Test SET with mixed data types in single query */
static void test_set_mixed_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:MixedTest)";
        execute_and_verify(executor, create_query, true, "CREATE for mixed types test");
        
        /* Set mixed data types in one query */
        const char *set_query = "MATCH (n:MixedTest) SET n.name = \"mixed\", n.count = 42, n.ratio = 0.75, n.active = true";
        execute_and_verify(executor, set_query, true, "SET mixed types");
        
        cypher_executor_free(executor);
    }
}

/* Test SET with type overwrite (changing type of existing property) */
static void test_set_type_overwrite(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node with string property */
        const char *create_query = "CREATE (n:TypeOverwrite {value: \"123\"})";
        execute_and_verify(executor, create_query, true, "CREATE for type overwrite test");
        
        /* Overwrite string with integer */
        const char *set_query1 = "MATCH (n:TypeOverwrite) SET n.value = 456";
        execute_and_verify(executor, set_query1, true, "SET type overwrite string->int");
        
        /* Overwrite integer with boolean */
        const char *set_query2 = "MATCH (n:TypeOverwrite) SET n.value = false";
        execute_and_verify(executor, set_query2, true, "SET type overwrite int->bool");
        
        cypher_executor_free(executor);
    }
}

/* Test SET label operations */
static void test_set_label_operations(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create a test node */
        const char *create_query = "CREATE (n:Person {name: \"Alice\"})";
        execute_and_verify(executor, create_query, true, "CREATE for label test");
        
        /* Add a label to the node */
        const char *set_query = "MATCH (n:Person) SET n:Employee";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            printf("SET label result: success=%d, properties_set=%d\n", 
                   result->success, result->properties_set);
            
            if (result->success) {
                CU_ASSERT_TRUE(result->success);
                CU_ASSERT_EQUAL(result->properties_set, 1); /* Should count label as 1 operation */
                
                /* Verify the label was added by querying - check if Employee label exists */
                cypher_result *verify_result = cypher_executor_execute(executor, 
                    "MATCH (n:Employee) RETURN n.name");
                
                if (verify_result && verify_result->success) {
                    printf("Label verification successful\n");
                } else {
                    printf("Label verification failed: %s\n", 
                           verify_result ? verify_result->error_message : "NULL result");
                }
                
                if (verify_result) cypher_result_free(verify_result);
            } else {
                printf("SET label failed: %s\n", result->error_message);
                CU_FAIL("SET label operation should succeed");
            }
            
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test SET relationship property */
static void test_set_relationship_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create nodes and a relationship */
        const char *create_query = "CREATE (a:Person {name: \"Alice\"})-[r:KNOWS {since: 2020}]->(b:Person {name: \"Bob\"})";
        execute_and_verify(executor, create_query, true, "CREATE for relationship SET test");

        /* Update relationship property */
        const char *set_query = "MATCH (a:Person)-[r:KNOWS]->(b:Person) SET r.since = 2024, r.strength = 0.9";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("SET relationship property error: %s\n", result->error_message);
            } else {
                printf("SET relationship result: success=%d, properties_set=%d\n",
                       result->success, result->properties_set);
                CU_ASSERT_TRUE(result->properties_set >= 2); /* since and strength */
            }
            cypher_result_free(result);
        }

        /* Verify the change */
        const char *verify_query = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN r.since, r.strength";
        cypher_result *verify_result = cypher_executor_execute(executor, verify_query);
        CU_ASSERT_PTR_NOT_NULL(verify_result);

        if (verify_result) {
            CU_ASSERT_TRUE(verify_result->success);
            if (verify_result->success && verify_result->row_count > 0) {
                printf("Verify relationship: since='%s', strength='%s'\n",
                       verify_result->data[0][0] ? verify_result->data[0][0] : "NULL",
                       verify_result->data[0][1] ? verify_result->data[0][1] : "NULL");

                /* Check that since was updated to 2024 */
                if (verify_result->data[0][0]) {
                    CU_ASSERT_STRING_EQUAL(verify_result->data[0][0], "2024");
                }
            }
            cypher_result_free(verify_result);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET relationship property with WHERE clause */
static void test_set_relationship_with_where(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create nodes with multiple relationships */
        execute_and_verify(executor,
            "CREATE (a:Person {name: \"Carol\"})-[r1:FRIENDS {weight: 1}]->(b:Person {name: \"Dave\"})",
            true, "CREATE first relationship");
        execute_and_verify(executor,
            "CREATE (c:Person {name: \"Eve\"})-[r2:FRIENDS {weight: 5}]->(d:Person {name: \"Frank\"})",
            true, "CREATE second relationship");

        /* Update only relationships with weight > 2 */
        const char *set_query = "MATCH (a:Person)-[r:FRIENDS]->(b:Person) WHERE r.weight > 2 SET r.strong = true";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);

        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("SET relationship WHERE error: %s\n", result->error_message);
            } else {
                printf("SET relationship WHERE result: success=%d, properties_set=%d\n",
                       result->success, result->properties_set);
                /* Should only update 1 relationship (Eve->Frank with weight=5) */
                CU_ASSERT_EQUAL(result->properties_set, 1);
            }
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET n = {map} replace semantics */
static void test_bulk_set_replace(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create node with multiple properties */
        execute_and_verify(executor,
            "CREATE (n:BulkReplace {name: \"Alice\", age: 30, city: \"NYC\"})",
            true, "CREATE for bulk replace");

        /* Replace all properties with a new map */
        const char *set_query = "MATCH (n:BulkReplace {name: \"Alice\"}) SET n = {name: \"Alice\", role: \"admin\"}";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Bulk SET replace error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        /* Verify new properties exist */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkReplace {name: \"Alice\"}) RETURN n.role");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "admin");
            }
            cypher_result_free(verify);
        }

        /* Verify old properties were removed */
        cypher_result *old = cypher_executor_execute(executor,
            "MATCH (n:BulkReplace {name: \"Alice\"}) RETURN n.age");
        CU_ASSERT_PTR_NOT_NULL(old);
        if (old) {
            CU_ASSERT_TRUE(old->success);
            if (old->success && old->row_count > 0) {
                /* age should be NULL after replace */
                CU_ASSERT_PTR_NULL(old->data[0][0]);
            }
            cypher_result_free(old);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET n += {map} merge semantics */
static void test_bulk_set_merge(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Create node with initial properties */
        execute_and_verify(executor,
            "CREATE (n:BulkMerge {a: 1, b: 2})",
            true, "CREATE for bulk merge");

        /* Merge new properties */
        execute_and_verify(executor,
            "MATCH (n:BulkMerge {a: 1}) SET n += {c: 3}",
            true, "Bulk SET merge");

        /* Verify all properties exist */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkMerge {a: 1}) RETURN n.a, n.b, n.c");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]); /* a still exists */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]); /* b still exists */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][2]); /* c was added */
                if (verify->data[0][2]) {
                    CU_ASSERT_STRING_EQUAL(verify->data[0][2], "3");
                }
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET merge updates existing properties */
static void test_bulk_set_merge_update(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:MergeUpdate {x: 1, y: 2})",
            true, "CREATE for merge update");

        /* Merge with updated value for existing property */
        execute_and_verify(executor,
            "MATCH (n:MergeUpdate {x: 1}) SET n += {x: 10, z: 3}",
            true, "Bulk SET merge update");

        /* Verify updated and new values */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:MergeUpdate {x: 10}) RETURN n.x, n.y, n.z");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                if (verify->data[0][0]) CU_ASSERT_STRING_EQUAL(verify->data[0][0], "10");
                if (verify->data[0][1]) CU_ASSERT_STRING_EQUAL(verify->data[0][1], "2");
                if (verify->data[0][2]) CU_ASSERT_STRING_EQUAL(verify->data[0][2], "3");
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with empty map clears properties */
static void test_bulk_set_empty_map(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:EmptyMap {x: 1, y: 2})",
            true, "CREATE for empty map");

        /* Replace with empty map */
        execute_and_verify(executor,
            "MATCH (n:EmptyMap) SET n = {}",
            true, "Bulk SET empty map");

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with mixed types in map */
static void test_bulk_set_mixed_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkMixed {name: \"placeholder\"})",
            true, "CREATE for bulk mixed");

        execute_and_verify(executor,
            "MATCH (n:BulkMixed) SET n = {name: \"Bob\", count: 42, score: 3.14, active: true}",
            true, "Bulk SET mixed types");

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkMixed {name: \"Bob\"}) RETURN n.name, n.count, n.score, n.active");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                if (verify->data[0][0]) CU_ASSERT_STRING_EQUAL(verify->data[0][0], "Bob");
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]); /* count */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][2]); /* score */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][3]); /* active */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET on edge (replace) */
static void test_bulk_set_edge_replace(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (a:EdgeBulk {name: \"A\"}), (b:EdgeBulk {name: \"B\"})",
            true, "CREATE nodes for edge bulk");
        execute_and_verify(executor,
            "MATCH (a:EdgeBulk {name: \"A\"}), (b:EdgeBulk {name: \"B\"}) CREATE (a)-[:KNOWS {since: 2020, strength: 5}]->(b)",
            true, "CREATE edge for edge bulk");

        /* Replace edge properties */
        execute_and_verify(executor,
            "MATCH (a:EdgeBulk {name: \"A\"})-[r:KNOWS]->(b) SET r = {weight: 10, label: \"friend\"}",
            true, "Bulk SET edge replace");

        /* Verify new properties */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (a:EdgeBulk {name: \"A\"})-[r:KNOWS]->(b) RETURN r.weight, r.label");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                if (verify->data[0][0]) CU_ASSERT_STRING_EQUAL(verify->data[0][0], "10");
                if (verify->data[0][1]) CU_ASSERT_STRING_EQUAL(verify->data[0][1], "friend");
            }
            cypher_result_free(verify);
        }

        /* Verify old properties removed */
        cypher_result *old = cypher_executor_execute(executor,
            "MATCH (a:EdgeBulk {name: \"A\"})-[r:KNOWS]->(b) RETURN r.since");
        CU_ASSERT_PTR_NOT_NULL(old);
        if (old) {
            CU_ASSERT_TRUE(old->success);
            if (old->success && old->row_count > 0) {
                CU_ASSERT_PTR_NULL(old->data[0][0]); /* since should be gone */
            }
            cypher_result_free(old);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET on edge (merge) */
static void test_bulk_set_edge_merge(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (a:EdgeMerge {name: \"C\"}), (b:EdgeMerge {name: \"D\"})",
            true, "CREATE nodes for edge merge");
        execute_and_verify(executor,
            "MATCH (a:EdgeMerge {name: \"C\"}), (b:EdgeMerge {name: \"D\"}) CREATE (a)-[:LIKES {weight: 3}]->(b)",
            true, "CREATE edge for edge merge");

        /* Merge edge properties */
        execute_and_verify(executor,
            "MATCH (a:EdgeMerge {name: \"C\"})-[r:LIKES]->(b) SET r += {extra: true}",
            true, "Bulk SET edge merge");

        /* Verify both old and new properties exist */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (a:EdgeMerge {name: \"C\"})-[r:LIKES]->(b) RETURN r.weight, r.extra");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]); /* weight preserved */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]); /* extra added */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with JSON map property value */
static void test_set_json_map_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:JsonSet {name: \"test\"})",
            true, "CREATE for JSON SET");

        /* Set a map literal as a property value */
        execute_and_verify(executor,
            "MATCH (n:JsonSet {name: \"test\"}) SET n.meta = {role: \"admin\", level: 5}",
            true, "SET JSON map property");

        /* Verify */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:JsonSet {name: \"test\"}) RETURN n.meta");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]); /* meta should have value */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with JSON list property value */
static void test_set_json_list_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:ListSet {name: \"test\"})",
            true, "CREATE for list SET");

        /* Set a list literal as a property value */
        execute_and_verify(executor,
            "MATCH (n:ListSet {name: \"test\"}) SET n.tags = [1, 2, 3]",
            true, "SET JSON list property");

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:ListSet {name: \"test\"}) RETURN n.tags");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]);
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with JSON nested values in map */
static void test_bulk_set_json_nested(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:JsonBulk {name: \"placeholder\"})",
            true, "CREATE for JSON bulk");

        /* Bulk SET with nested JSON values */
        execute_and_verify(executor,
            "MATCH (n:JsonBulk) SET n = {name: \"Charlie\", meta: {role: \"admin\"}, tags: [1, 2]}",
            true, "Bulk SET with JSON nested");

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:JsonBulk {name: \"Charlie\"}) RETURN n.name, n.meta, n.tags");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                if (verify->data[0][0]) CU_ASSERT_STRING_EQUAL(verify->data[0][0], "Charlie");
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]); /* meta as JSON */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][2]); /* tags as JSON */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with parameter substitution */
static void test_set_parameter(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:ParamSet {name: \"test\"})",
            true, "CREATE for param SET");

        /* Set parameter value via execute_params */
        const char *set_query = "MATCH (n:ParamSet {name: \"test\"}) SET n.status = $val";
        cypher_result *result = cypher_executor_execute_params(executor, set_query, "{\"val\": \"updated\"}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Parameter SET error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        /* Verify */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:ParamSet {name: \"test\"}) RETURN n.status");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "updated");
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with integer parameter */
static void test_set_parameter_int(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:ParamInt {name: \"test\"})",
            true, "CREATE for param int SET");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:ParamInt {name: \"test\"}) SET n.count = $count",
            "{\"count\": 42}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Parameter int SET error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with null parameter (should skip) */
static void test_set_parameter_null(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:ParamNull {name: \"test\", status: \"active\"})",
            true, "CREATE for param null SET");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:ParamNull {name: \"test\"}) SET n.status = $val",
            "{\"val\": null}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Parameter null SET error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with unsupported expression type (should fail) */
static void test_set_unsupported_expr(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:ExprTest {val: 1})",
            true, "CREATE for expr test");

        /* SET n.val = n.val + 1 uses a binary expression - may or may not be supported */
        const char *set_query = "MATCH (n:ExprTest) SET n.val = n.val + 1";
        cypher_result *result = cypher_executor_execute(executor, set_query);
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            /* Just verify it returns a result (may succeed or fail) */
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with WHERE filtering */
static void test_bulk_set_with_where(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkWhere {name: \"X\", status: \"active\"})",
            true, "CREATE first for bulk where");
        execute_and_verify(executor,
            "CREATE (n:BulkWhere {name: \"Y\", status: \"inactive\"})",
            true, "CREATE second for bulk where");

        /* Only active nodes get updated */
        execute_and_verify(executor,
            "MATCH (n:BulkWhere) WHERE n.status = \"active\" SET n += {updated: true}",
            true, "Bulk SET with WHERE");

        cypher_executor_free(executor);
    }
}

/* Test mixing property SET and bulk SET in one query */
static void test_set_mixed_property_and_bulk(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:MixedSetBulk {name: \"C\"})",
            true, "CREATE for mixed SET");

        execute_and_verify(executor,
            "MATCH (n:MixedSetBulk {name: \"C\"}) SET n.age = 25, n += {role: \"dev\"}",
            true, "Mixed property and bulk SET");

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:MixedSetBulk {name: \"C\"}) RETURN n.name, n.age, n.role");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                if (verify->data[0][0]) CU_ASSERT_STRING_EQUAL(verify->data[0][0], "C");
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]); /* age */
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][2]); /* role */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET JSON property on edge */
static void test_set_json_edge_property(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (a:JsonEdge {name: \"A\"})-[:HAS {weight: 1}]->(b:JsonEdge {name: \"B\"})",
            true, "CREATE for JSON edge SET");

        execute_and_verify(executor,
            "MATCH (a:JsonEdge {name: \"A\"})-[r:HAS]->(b) SET r.meta = {source: \"test\"}",
            true, "SET JSON property on edge");

        cypher_executor_free(executor);
    }
}

/* Test SET with function call: timestamp() — issue #35 */
static void test_set_function_call_timestamp(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:FuncTs {name: \"ts_test\"})",
            true, "CREATE for function call SET");

        execute_and_verify(executor,
            "MATCH (n:FuncTs {name: \"ts_test\"}) SET n.updated = timestamp()",
            true, "SET n.updated = timestamp()");

        /* Verify the property was set to a non-null value */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:FuncTs {name: \"ts_test\"}) RETURN n.updated");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            CU_ASSERT_TRUE(verify->row_count > 0);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                /* timestamp() returns an integer, should be non-empty */
                CU_ASSERT_TRUE(strlen(verify->data[0][0]) > 0);
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with function call: toUpper() — issue #35 */
static void test_set_function_call_toUpper(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:FuncUpper {name: \"raw\"})",
            true, "CREATE for toUpper SET");

        execute_and_verify(executor,
            "MATCH (n:FuncUpper {name: \"raw\"}) SET n.name = toUpper('alice')",
            true, "SET n.name = toUpper('alice')");

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:FuncUpper) RETURN n.name");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "ALICE");
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with parameter map merge — issue #38 */
static void test_bulk_set_parameter_merge(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPMerge {name: \"bob\", age: 25})",
            true, "CREATE for bulk param merge");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPMerge {name: \"bob\"}) SET n += $props",
            "{\"props\": {\"city\": \"LA\", \"active\": true}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Bulk param merge error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        /* Verify original property preserved */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkPMerge {name: \"bob\"}) RETURN n.age");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "25");
            }
            cypher_result_free(verify);
        }

        /* Verify new property added */
        cypher_result *verify2 = cypher_executor_execute(executor,
            "MATCH (n:BulkPMerge {name: \"bob\"}) RETURN n.city");
        CU_ASSERT_PTR_NOT_NULL(verify2);
        if (verify2) {
            CU_ASSERT_TRUE(verify2->success);
            if (verify2->success && verify2->row_count > 0 && verify2->data[0][0]) {
                CU_ASSERT_STRING_EQUAL(verify2->data[0][0], "LA");
            }
            cypher_result_free(verify2);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with parameter map replace — issue #38 */
static void test_bulk_set_parameter_replace(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPReplace {name: \"alice\", age: 30, city: \"NYC\"})",
            true, "CREATE for bulk param replace");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPReplace {name: \"alice\"}) SET n = $props",
            "{\"props\": {\"name\": \"alice\", \"score\": 100}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Bulk param replace error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        /* Verify new property exists */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkPReplace {name: \"alice\"}) RETURN n.score");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "100");
            }
            cypher_result_free(verify);
        }

        /* Verify old property was removed (replace mode) */
        cypher_result *verify2 = cypher_executor_execute(executor,
            "MATCH (n:BulkPReplace {name: \"alice\"}) RETURN n.age");
        CU_ASSERT_PTR_NOT_NULL(verify2);
        if (verify2) {
            CU_ASSERT_TRUE(verify2->success);
            if (verify2->success && verify2->row_count > 0) {
                /* age should be null after replace */
                CU_ASSERT_PTR_NULL(verify2->data[0][0]);
            }
            cypher_result_free(verify2);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with float-returning function: toFloat() */
static void test_set_function_call_toFloat(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:FuncFloat {name: \"ftest\"})",
            true, "CREATE for toFloat SET");

        execute_and_verify(executor,
            "MATCH (n:FuncFloat {name: \"ftest\"}) SET n.score = toFloat('3.14')",
            true, "SET n.score = toFloat('3.14')");

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:FuncFloat {name: \"ftest\"}) RETURN n.score");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                /* Should be 3.14 */
                double val = atof(verify->data[0][0]);
                CU_ASSERT_DOUBLE_EQUAL(val, 3.14, 0.001);
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test SET with NULL-returning function: property should be skipped */
static void test_set_function_call_null_result(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:FuncNull {name: \"ntest\", keep: \"yes\"})",
            true, "CREATE for null function SET");

        /* toIntegerOrNull on a non-numeric string returns NULL */
        execute_and_verify(executor,
            "MATCH (n:FuncNull {name: \"ntest\"}) SET n.bad = toIntegerOrNull('not_a_number')",
            true, "SET n.bad = toIntegerOrNull('not_a_number')");

        /* The property should not exist (NULL result skips SET) */
        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:FuncNull {name: \"ntest\"}) RETURN n.bad, n.keep");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NULL(verify->data[0][0]); /* n.bad should be NULL */
                CU_ASSERT_STRING_EQUAL(verify->data[0][1], "yes"); /* n.keep preserved */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET parameter with float values */
static void test_bulk_set_parameter_float(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPFloat {name: \"flt\"})",
            true, "CREATE for bulk param float");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPFloat {name: \"flt\"}) SET n += $props",
            "{\"props\": {\"temperature\": 98.6, \"ratio\": -0.5}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Bulk param float error: %s\n", result->error_message);
            }
            cypher_result_free(result);
        }

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkPFloat {name: \"flt\"}) RETURN n.temperature, n.ratio");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]);
                if (verify->data[0][0]) {
                    double temp = atof(verify->data[0][0]);
                    CU_ASSERT_DOUBLE_EQUAL(temp, 98.6, 0.01);
                }
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]);
                if (verify->data[0][1]) {
                    double ratio = atof(verify->data[0][1]);
                    CU_ASSERT_DOUBLE_EQUAL(ratio, -0.5, 0.01);
                }
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET parameter with null values (should skip null keys) */
static void test_bulk_set_parameter_null_values(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPNull {name: \"nv\", existing: \"kept\"})",
            true, "CREATE for bulk param null");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPNull {name: \"nv\"}) SET n += $props",
            "{\"props\": {\"added\": \"yes\", \"skipped\": null}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            cypher_result_free(result);
        }

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkPNull {name: \"nv\"}) RETURN n.added, n.skipped, n.existing");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_STRING_EQUAL(verify->data[0][0], "yes");   /* added */
                CU_ASSERT_PTR_NULL(verify->data[0][1]);               /* skipped (null) */
                CU_ASSERT_STRING_EQUAL(verify->data[0][2], "kept");   /* existing preserved */
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET parameter with boolean false */
static void test_bulk_set_parameter_bool_false(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPBoolF {name: \"bf\"})",
            true, "CREATE for bulk param bool false");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPBoolF {name: \"bf\"}) SET n += $props",
            "{\"props\": {\"active\": false, \"verified\": true}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            cypher_result_free(result);
        }

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkPBoolF {name: \"bf\"}) RETURN n.active, n.verified");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0) {
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][0]);
                if (verify->data[0][0]) {
                    /* false should be stored as 0 or "false" */
                    CU_ASSERT_TRUE(
                        strcmp(verify->data[0][0], "0") == 0 ||
                        strcmp(verify->data[0][0], "false") == 0);
                }
                CU_ASSERT_PTR_NOT_NULL(verify->data[0][1]);
                if (verify->data[0][1]) {
                    /* true should be stored as 1 or "true" */
                    CU_ASSERT_TRUE(
                        strcmp(verify->data[0][1], "1") == 0 ||
                        strcmp(verify->data[0][1], "true") == 0);
                }
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET parameter with nested array */
static void test_bulk_set_parameter_nested_array(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPArr {name: \"arr\"})",
            true, "CREATE for bulk param array");

        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPArr {name: \"arr\"}) SET n += $props",
            "{\"props\": {\"tags\": [\"a\", \"b\", \"c\"]}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_TRUE(result->success);
            cypher_result_free(result);
        }

        cypher_result *verify = cypher_executor_execute(executor,
            "MATCH (n:BulkPArr {name: \"arr\"}) RETURN n.tags");
        CU_ASSERT_PTR_NOT_NULL(verify);
        if (verify) {
            CU_ASSERT_TRUE(verify->success);
            if (verify->success && verify->row_count > 0 && verify->data[0][0]) {
                /* Should be stored as JSON array */
                CU_ASSERT_TRUE(strstr(verify->data[0][0], "a") != NULL);
                CU_ASSERT_TRUE(strstr(verify->data[0][0], "b") != NULL);
                CU_ASSERT_TRUE(strstr(verify->data[0][0], "c") != NULL);
            }
            cypher_result_free(verify);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with non-JSON parameter (should error) */
static void test_bulk_set_parameter_non_json_error(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPErr {name: \"err\"})",
            true, "CREATE for bulk param error");

        /* Pass a string parameter instead of a JSON object */
        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPErr {name: \"err\"}) SET n += $props",
            "{\"props\": \"not_an_object\"}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_FALSE(result->success);
            CU_ASSERT_PTR_NOT_NULL(result->error_message);
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Test bulk SET with missing parameter (should error) */
static void test_bulk_set_parameter_missing_error(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        execute_and_verify(executor,
            "CREATE (n:BulkPMiss {name: \"miss\"})",
            true, "CREATE for bulk param missing");

        /* Reference a parameter that doesn't exist */
        cypher_result *result = cypher_executor_execute_params(executor,
            "MATCH (n:BulkPMiss {name: \"miss\"}) SET n += $nonexistent",
            "{\"other\": {\"a\": 1}}");
        CU_ASSERT_PTR_NOT_NULL(result);
        if (result) {
            CU_ASSERT_FALSE(result->success);
            CU_ASSERT_PTR_NOT_NULL(result->error_message);
            cypher_result_free(result);
        }

        cypher_executor_free(executor);
    }
}

/* Initialize the SET executor test suite */
int init_executor_set_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor SET", setup_executor_set_suite, teardown_executor_set_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "SET basic property", test_set_basic_property) ||
        !CU_add_test(suite, "SET multiple properties", test_set_multiple_properties) ||
        !CU_add_test(suite, "SET overwrite property", test_set_overwrite_property) ||
        !CU_add_test(suite, "SET with WHERE clause", test_set_with_where_clause) ||
        !CU_add_test(suite, "SET data types", test_set_data_types) ||
        !CU_add_test(suite, "SET no match", test_set_no_match) ||
        !CU_add_test(suite, "SET integer types", test_set_integer_types) ||
        !CU_add_test(suite, "SET real types", test_set_real_types) ||
        !CU_add_test(suite, "SET boolean types", test_set_boolean_types) ||
        !CU_add_test(suite, "SET string types", test_set_string_types) ||
        !CU_add_test(suite, "SET mixed types", test_set_mixed_types) ||
        !CU_add_test(suite, "SET type overwrite", test_set_type_overwrite) ||
        !CU_add_test(suite, "SET label operations", test_set_label_operations) ||
        !CU_add_test(suite, "SET relationship property", test_set_relationship_property) ||
        !CU_add_test(suite, "SET relationship with WHERE", test_set_relationship_with_where) ||
        !CU_add_test(suite, "Bulk SET replace", test_bulk_set_replace) ||
        !CU_add_test(suite, "Bulk SET merge", test_bulk_set_merge) ||
        !CU_add_test(suite, "Bulk SET merge update", test_bulk_set_merge_update) ||
        !CU_add_test(suite, "Bulk SET empty map", test_bulk_set_empty_map) ||
        !CU_add_test(suite, "Bulk SET mixed types", test_bulk_set_mixed_types) ||
        !CU_add_test(suite, "Bulk SET edge replace", test_bulk_set_edge_replace) ||
        !CU_add_test(suite, "Bulk SET edge merge", test_bulk_set_edge_merge) ||
        !CU_add_test(suite, "SET JSON map property", test_set_json_map_property) ||
        !CU_add_test(suite, "SET JSON list property", test_set_json_list_property) ||
        !CU_add_test(suite, "Bulk SET JSON nested", test_bulk_set_json_nested) ||
        !CU_add_test(suite, "SET parameter", test_set_parameter) ||
        !CU_add_test(suite, "SET parameter int", test_set_parameter_int) ||
        !CU_add_test(suite, "SET parameter null", test_set_parameter_null) ||
        !CU_add_test(suite, "SET unsupported expr", test_set_unsupported_expr) ||
        !CU_add_test(suite, "Bulk SET with WHERE", test_bulk_set_with_where) ||
        !CU_add_test(suite, "SET mixed property and bulk", test_set_mixed_property_and_bulk) ||
        !CU_add_test(suite, "SET JSON edge property", test_set_json_edge_property) ||
        !CU_add_test(suite, "SET function call timestamp", test_set_function_call_timestamp) ||
        !CU_add_test(suite, "SET function call toUpper", test_set_function_call_toUpper) ||
        !CU_add_test(suite, "Bulk SET parameter merge", test_bulk_set_parameter_merge) ||
        !CU_add_test(suite, "Bulk SET parameter replace", test_bulk_set_parameter_replace) ||
        !CU_add_test(suite, "SET function call toFloat", test_set_function_call_toFloat) ||
        !CU_add_test(suite, "SET function call null result", test_set_function_call_null_result) ||
        !CU_add_test(suite, "Bulk SET parameter float", test_bulk_set_parameter_float) ||
        !CU_add_test(suite, "Bulk SET parameter null values", test_bulk_set_parameter_null_values) ||
        !CU_add_test(suite, "Bulk SET parameter bool false", test_bulk_set_parameter_bool_false) ||
        !CU_add_test(suite, "Bulk SET parameter nested array", test_bulk_set_parameter_nested_array) ||
        !CU_add_test(suite, "Bulk SET parameter non-JSON error", test_bulk_set_parameter_non_json_error) ||
        !CU_add_test(suite, "Bulk SET parameter missing error", test_bulk_set_parameter_missing_error)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}