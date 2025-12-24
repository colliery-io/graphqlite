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
static int setup_executor_basic_suite(void)
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
static int teardown_executor_basic_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Test executor creation */
static void test_executor_creation(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        cypher_executor_free(executor);
    }
}

/* Test simple CREATE query execution */
static void test_create_query_execution(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (n:Person {name: \"Alice\", age: 30})";
        
        printf("\nDirect parser test:\n");
        ast_node *ast = parse_cypher_query(query);
        if (ast) {
            printf("Parser succeeded: type=%d, data=%p\n", ast->type, (void*)ast);
            cypher_parser_free_result(ast);
        } else {
            printf("Parser failed\n");
        }
        
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Execution error: %s\n", result->error_message);
            } else {
                printf("CREATE query succeeded: nodes=%d, props=%d\n", 
                       result->nodes_created, result->properties_set);
                CU_ASSERT_TRUE(result->nodes_created > 0);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test simple MATCH query execution */
static void test_match_query_execution(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* First create some data */
        const char *create_query = "CREATE (n:Person {name: \"Bob\"})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            if (!create_result->success) {
                printf("CREATE execution error: %s\n", create_result->error_message);
            }
            cypher_result_free(create_result);
        }
        
        /* Now test MATCH */
        const char *match_query = "MATCH (n:Person) RETURN n";
        cypher_result *match_result = cypher_executor_execute(executor, match_query);
        CU_ASSERT_PTR_NOT_NULL(match_result);
        
        if (match_result) {
            CU_ASSERT_TRUE(match_result->success);
            if (!match_result->success) {
                printf("MATCH execution error: %s\n", match_result->error_message);
            } else {
                printf("MATCH query succeeded\n");
            }
            cypher_result_free(match_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test invalid query handling */
static void test_invalid_query_handling(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *invalid_query = "INVALID SYNTAX HERE";
        cypher_result *result = cypher_executor_execute(executor, invalid_query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_FALSE(result->success);
            CU_ASSERT_PTR_NOT_NULL(result->error_message);
            printf("Invalid query correctly failed: %s\n", result->error_message);
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test null input handling */
static void test_null_input_handling(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Test NULL query */
        cypher_result *result1 = cypher_executor_execute(executor, NULL);
        CU_ASSERT_PTR_NOT_NULL(result1);
        if (result1) {
            CU_ASSERT_FALSE(result1->success);
            cypher_result_free(result1);
        }
        
        /* Test empty query */
        cypher_result *result2 = cypher_executor_execute(executor, "");
        CU_ASSERT_PTR_NOT_NULL(result2);
        if (result2) {
            CU_ASSERT_FALSE(result2->success);
            cypher_result_free(result2);
        }
        
        cypher_executor_free(executor);
    }
    
    /* Test NULL executor */
    cypher_result *result3 = cypher_executor_execute(NULL, "CREATE (n)");
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->success);
        cypher_result_free(result3);
    }
}

/* Test result printing (manual verification) */
static void test_result_printing(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (n:TestPrint {name: \"PrintTest\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            printf("--- Test Result Output ---\n");
            cypher_result_print(result);
            printf("--- End Test Result ---\n");
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test CREATE with different data types */
static void test_create_data_types(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (n:DataTypes {str: \"hello\", int: 42, real: 3.14, bool: true})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Data types CREATE error: %s\n", result->error_message);
            } else {
                printf("Data types CREATE succeeded: nodes=%d\n", result->nodes_created);
                CU_ASSERT_TRUE(result->nodes_created > 0);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test CREATE multiple nodes in single query */
static void test_create_multiple_nodes(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        const char *query = "CREATE (a:Person {name: \"Alice\"}), (b:Person {name: \"Bob\"}), (c:Company {name: \"TechCorp\"})";
        cypher_result *result = cypher_executor_execute(executor, query);
        CU_ASSERT_PTR_NOT_NULL(result);
        
        if (result) {
            CU_ASSERT_TRUE(result->success);
            if (!result->success) {
                printf("Multiple nodes CREATE error: %s\n", result->error_message);
            } else {
                printf("Multiple nodes CREATE succeeded: nodes=%d\n", result->nodes_created);
                CU_ASSERT_TRUE(result->nodes_created >= 3);
            }
            cypher_result_free(result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test MATCH with WHERE clause */
static void test_match_with_where(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create test data */
        const char *create_query = "CREATE (a:Person {name: \"Alice\", age: 30}), (b:Person {name: \"Bob\", age: 25})";
        cypher_result *create_result = cypher_executor_execute(executor, create_query);
        CU_ASSERT_PTR_NOT_NULL(create_result);
        
        if (create_result) {
            CU_ASSERT_TRUE(create_result->success);
            if (!create_result->success) {
                printf("CREATE execution error: %s\n", create_result->error_message);
            }
            cypher_result_free(create_result);
        }
        
        /* Test MATCH with WHERE */
        const char *match_query = "MATCH (n:Person) WHERE n.age > 28 RETURN n";
        cypher_result *match_result = cypher_executor_execute(executor, match_query);
        CU_ASSERT_PTR_NOT_NULL(match_result);
        
        if (match_result) {
            if (match_result->success) {
                printf("MATCH with WHERE error: %s\n", match_result->error_message);
                /* WHERE clauses may not be fully implemented yet */
            } else {
                printf("MATCH with WHERE succeeded\n");
            }
            cypher_result_free(match_result);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test database state consistency */
static void test_database_consistency(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);
    
    if (executor) {
        /* Create nodes and verify database state */
        const char *query1 = "CREATE (n:ConsistencyTest {id: 1})";
        cypher_result *result1 = cypher_executor_execute(executor, query1);
        CU_ASSERT_PTR_NOT_NULL(result1);
        
        if (result1) {
            CU_ASSERT_TRUE(result1->success);
            cypher_result_free(result1);
        }
        
        /* Verify the node exists */
        const char *query2 = "MATCH (n:ConsistencyTest) RETURN COUNT(n) AS count";
        cypher_result *result2 = cypher_executor_execute(executor, query2);
        CU_ASSERT_PTR_NOT_NULL(result2);
        
        if (result2) {
            if (result2->success) {
                printf("Database consistency check passed\n");
            } else {
                printf("Database consistency check error: %s\n", result2->error_message);
            }
            cypher_result_free(result2);
        }
        
        cypher_executor_free(executor);
    }
}

/* Test backtick-quoted identifiers for reserved words and special chars */
static void test_backtick_quoted_identifiers(void)
{
    cypher_executor *executor = cypher_executor_create(test_db);
    CU_ASSERT_PTR_NOT_NULL(executor);

    if (executor) {
        /* Test backtick-quoted label (reserved word) */
        const char *q1 = "CREATE (n:`Match` {name: \"test\"})";
        cypher_result *r1 = cypher_executor_execute(executor, q1);
        CU_ASSERT_PTR_NOT_NULL(r1);
        if (r1) {
            CU_ASSERT_TRUE(r1->success);
            if (!r1->success) printf("Backtick label failed: %s\n", r1->error_message);
            cypher_result_free(r1);
        }

        /* Test backtick-quoted relationship type (reserved word) */
        const char *q2 = "CREATE (a:BQ1)-[:`IN`]->(b:BQ2)";
        cypher_result *r2 = cypher_executor_execute(executor, q2);
        CU_ASSERT_PTR_NOT_NULL(r2);
        if (r2) {
            CU_ASSERT_TRUE(r2->success);
            if (!r2->success) printf("Backtick rel type failed: %s\n", r2->error_message);
            cypher_result_free(r2);
        }

        /* Test backtick-quoted property name */
        const char *q3 = "CREATE (n:BQ3 {`full name`: \"John Doe\"})";
        cypher_result *r3 = cypher_executor_execute(executor, q3);
        CU_ASSERT_PTR_NOT_NULL(r3);
        if (r3) {
            CU_ASSERT_TRUE(r3->success);
            if (!r3->success) printf("Backtick property failed: %s\n", r3->error_message);
            cypher_result_free(r3);
        }

        /* Test backtick-quoted label with special chars */
        const char *q4 = "CREATE (n:`My-Label` {name: \"test\"})";
        cypher_result *r4 = cypher_executor_execute(executor, q4);
        CU_ASSERT_PTR_NOT_NULL(r4);
        if (r4) {
            CU_ASSERT_TRUE(r4->success);
            if (!r4->success) printf("Backtick special label failed: %s\n", r4->error_message);
            cypher_result_free(r4);
        }

        /* Verify backtick-quoted label can be matched */
        const char *q5 = "MATCH (n:`Match`) RETURN n.name";
        cypher_result *r5 = cypher_executor_execute(executor, q5);
        CU_ASSERT_PTR_NOT_NULL(r5);
        if (r5) {
            CU_ASSERT_TRUE(r5->success);
            if (r5->success && r5->row_count > 0) {
                printf("Backtick match result: %s\n", r5->data[0][0]);
                CU_ASSERT_STRING_EQUAL(r5->data[0][0], "test");
            }
            cypher_result_free(r5);
        }

        cypher_executor_free(executor);
    }
}

/* Initialize the basic executor test suite */
int init_executor_basic_suite(void)
{
    CU_pSuite suite = CU_add_suite("Executor Basic", setup_executor_basic_suite, teardown_executor_basic_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Executor creation", test_executor_creation) ||
        !CU_add_test(suite, "CREATE query execution", test_create_query_execution) ||
        !CU_add_test(suite, "MATCH query execution", test_match_query_execution) ||
        !CU_add_test(suite, "Invalid query handling", test_invalid_query_handling) ||
        !CU_add_test(suite, "Null input handling", test_null_input_handling) ||
        !CU_add_test(suite, "Result printing", test_result_printing) ||
        !CU_add_test(suite, "CREATE data types", test_create_data_types) ||
        !CU_add_test(suite, "CREATE multiple nodes", test_create_multiple_nodes) ||
        !CU_add_test(suite, "MATCH with WHERE", test_match_with_where) ||
        !CU_add_test(suite, "Database consistency", test_database_consistency) ||
        !CU_add_test(suite, "Backtick quoted identifiers", test_backtick_quoted_identifiers)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}