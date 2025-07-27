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
                
                /* Verify the label was added by querying */
                cypher_result *verify_result = cypher_executor_execute(executor, 
                    "MATCH (n:Person:Employee) RETURN n.name");
                
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
        !CU_add_test(suite, "SET label operations", test_set_label_operations)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}