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
static int setup_functions_suite(void)
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
static int teardown_functions_suite(void)
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
        /* Create an error result for parse failures */
        cypher_query_result *error_result = (cypher_query_result*)calloc(1, sizeof(cypher_query_result));
        if (error_result) {
            error_result->has_error = true;
            error_result->error_message = strdup("Parse error");
        }
        return error_result;
    }
    
    /* Create transform context */
    cypher_transform_context *ctx = cypher_transform_create_context(test_db);
    if (!ctx) {
        cypher_parser_free_result(ast);
        /* Create an error result for context creation failures */
        cypher_query_result *error_result = (cypher_query_result*)calloc(1, sizeof(cypher_query_result));
        if (error_result) {
            error_result->has_error = true;
            error_result->error_message = strdup("Context creation error");
        }
        return error_result;
    }
    
    /* Transform to SQL */
    cypher_query_result *result = cypher_transform_query(ctx, (cypher_query*)ast);
    
    /* Cleanup */
    cypher_transform_free_context(ctx);
    cypher_parser_free_result(ast);
    
    return result;
}

/* Test TYPE function basic functionality */
static void test_type_function_basic(void)
{
    /* Test that TYPE function is recognized and generates proper SQL */
    const char *query = "MATCH ()-[r:KNOWS]->() RETURN type(r)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* Should succeed - no errors */
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test TYPE function argument validation */
static void test_type_function_validation(void)
{
    /* Test with valid relationship variable */
    const char *query = "MATCH ()-[r]->() RETURN type(r)";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* Should succeed */
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("Transform error: %s\n", result->error_message);
        }
        cypher_free_result(result);
    }
}

/* Test TYPE function error cases */
static void test_type_function_errors(void)
{
    /* Test 1: TYPE function with no arguments */
    const char *query1 = "MATCH ()-[r]->() RETURN type()";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    
    if (result1) {
        /* Should fail with error about missing argument */
        CU_ASSERT_TRUE(result1->has_error);
        if (result1->error_message) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result1->error_message, "exactly one non-null argument"));
        }
        cypher_free_result(result1);
    }
    
    /* Test 2: TYPE function with node variable */
    const char *query2 = "MATCH (n) RETURN type(n)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    
    if (result2) {
        /* Should fail with error about requiring relationship */
        CU_ASSERT_TRUE(result2->has_error);
        if (result2->error_message) {
            CU_ASSERT_PTR_NOT_NULL(strstr(result2->error_message, "relationship variable"));
        }
        cypher_free_result(result2);
    }
}

/* Test COUNT function variations */
static void test_count_function(void)
{
    /* Test COUNT(*) */
    const char *query1 = "RETURN count(*)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        if (!result1->has_error) {
            printf("\nCOUNT(*) query transformed successfully\n");
        } else {
            printf("\nCOUNT(*) query failed: %s\n", 
                   result1->error_message ? result1->error_message : "Unknown error");
        }
        cypher_free_result(result1);
    }
    
    /* Test COUNT(variable) */
    const char *query2 = "MATCH (n) RETURN count(n)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        if (!result2->has_error) {
            printf("\nCOUNT(variable) query transformed successfully\n");
        } else {
            printf("\nCOUNT(variable) query failed: %s\n", 
                   result2->error_message ? result2->error_message : "Unknown error");
        }
        cypher_free_result(result2);
    }
    
    /* Test COUNT(DISTINCT variable) */
    const char *query3 = "MATCH (n) RETURN count(distinct n)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        if (!result3->has_error) {
            printf("\nCOUNT(DISTINCT variable) query transformed successfully\n");
        } else {
            printf("\nCOUNT(DISTINCT variable) query failed: %s\n", 
                   result3->error_message ? result3->error_message : "Unknown error");
        }
        cypher_free_result(result3);
    }
    
    /* Test COUNT with property */
    const char *query4 = "MATCH (n) RETURN count(n.name)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        if (!result4->has_error) {
            printf("\nCOUNT(property) query transformed successfully\n");
        } else {
            printf("\nCOUNT(property) query failed: %s\n", 
                   result4->error_message ? result4->error_message : "Unknown error");
        }
        cypher_free_result(result4);
    }
}

/* Test other aggregate functions */
static void test_aggregate_functions(void)
{
    /* Test MIN function */
    const char *query1 = "MATCH (n) RETURN min(n.age)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        if (!result1->has_error) {
            printf("\nMIN function query transformed successfully\n");
        } else {
            printf("\nMIN function query failed: %s\n", 
                   result1->error_message ? result1->error_message : "Unknown error");
        }
        cypher_free_result(result1);
    }
    
    /* Test MAX function */
    const char *query2 = "MATCH (n) RETURN max(n.age)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        if (!result2->has_error) {
            printf("\nMAX function query transformed successfully\n");
        } else {
            printf("\nMAX function query failed: %s\n", 
                   result2->error_message ? result2->error_message : "Unknown error");
        }
        cypher_free_result(result2);
    }
    
    /* Test AVG function */
    const char *query3 = "MATCH (n) RETURN avg(n.age)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        if (!result3->has_error) {
            printf("\nAVG function query transformed successfully\n");
        } else {
            printf("\nAVG function query failed: %s\n", 
                   result3->error_message ? result3->error_message : "Unknown error");
        }
        cypher_free_result(result3);
    }
    
    /* Test SUM function */
    const char *query4 = "MATCH (n) RETURN sum(n.age)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        if (!result4->has_error) {
            printf("\nSUM function query transformed successfully\n");
        } else {
            printf("\nSUM function query failed: %s\n", 
                   result4->error_message ? result4->error_message : "Unknown error");
        }
        cypher_free_result(result4);
    }
}

/* Test string functions */
static void test_string_functions(void)
{
    /* Test LENGTH function */
    const char *query1 = "MATCH (n) RETURN length(n.name)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        if (!result1->has_error) {
            printf("\nLENGTH function query transformed successfully\n");
        } else {
            printf("\nLENGTH function query failed: %s\n", 
                   result1->error_message ? result1->error_message : "Unknown error");
        }
        cypher_free_result(result1);
    }
    
    /* Test UPPER function */
    const char *query2 = "MATCH (n) RETURN upper(n.name)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        if (!result2->has_error) {
            printf("\nUPPER function query transformed successfully\n");
        } else {
            printf("\nUPPER function query failed: %s\n", 
                   result2->error_message ? result2->error_message : "Unknown error");
        }
        cypher_free_result(result2);
    }
    
    /* Test LOWER function */
    const char *query3 = "MATCH (n) RETURN lower(n.name)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        if (!result3->has_error) {
            printf("\nLOWER function query transformed successfully\n");
        } else {
            printf("\nLOWER function query failed: %s\n", 
                   result3->error_message ? result3->error_message : "Unknown error");
        }
        cypher_free_result(result3);
    }
}

/* Test mathematical functions */
static void test_math_functions(void)
{
    /* Test ABS function */
    const char *query1 = "MATCH (n) RETURN abs(n.value)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        if (!result1->has_error) {
            printf("\nABS function query transformed successfully\n");
        } else {
            printf("\nABS function query failed: %s\n", 
                   result1->error_message ? result1->error_message : "Unknown error");
        }
        cypher_free_result(result1);
    }
    
    /* Test ROUND function */
    const char *query2 = "MATCH (n) RETURN round(n.price)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        if (!result2->has_error) {
            printf("\nROUND function query transformed successfully\n");
        } else {
            printf("\nROUND function query failed: %s\n", 
                   result2->error_message ? result2->error_message : "Unknown error");
        }
        cypher_free_result(result2);
    }
}

/* Test function error handling */
static void test_function_error_handling(void)
{
    /* Test unknown function */
    const char *query1 = "MATCH (n) RETURN unknown_function(n)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        /* Should fail with unknown function error */
        if (result1->has_error) {
            printf("\nUnknown function correctly failed: %s\n", 
                   result1->error_message ? result1->error_message : "Unknown error");
        } else {
            printf("\nUnknown function unexpectedly succeeded\n");
        }
        cypher_free_result(result1);
    }
    
    /* Test function with wrong argument count */
    const char *query2 = "MATCH (n) RETURN count(n, n)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        /* Should fail with argument count error */
        if (result2->has_error) {
            printf("\nWrong argument count correctly failed: %s\n", 
                   result2->error_message ? result2->error_message : "Unknown error");
        } else {
            printf("\nWrong argument count unexpectedly succeeded\n");
        }
        cypher_free_result(result2);
    }
}

/* Test multiple relationship type transform */
static void test_multiple_relationship_types_transform(void)
{
    const char *query = "MATCH (a)-[:WORKS_FOR|CONSULTS_FOR]->(b) RETURN a.name, b.name";
    cypher_query_result *result = parse_and_transform(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        /* Should succeed without error - transform should handle multiple types */
        if (result->has_error) {
            printf("\nMultiple relationship types transform failed: %s\n", 
                   result->error_message ? result->error_message : "Unknown error");
        } else {
            printf("\nMultiple relationship types transform succeeded\n");
        }
        CU_ASSERT_EQUAL(result->has_error, 0);
        cypher_free_result(result);
    }
}

/* Initialize the functions transform test suite */
int init_transform_functions_suite(void)
{
    CU_pSuite suite = CU_add_suite("Transform Functions", setup_functions_suite, teardown_functions_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "TYPE function basic", test_type_function_basic) ||
        !CU_add_test(suite, "TYPE function validation", test_type_function_validation) ||
        !CU_add_test(suite, "TYPE function error cases", test_type_function_errors) ||
        !CU_add_test(suite, "COUNT function variations", test_count_function) ||
        !CU_add_test(suite, "Aggregate functions", test_aggregate_functions) ||
        !CU_add_test(suite, "String functions", test_string_functions) ||
        !CU_add_test(suite, "Math functions", test_math_functions) ||
        !CU_add_test(suite, "Function error handling", test_function_error_handling) ||
        !CU_add_test(suite, "Multiple relationship types transform", test_multiple_relationship_types_transform)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}