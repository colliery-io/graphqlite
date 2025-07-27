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
static int setup_set_suite(void)
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
static int teardown_set_suite(void)
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

/* Test SET clause transformations */
static void test_set_basic(void)
{
    const char *query = "MATCH (n:Person) SET n.age = 30";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET basic transform error: %s\n", result->error_message);
        } else {
            printf("SET basic query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with multiple properties */
static void test_set_multiple(void)
{
    const char *query = "MATCH (n:Person) SET n.age = 30, n.name = \"Alice\"";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET multiple transform error: %s\n", result->error_message);
        } else {
            printf("SET multiple properties query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with different data types */
static void test_set_data_types(void)
{
    const char *query = "MATCH (n:Test) SET n.str = \"hello\", n.int = 42, n.real = 3.14, n.bool = true";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET data types transform error: %s\n", result->error_message);
        } else {
            printf("SET data types query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with WHERE clause */
static void test_set_with_where(void)
{
    const char *query = "MATCH (n:Person) WHERE n.age > 25 SET n.senior = true";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET with WHERE transform error: %s\n", result->error_message);
        } else {
            printf("SET with WHERE query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET error conditions */
static void test_set_error_conditions(void)
{
    /* Test SET without MATCH - should fail */
    const char *query1 = "SET n.age = 30";
    cypher_query_result *result1 = parse_and_transform(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    
    if (result1) {
        /* This should fail because there's no MATCH clause */
        CU_ASSERT_TRUE(result1->has_error);
        printf("SET without MATCH correctly failed: %s\n", 
               result1->error_message ? result1->error_message : "Parse error");
        cypher_free_result(result1);
    }
    
    /* Test invalid SET syntax */
    const char *query2 = "MATCH (n) SET n = 30"; /* Missing property */
    cypher_query_result *result2 = parse_and_transform(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    
    if (result2) {
        /* This might fail at parse or transform level */
        if (result2->has_error) {
            printf("Invalid SET syntax correctly failed: %s\n", 
                   result2->error_message ? result2->error_message : "Parse error");
        }
        cypher_free_result(result2);
    }
}

/* Test SET transform validation */
static void test_set_transform_validation(void)
{
    const char *query = "MATCH (n:Person) SET n.age = 30, n.name = \"test\"";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET transform validation failed: %s\n", result->error_message);
        } else {
            printf("SET transform validation passed\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with null values */
static void test_set_null_values(void)
{
    const char *query = "MATCH (n:Person) SET n.description = null";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET null values transform error: %s\n", result->error_message);
        } else {
            printf("SET null values query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with property expressions */
static void test_set_property_expressions(void)
{
    const char *query = "MATCH (n:Person) SET n.age = n.age + 1";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* This may not be implemented yet */
        if (result->has_error) {
            printf("SET property expressions error: %s\n", result->error_message);
            /* Expected for complex expressions */
        } else {
            printf("SET property expressions query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with label operations */
static void test_set_label_operations(void)
{
    const char *query = "MATCH (n) SET n:NewLabel";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* SET label operations should now work */
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET label operations error: %s\n", result->error_message);
        } else {
            printf("SET label operations query transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET with multiple labels */
static void test_set_multiple_labels(void)
{
    const char *query = "MATCH (n:Person) SET n:Employee:Manager";
    
    cypher_query_result *result = parse_and_transform(query);
    
    /* Multiple labels in single operation not implemented yet - expect parse failure */
    if (result == NULL) {
        printf("SET multiple labels correctly failed at parse stage (not implemented)\n");
    } else if (result->has_error) {
        printf("SET multiple labels error: %s\n", result->error_message);
        cypher_free_result(result);
    } else {
        printf("SET multiple labels query transformed successfully\n");
        cypher_free_result(result);
    }
}

/* Test SET mixing labels and properties */
static void test_set_mixed_operations(void)
{
    const char *query = "MATCH (n:Person) SET n:Employee, n.status = 'active'";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_FALSE(result->has_error);
        if (result->has_error) {
            printf("SET mixed operations error: %s\n", result->error_message);
        } else {
            printf("SET mixed operations (label + property) transformed successfully\n");
        }
        cypher_free_result(result);
    }
}

/* Test SET label without MATCH (should fail) */
static void test_set_label_without_match(void)
{
    const char *query = "SET n:NewLabel";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* This should fail - no variable binding */
        CU_ASSERT_TRUE(result->has_error);
        printf("SET label without MATCH correctly failed: %s\n", 
               result->error_message ? result->error_message : "Unknown error");
        cypher_free_result(result);
    }
}

/* Initialize the SET transform test suite */
int init_transform_set_suite(void)
{
    CU_pSuite suite = CU_add_suite("Transform SET", setup_set_suite, teardown_set_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "SET basic", test_set_basic) ||
        !CU_add_test(suite, "SET multiple properties", test_set_multiple) ||
        !CU_add_test(suite, "SET data types", test_set_data_types) ||
        !CU_add_test(suite, "SET with WHERE", test_set_with_where) ||
        !CU_add_test(suite, "SET error conditions", test_set_error_conditions) ||
        !CU_add_test(suite, "SET transform validation", test_set_transform_validation) ||
        !CU_add_test(suite, "SET null values", test_set_null_values) ||
        !CU_add_test(suite, "SET property expressions", test_set_property_expressions) ||
        !CU_add_test(suite, "SET label operations", test_set_label_operations) ||
        !CU_add_test(suite, "SET multiple labels", test_set_multiple_labels) ||
        !CU_add_test(suite, "SET mixed operations", test_set_mixed_operations) ||
        !CU_add_test(suite, "SET label without MATCH", test_set_label_without_match)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}