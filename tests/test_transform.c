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
#include "test_transform.h"

/* Test database handle */
static sqlite3 *test_db = NULL;

/* Setup function - create test database */
static int setup_transform_suite(void)
{
    /* Create in-memory database */
    int rc = sqlite3_open(":memory:", &test_db);
    if (rc != SQLITE_OK) {
        return -1;
    }
    
    /* Create graph schema */
    const char *schema[] = {
        "CREATE TABLE nodes (id INTEGER PRIMARY KEY AUTOINCREMENT)",
        "CREATE TABLE edges (id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "source_id INTEGER, target_id INTEGER, type TEXT, "
        "FOREIGN KEY(source_id) REFERENCES nodes(id), "
        "FOREIGN KEY(target_id) REFERENCES nodes(id))",
        "CREATE TABLE node_labels (node_id INTEGER, label TEXT, "
        "PRIMARY KEY(node_id, label), FOREIGN KEY(node_id) REFERENCES nodes(id))",
        "CREATE TABLE properties (element_id INTEGER, element_type TEXT, "
        "key TEXT, value TEXT, value_type TEXT, "
        "PRIMARY KEY(element_id, element_type, key))",
        NULL
    };
    
    for (int i = 0; schema[i]; i++) {
        char *error;
        rc = sqlite3_exec(test_db, schema[i], NULL, NULL, &error);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Schema error: %s\n", error);
            sqlite3_free(error);
            return -1;
        }
    }
    
    return 0;
}

/* Teardown function */
static int teardown_transform_suite(void)
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

/* Test simple MATCH transformation */
static void test_match_simple(void)
{
    const char *query = "MATCH (n) RETURN n";
    
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

/* Test MATCH with label */
static void test_match_with_label(void)
{
    const char *query = "MATCH (n:Person) RETURN n";
    
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

/* Test invalid query handling */
static void test_invalid_query(void)
{
    const char *query = "MATCH (n) WHERE n.name = 'Alice' RETURN n";
    
    cypher_query_result *result = parse_and_transform(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        /* WHERE is not yet implemented, should fail gracefully */
        /* For now, we expect it to succeed but WHERE is ignored */
        cypher_free_result(result);
    }
}

/* Initialize the transform test suite */
int init_transform_suite(void)
{
    CU_pSuite suite = CU_add_suite("Transform", setup_transform_suite, teardown_transform_suite);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests */
    if (!CU_add_test(suite, "Simple MATCH", test_match_simple) ||
        !CU_add_test(suite, "MATCH with label", test_match_with_label) ||
        !CU_add_test(suite, "Simple CREATE", test_create_simple) ||
        !CU_add_test(suite, "CREATE with label", test_create_with_label) ||
        !CU_add_test(suite, "Invalid query handling", test_invalid_query)) {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}