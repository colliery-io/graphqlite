#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gql/gql_parser.h"
#include "test_gql_parser.h"

// Setup function called before each test
int setup_parser_test(void) {
    return 0;
}

// Teardown function called after each test
int teardown_parser_test(void) {
    return 0;
}

// =============================================================================
// Basic Parser Tests
// =============================================================================

void test_simple_match_parsing(void) {
    const char *query = "MATCH (n) RETURN n";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_MATCH_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_match_with_labels_parsing(void) {
    const char *query = "MATCH (p:Person) RETURN p";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_MATCH_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_multiple_labels_parsing(void) {
    const char *query = "MATCH (p:Person & Employee) RETURN p";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_MATCH_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_edge_pattern_parsing(void) {
    const char *query = "MATCH (a)-[r:KNOWS]->(b) RETURN a, b";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_MATCH_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_where_clause_parsing(void) {
    const char *query = "MATCH (p:Person) WHERE p.age > 25 RETURN p";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_MATCH_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_property_access_parsing(void) {
    const char *query = "MATCH (p:Person) RETURN p.name, p.age";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_MATCH_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_create_query_parsing(void) {
    const char *query = "CREATE (n:Person {name: \"Alice\"})";
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_PTR_NOT_NULL(ast);
    CU_ASSERT_FALSE(gql_parser_has_error(parser));
    CU_ASSERT_EQUAL(ast->type, GQL_AST_CREATE_QUERY);
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

void test_invalid_syntax_parsing(void) {
    const char *query = "MATCH (p:Person WHERE p.age > 25 RETURN p"; // Missing closing parenthesis
    gql_parser_t *parser = gql_parser_create(query);
    CU_ASSERT_PTR_NOT_NULL(parser);
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    CU_ASSERT_TRUE(gql_parser_has_error(parser));
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    gql_parser_destroy(parser);
}

// =============================================================================
// Test Suite Registration
// =============================================================================

int add_parser_tests(void) {
    CU_pSuite suite = CU_add_suite("Parser Tests", setup_parser_test, teardown_parser_test);
    if (NULL == suite) {
        return -1;
    }
    
    if (NULL == CU_add_test(suite, "Simple MATCH parsing", test_simple_match_parsing) ||
        NULL == CU_add_test(suite, "MATCH with labels parsing", test_match_with_labels_parsing) ||
        NULL == CU_add_test(suite, "Multiple labels parsing", test_multiple_labels_parsing) ||
        NULL == CU_add_test(suite, "Edge pattern parsing", test_edge_pattern_parsing) ||
        NULL == CU_add_test(suite, "WHERE clause parsing", test_where_clause_parsing) ||
        NULL == CU_add_test(suite, "Property access parsing", test_property_access_parsing) ||
        NULL == CU_add_test(suite, "CREATE query parsing", test_create_query_parsing) ||
        NULL == CU_add_test(suite, "Invalid syntax parsing", test_invalid_syntax_parsing)) {
        return -1;
    }
    
    return 0;
}