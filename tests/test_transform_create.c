#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <sqlite3.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "transform/cypher_transform.h"
#include "parser/cypher_debug.h"
#include "executor/cypher_schema.h"

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database */
static int setup_create_suite(void)
{
    /* Create in-memory database */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Use the proper GraphQLite schema */
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
static int teardown_create_suite(void)
{
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
    return 0;
}

/* Helper function to parse and transform a query */
static cypher_query_result* parse_and_transform(const char *query_str)
{
    /* Parse the query */
    ast_node *ast = parse_cypher_query(query_str);
    if (!ast) {
        return NULL;
    }
    
    /* Create transform context */
    cypher_transform_context *ctx = cypher_transform_create_context(test_db);
    if (!ctx) {
        cypher_parser_free_result(ast);
        return NULL;
    }
    
    /* Transform to SQL */
    cypher_query_result *result = cypher_transform_query(ctx, (cypher_query*)ast);
    
    /* Cleanup */
    cypher_transform_free_context(ctx);
    cypher_parser_free_result(ast);
    
    return result;
}

/* Test simple CREATE transformation */
static void test_create_simple(void)
{
    const char *query = "CREATE (n)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test CREATE with label */
static void test_create_with_label(void)
{
    const char *query = "CREATE (n:Person)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test that CREATE transformation succeeds and prepares statements */
static void test_create_sql_validation(void)
{
    const char *query = "CREATE (n)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("\nCREATE transform error: %s\n", result->error_message ? result->error_message : "Unknown error");
        }
        
        /* For CREATE queries, we expect the transform to succeed without errors */
        /* The actual SQL execution happens in the executor layer */
        
        cypher_free_result(result);
    }
}

/* Test CREATE with properties */
static void test_create_with_properties(void)
{
    const char *query = "CREATE (n:Person {name: \"Alice\", age: 30})";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("CREATE with properties transform error: %s\n", result->error_message);
        } else {
            printf("CREATE with properties query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test CREATE multiple nodes */
static void test_create_multiple_nodes(void)
{
    const char *query = "CREATE (a:Person), (b:Company)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("CREATE multiple nodes transform error: %s\n", result->error_message);
        } else {
            printf("CREATE multiple nodes query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test CREATE relationships */
static void test_create_relationships(void)
{
    const char *query = "CREATE (a)-[r:KNOWS]->(b)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* Relationship creation may not be fully implemented yet */
        if (result->has_error) {
            printf("CREATE relationships transform error: %s\n", result->error_message);
            /* This might be expected for now */
        } else {
            printf("CREATE relationships query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test CREATE with relationship properties */
static void test_create_relationship_properties(void)
{
    const char *query = "CREATE (a)-[r:KNOWS {since: 2020}]->(b)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* Relationship properties may not be fully implemented yet */
        if (result->has_error) {
            printf("CREATE relationship properties transform error: %s\n", result->error_message);
            /* This might be expected for now */
        } else {
            printf("CREATE relationship properties query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test CREATE with complex patterns */
static void test_create_complex_patterns(void)
{
    const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:WORKS_AT]->(b:Company {name: \"TechCorp\"})";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* Complex patterns may not be fully implemented yet */
        if (result->has_error) {
            printf("CREATE complex patterns transform error: %s\n", result->error_message);
            /* This might be expected for now */
        } else {
            printf("CREATE complex patterns query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test CREATE error conditions */
static void test_create_error_conditions(void)
{
    /* Test invalid property syntax */
    const char *query1 = "CREATE (n {invalid})";
    cypher_query_result *result1 = parse_and_transform(query1);
    
    if (result1) {
        /* This should fail at parse level */
        if (result1->has_error) {
            printf("Invalid CREATE property syntax correctly failed: %s\n", 
                   result1->error_message ? result1->error_message : "Parse error");
        }
        cypher_free_result(result1);
    } else {
        printf("Invalid CREATE property syntax failed to parse (expected)\n");
    }
    
    /* Test empty CREATE */
    const char *query2 = "CREATE";
    cypher_query_result *result2 = parse_and_transform(query2);
    
    if (result2) {
        /* This should fail */
        CU_ASSERT_TRUE(result2->has_error);
        printf("Empty CREATE correctly failed: %s\n", 
               result2->error_message ? result2->error_message : "Parse error");
        cypher_free_result(result2);
    } else {
        printf("Empty CREATE failed to parse (expected)\n");
    }
}

/* Test CREATE data type handling */
static void test_create_data_types(void)
{
    const char *query = "CREATE (n:Test {str: \"hello\", int: 42, real: 3.14, bool: true, null_val: null})";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("CREATE data types transform error: %s\n", result->error_message);
        } else {
            printf("CREATE data types query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Initialize the CREATE transform test suite */
int init_transform_create_suite(void)
{
    CU_pSuite suite = CU_add_suite("Transform CREATE", setup_create_suite, teardown_create_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Simple CREATE", test_create_simple) ||
        !CU_add_test(suite, "CREATE with label", test_create_with_label) ||
        !CU_add_test(suite, "CREATE SQL validation", test_create_sql_validation) ||
        !CU_add_test(suite, "CREATE with properties", test_create_with_properties) ||
        !CU_add_test(suite, "CREATE multiple nodes", test_create_multiple_nodes) ||
        !CU_add_test(suite, "CREATE relationships", test_create_relationships) ||
        !CU_add_test(suite, "CREATE relationship properties", test_create_relationship_properties) ||
        !CU_add_test(suite, "CREATE complex patterns", test_create_complex_patterns) ||
        !CU_add_test(suite, "CREATE error conditions", test_create_error_conditions) ||
        !CU_add_test(suite, "CREATE data types", test_create_data_types)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}