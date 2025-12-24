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
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nLENGTH function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test toUpper function */
    const char *query2 = "MATCH (n) RETURN toUpper(n.name)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\ntoUpper function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test toLower function */
    const char *query3 = "MATCH (n) RETURN toLower(n.name)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\ntoLower function query failed: %s\n", result3->error_message);
        }
        cypher_free_result(result3);
    }

    /* Test trim function */
    const char *query4 = "MATCH (n) RETURN trim(n.name)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_FALSE(result4->has_error);
        if (result4->has_error) {
            printf("\ntrim function query failed: %s\n", result4->error_message);
        }
        cypher_free_result(result4);
    }

    /* Test substring function with 2 args */
    const char *query5 = "MATCH (n) RETURN substring(n.name, 0)";
    cypher_query_result *result5 = parse_and_transform(query5);
    CU_ASSERT_PTR_NOT_NULL(result5);
    if (result5) {
        CU_ASSERT_FALSE(result5->has_error);
        if (result5->has_error) {
            printf("\nsubstring(2 args) query failed: %s\n", result5->error_message);
        }
        cypher_free_result(result5);
    }

    /* Test substring function with 3 args */
    const char *query6 = "MATCH (n) RETURN substring(n.name, 0, 5)";
    cypher_query_result *result6 = parse_and_transform(query6);
    CU_ASSERT_PTR_NOT_NULL(result6);
    if (result6) {
        CU_ASSERT_FALSE(result6->has_error);
        if (result6->has_error) {
            printf("\nsubstring(3 args) query failed: %s\n", result6->error_message);
        }
        cypher_free_result(result6);
    }

    /* Test replace function */
    const char *query7 = "MATCH (n) RETURN replace(n.name, \"a\", \"b\")";
    cypher_query_result *result7 = parse_and_transform(query7);
    CU_ASSERT_PTR_NOT_NULL(result7);
    if (result7) {
        CU_ASSERT_FALSE(result7->has_error);
        if (result7->has_error) {
            printf("\nreplace function query failed: %s\n", result7->error_message);
        }
        cypher_free_result(result7);
    }

    /* Test left function */
    const char *query8 = "MATCH (n) RETURN left(n.name, 5)";
    cypher_query_result *result8 = parse_and_transform(query8);
    CU_ASSERT_PTR_NOT_NULL(result8);
    if (result8) {
        CU_ASSERT_FALSE(result8->has_error);
        if (result8->has_error) {
            printf("\nleft function query failed: %s\n", result8->error_message);
        }
        cypher_free_result(result8);
    }

    /* Test right function */
    const char *query9 = "MATCH (n) RETURN right(n.name, 5)";
    cypher_query_result *result9 = parse_and_transform(query9);
    CU_ASSERT_PTR_NOT_NULL(result9);
    if (result9) {
        CU_ASSERT_FALSE(result9->has_error);
        if (result9->has_error) {
            printf("\nright function query failed: %s\n", result9->error_message);
        }
        cypher_free_result(result9);
    }
}

/* Test pattern matching functions */
static void test_pattern_match_functions(void)
{
    /* Test startsWith function */
    const char *query1 = "MATCH (n) WHERE startsWith(n.name, \"A\") RETURN n";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nstartsWith function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test endsWith function */
    const char *query2 = "MATCH (n) WHERE endsWith(n.name, \"z\") RETURN n";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nendsWith function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test contains function */
    const char *query3 = "MATCH (n) WHERE contains(n.name, \"test\") RETURN n";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\ncontains function query failed: %s\n", result3->error_message);
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
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nABS function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test ROUND function with 1 arg */
    const char *query2 = "MATCH (n) RETURN round(n.price)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nROUND(1 arg) function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test ROUND function with 2 args */
    const char *query3 = "MATCH (n) RETURN round(n.price, 2)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\nROUND(2 args) function query failed: %s\n", result3->error_message);
        }
        cypher_free_result(result3);
    }

    /* Test CEIL function */
    const char *query4 = "MATCH (n) RETURN ceil(n.value)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_FALSE(result4->has_error);
        if (result4->has_error) {
            printf("\nCEIL function query failed: %s\n", result4->error_message);
        }
        cypher_free_result(result4);
    }

    /* Test FLOOR function */
    const char *query5 = "MATCH (n) RETURN floor(n.value)";
    cypher_query_result *result5 = parse_and_transform(query5);
    CU_ASSERT_PTR_NOT_NULL(result5);
    if (result5) {
        CU_ASSERT_FALSE(result5->has_error);
        if (result5->has_error) {
            printf("\nFLOOR function query failed: %s\n", result5->error_message);
        }
        cypher_free_result(result5);
    }

    /* Test SIGN function */
    const char *query6 = "MATCH (n) RETURN sign(n.value)";
    cypher_query_result *result6 = parse_and_transform(query6);
    CU_ASSERT_PTR_NOT_NULL(result6);
    if (result6) {
        CU_ASSERT_FALSE(result6->has_error);
        if (result6->has_error) {
            printf("\nSIGN function query failed: %s\n", result6->error_message);
        }
        cypher_free_result(result6);
    }

    /* Test SQRT function */
    const char *query7 = "MATCH (n) RETURN sqrt(n.value)";
    cypher_query_result *result7 = parse_and_transform(query7);
    CU_ASSERT_PTR_NOT_NULL(result7);
    if (result7) {
        CU_ASSERT_FALSE(result7->has_error);
        if (result7->has_error) {
            printf("\nSQRT function query failed: %s\n", result7->error_message);
        }
        cypher_free_result(result7);
    }

    /* Test LOG function */
    const char *query8 = "MATCH (n) RETURN log(n.value)";
    cypher_query_result *result8 = parse_and_transform(query8);
    CU_ASSERT_PTR_NOT_NULL(result8);
    if (result8) {
        CU_ASSERT_FALSE(result8->has_error);
        if (result8->has_error) {
            printf("\nLOG function query failed: %s\n", result8->error_message);
        }
        cypher_free_result(result8);
    }

    /* Test EXP function */
    const char *query9 = "MATCH (n) RETURN exp(n.value)";
    cypher_query_result *result9 = parse_and_transform(query9);
    CU_ASSERT_PTR_NOT_NULL(result9);
    if (result9) {
        CU_ASSERT_FALSE(result9->has_error);
        if (result9->has_error) {
            printf("\nEXP function query failed: %s\n", result9->error_message);
        }
        cypher_free_result(result9);
    }
}

/* Test trigonometric functions */
static void test_trig_functions(void)
{
    /* Test SIN function */
    const char *query1 = "MATCH (n) RETURN sin(n.angle)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nSIN function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test COS function */
    const char *query2 = "MATCH (n) RETURN cos(n.angle)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nCOS function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test TAN function */
    const char *query3 = "MATCH (n) RETURN tan(n.angle)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\nTAN function query failed: %s\n", result3->error_message);
        }
        cypher_free_result(result3);
    }

    /* Test ASIN function */
    const char *query4 = "MATCH (n) RETURN asin(n.value)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_FALSE(result4->has_error);
        if (result4->has_error) {
            printf("\nASIN function query failed: %s\n", result4->error_message);
        }
        cypher_free_result(result4);
    }

    /* Test ACOS function */
    const char *query5 = "MATCH (n) RETURN acos(n.value)";
    cypher_query_result *result5 = parse_and_transform(query5);
    CU_ASSERT_PTR_NOT_NULL(result5);
    if (result5) {
        CU_ASSERT_FALSE(result5->has_error);
        if (result5->has_error) {
            printf("\nACOS function query failed: %s\n", result5->error_message);
        }
        cypher_free_result(result5);
    }

    /* Test ATAN function */
    const char *query6 = "MATCH (n) RETURN atan(n.value)";
    cypher_query_result *result6 = parse_and_transform(query6);
    CU_ASSERT_PTR_NOT_NULL(result6);
    if (result6) {
        CU_ASSERT_FALSE(result6->has_error);
        if (result6->has_error) {
            printf("\nATAN function query failed: %s\n", result6->error_message);
        }
        cypher_free_result(result6);
    }
}

/* Test utility functions */
static void test_utility_functions(void)
{
    /* Test PI function */
    const char *query1 = "MATCH (n) RETURN pi()";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nPI function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test E function */
    const char *query2 = "MATCH (n) RETURN e()";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nE function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test RAND function */
    const char *query3 = "MATCH (n) RETURN rand()";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\nRAND function query failed: %s\n", result3->error_message);
        }
        cypher_free_result(result3);
    }

    /* Test COALESCE function */
    const char *query4 = "MATCH (n) RETURN coalesce(n.name, \"default\")";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_FALSE(result4->has_error);
        if (result4->has_error) {
            printf("\nCOALESCE function query failed: %s\n", result4->error_message);
        }
        cypher_free_result(result4);
    }

    /* Test toString function */
    const char *query5 = "MATCH (n) RETURN toString(n.age)";
    cypher_query_result *result5 = parse_and_transform(query5);
    CU_ASSERT_PTR_NOT_NULL(result5);
    if (result5) {
        CU_ASSERT_FALSE(result5->has_error);
        if (result5->has_error) {
            printf("\ntoString function query failed: %s\n", result5->error_message);
        }
        cypher_free_result(result5);
    }

    /* Test toInteger function */
    const char *query6 = "MATCH (n) RETURN toInteger(n.value)";
    cypher_query_result *result6 = parse_and_transform(query6);
    CU_ASSERT_PTR_NOT_NULL(result6);
    if (result6) {
        CU_ASSERT_FALSE(result6->has_error);
        if (result6->has_error) {
            printf("\ntoInteger function query failed: %s\n", result6->error_message);
        }
        cypher_free_result(result6);
    }

    /* Test toFloat function */
    const char *query7 = "MATCH (n) RETURN toFloat(n.value)";
    cypher_query_result *result7 = parse_and_transform(query7);
    CU_ASSERT_PTR_NOT_NULL(result7);
    if (result7) {
        CU_ASSERT_FALSE(result7->has_error);
        if (result7->has_error) {
            printf("\ntoFloat function query failed: %s\n", result7->error_message);
        }
        cypher_free_result(result7);
    }
}

/* Test entity introspection functions (id, labels, properties, keys) */
static void test_entity_functions(void)
{
    /* Test id() function on node */
    const char *query1 = "MATCH (n) RETURN id(n)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nid(n) function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test id() function on relationship */
    const char *query2 = "MATCH ()-[r]->() RETURN id(r)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nid(r) function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test labels() function */
    const char *query3 = "MATCH (n) RETURN labels(n)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\nlabels() function query failed: %s\n", result3->error_message);
        }
        cypher_free_result(result3);
    }

    /* Test properties() function on node */
    const char *query4 = "MATCH (n) RETURN properties(n)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_FALSE(result4->has_error);
        if (result4->has_error) {
            printf("\nproperties(n) function query failed: %s\n", result4->error_message);
        }
        cypher_free_result(result4);
    }

    /* Test properties() function on relationship */
    const char *query5 = "MATCH ()-[r]->() RETURN properties(r)";
    cypher_query_result *result5 = parse_and_transform(query5);
    CU_ASSERT_PTR_NOT_NULL(result5);
    if (result5) {
        CU_ASSERT_FALSE(result5->has_error);
        if (result5->has_error) {
            printf("\nproperties(r) function query failed: %s\n", result5->error_message);
        }
        cypher_free_result(result5);
    }

    /* Test keys() function on node */
    const char *query6 = "MATCH (n) RETURN keys(n)";
    cypher_query_result *result6 = parse_and_transform(query6);
    CU_ASSERT_PTR_NOT_NULL(result6);
    if (result6) {
        CU_ASSERT_FALSE(result6->has_error);
        if (result6->has_error) {
            printf("\nkeys(n) function query failed: %s\n", result6->error_message);
        }
        cypher_free_result(result6);
    }

    /* Test keys() function on relationship */
    const char *query7 = "MATCH ()-[r]->() RETURN keys(r)";
    cypher_query_result *result7 = parse_and_transform(query7);
    CU_ASSERT_PTR_NOT_NULL(result7);
    if (result7) {
        CU_ASSERT_FALSE(result7->has_error);
        if (result7->has_error) {
            printf("\nkeys(r) function query failed: %s\n", result7->error_message);
        }
        cypher_free_result(result7);
    }
}

/* Test relationship endpoint functions (startNode, endNode) */
static void test_relationship_endpoint_functions(void)
{
    /* Test startNode() function */
    const char *query1 = "MATCH ()-[r]->() RETURN startNode(r)";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nstartNode() function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test endNode() function */
    const char *query2 = "MATCH ()-[r]->() RETURN endNode(r)";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nendNode() function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test startNode() with node should fail */
    const char *query3 = "MATCH (n) RETURN startNode(n)";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_TRUE(result3->has_error);
        if (!result3->has_error) {
            printf("\nstartNode(n) should have failed for node variable\n");
        }
        cypher_free_result(result3);
    }

    /* Test labels() with relationship should fail */
    const char *query4 = "MATCH ()-[r]->() RETURN labels(r)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_TRUE(result4->has_error);
        if (!result4->has_error) {
            printf("\nlabels(r) should have failed for relationship variable\n");
        }
        cypher_free_result(result4);
    }
}

/* Test list functions: head, tail, last, range, collect */
static void test_list_functions(void)
{
    /* Test head() function - need a list expression, use labels() which returns a list */
    const char *query1 = "MATCH (n) RETURN head(labels(n))";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\nhead() function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test last() function */
    const char *query2 = "MATCH (n) RETURN last(labels(n))";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nlast() function query failed: %s\n", result2->error_message);
        }
        cypher_free_result(result2);
    }

    /* Test tail() function */
    const char *query3 = "MATCH (n) RETURN tail(labels(n))";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_FALSE(result3->has_error);
        if (result3->has_error) {
            printf("\ntail() function query failed: %s\n", result3->error_message);
        }
        cypher_free_result(result3);
    }

    /* Test range() function with 2 arguments */
    const char *query4 = "MATCH (n) RETURN range(1, 5)";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_FALSE(result4->has_error);
        if (result4->has_error) {
            printf("\nrange(1, 5) function query failed: %s\n", result4->error_message);
        }
        cypher_free_result(result4);
    }

    /* Test range() function with 3 arguments (step) */
    const char *query5 = "MATCH (n) RETURN range(0, 10, 2)";
    cypher_query_result *result5 = parse_and_transform(query5);
    CU_ASSERT_PTR_NOT_NULL(result5);
    if (result5) {
        CU_ASSERT_FALSE(result5->has_error);
        if (result5->has_error) {
            printf("\nrange(0, 10, 2) function query failed: %s\n", result5->error_message);
        }
        cypher_free_result(result5);
    }

    /* Test collect() function */
    const char *query6 = "MATCH (n) RETURN collect(n.name)";
    cypher_query_result *result6 = parse_and_transform(query6);
    CU_ASSERT_PTR_NOT_NULL(result6);
    if (result6) {
        CU_ASSERT_FALSE(result6->has_error);
        if (result6->has_error) {
            printf("\ncollect() function query failed: %s\n", result6->error_message);
        }
        cypher_free_result(result6);
    }
}

/* Test utility functions: timestamp, randomUUID */
static void test_timestamp_uuid_functions(void)
{
    /* Test timestamp() function */
    const char *query1 = "MATCH (n) RETURN timestamp()";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_FALSE(result1->has_error);
        if (result1->has_error) {
            printf("\ntimestamp() function query failed: %s\n", result1->error_message);
        }
        cypher_free_result(result1);
    }

    /* Test randomUUID() function */
    const char *query2 = "MATCH (n) RETURN randomUUID()";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_FALSE(result2->has_error);
        if (result2->has_error) {
            printf("\nrandomUUID() function query failed: %s\n", result2->error_message);
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

/* Test list comprehension transform */
static void test_list_comprehension(void)
{
    /* Test basic list comprehension: [x IN [1,2,3]] */
    const char *query1 = "RETURN [x IN [1, 2, 3]]";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        if (result1->has_error) {
            printf("\nList comprehension basic failed: %s\n",
                   result1->error_message ? result1->error_message : "Unknown error");
        }
        CU_ASSERT_FALSE(result1->has_error);
        cypher_free_result(result1);
    }

    /* Test list comprehension with WHERE filter: [x IN [1,2,3] WHERE x > 1] */
    const char *query2 = "RETURN [x IN [1, 2, 3] WHERE x > 1]";
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        if (result2->has_error) {
            printf("\nList comprehension with WHERE failed: %s\n",
                   result2->error_message ? result2->error_message : "Unknown error");
        }
        CU_ASSERT_FALSE(result2->has_error);
        cypher_free_result(result2);
    }

    /* Test list comprehension with transform: [x IN [1,2,3] | x * 2] */
    const char *query3 = "RETURN [x IN [1, 2, 3] | x * 2]";
    cypher_query_result *result3 = parse_and_transform(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        if (result3->has_error) {
            printf("\nList comprehension with transform failed: %s\n",
                   result3->error_message ? result3->error_message : "Unknown error");
        }
        CU_ASSERT_FALSE(result3->has_error);
        cypher_free_result(result3);
    }

    /* Test list comprehension with WHERE and transform: [x IN [1,2,3] WHERE x > 1 | x * 2] */
    const char *query4 = "RETURN [x IN [1, 2, 3] WHERE x > 1 | x * 2]";
    cypher_query_result *result4 = parse_and_transform(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        if (result4->has_error) {
            printf("\nList comprehension with WHERE and transform failed: %s\n",
                   result4->error_message ? result4->error_message : "Unknown error");
        }
        CU_ASSERT_FALSE(result4->has_error);
        cypher_free_result(result4);
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
        !CU_add_test(suite, "Pattern match functions", test_pattern_match_functions) ||
        !CU_add_test(suite, "Math functions", test_math_functions) ||
        !CU_add_test(suite, "Trigonometric functions", test_trig_functions) ||
        !CU_add_test(suite, "Utility functions", test_utility_functions) ||
        !CU_add_test(suite, "Entity introspection functions", test_entity_functions) ||
        !CU_add_test(suite, "Relationship endpoint functions", test_relationship_endpoint_functions) ||
        !CU_add_test(suite, "List functions", test_list_functions) ||
        !CU_add_test(suite, "Timestamp and UUID functions", test_timestamp_uuid_functions) ||
        !CU_add_test(suite, "Function error handling", test_function_error_handling) ||
        !CU_add_test(suite, "Multiple relationship types transform", test_multiple_relationship_types_transform) ||
        !CU_add_test(suite, "List comprehension", test_list_comprehension)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}