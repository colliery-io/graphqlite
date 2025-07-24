#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"
#include "test_parser.h"

/* Test basic query parsing */
static void test_simple_match_return(void)
{
    const char *query = "MATCH (n) RETURN n";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_parser_free_result(result);
    }
}

/* Test simple CREATE parsing */
static void test_simple_create(void)
{
    const char *query = "CREATE (n)";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test node pattern with label */
static void test_node_with_label(void)
{
    const char *query = "MATCH (n:Person) RETURN n";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test return with alias */
static void test_return_with_alias(void)
{
    const char *query = "MATCH (n) RETURN n AS person";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test literals */
static void test_literal_parsing(void)
{
    const char *query = "RETURN 42, 'hello', true, false, null";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test invalid syntax */
static void test_invalid_syntax(void)
{
    const char *query = "MATCH RETURN"; /* Invalid - missing pattern */
    
    ast_node *result = parse_cypher_query(query);
    /* Should return NULL or indicate error for invalid syntax */
    
    if (result) {
        cypher_parser_free_result(result);
    }
}

/* Test empty query */
static void test_empty_query(void)
{
    const char *query = "";
    
    ast_node *result = parse_cypher_query(query);
    /* Should handle empty query gracefully */
    
    if (result) {
        cypher_parser_free_result(result);
    }
}

/* Test NULL query */
static void test_null_query(void)
{
    ast_node *result = parse_cypher_query(NULL);
    CU_ASSERT_PTR_NULL(result);
}

/* Test AST printing for debugging */
static void test_ast_printing(void)
{
    const char *query = "MATCH (n) RETURN n";
    
    ast_node *result = parse_cypher_query(query);
    if (result) {
        /* Print AST for visual inspection - only in debug mode */
#ifdef GRAPHQLITE_DEBUG
        printf("\n--- AST for '%s' ---\n", query);
        ast_node_print(result, 0);
        printf("--- End AST ---\n");
#endif
        
        cypher_parser_free_result(result);
    }
}

/* Initialize the parser test suite */
int init_parser_suite(void)
{
    CU_pSuite suite = CU_add_suite("Parser", NULL, NULL);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests to suite */
    if (!CU_add_test(suite, "Simple MATCH RETURN", test_simple_match_return) ||
        !CU_add_test(suite, "Simple CREATE", test_simple_create) ||
        !CU_add_test(suite, "Node with label", test_node_with_label) ||
        !CU_add_test(suite, "RETURN with alias", test_return_with_alias) ||
        !CU_add_test(suite, "Literal parsing", test_literal_parsing) ||
        !CU_add_test(suite, "Invalid syntax", test_invalid_syntax) ||
        !CU_add_test(suite, "Empty query", test_empty_query) ||
        !CU_add_test(suite, "NULL query", test_null_query) ||
        !CU_add_test(suite, "AST printing", test_ast_printing))
    {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}