#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "cypher.tab.h"

// External parser functions
extern cypher_ast_node_t* parse_result;
extern void init_lexer(const char *input);
extern void cleanup_lexer(void);
extern int yyparse(void);

// Test helper: parse a query and return the AST
static cypher_ast_node_t* parse_query(const char *query) {
    parse_result = NULL;
    init_lexer(query);
    
    int result = yyparse();
    cleanup_lexer();
    
    if (result != 0) {
        return NULL;  // Parse failed
    }
    
    return parse_result;
}

// ============================================================================
// Basic Parser Tests
// ============================================================================

void test_parser_create_simple_node(void) {
    const char *query = "CREATE (n:Person)";
    cypher_ast_node_t *ast = parse_query(query);
    
    // Should parse successfully
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    // Should be a CREATE statement
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    // Should have a node pattern
    CU_ASSERT_PTR_NOT_NULL(ast->data.create_stmt.node_pattern);
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_NODE_PATTERN);
    
    // Should have variable "n"
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.variable);
    CU_ASSERT_EQUAL(pattern->data.node_pattern.variable->type, AST_VARIABLE);
    CU_ASSERT_STRING_EQUAL(pattern->data.node_pattern.variable->data.variable.name, "n");
    
    // Should have label "Person"
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.label);
    CU_ASSERT_EQUAL(pattern->data.node_pattern.label->type, AST_LABEL);
    CU_ASSERT_STRING_EQUAL(pattern->data.node_pattern.label->data.label.name, "Person");
    
    // Should not have properties
    CU_ASSERT_PTR_NULL(pattern->data.node_pattern.properties);
    
    // Clean up
    ast_free(ast);
}

void test_parser_create_node_with_property(void) {
    const char *query = "CREATE (n:Person {name: \"John\"})";
    cypher_ast_node_t *ast = parse_query(query);
    
    // Should parse successfully
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    // Should be a CREATE statement with node pattern
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_PTR_NOT_NULL(pattern);
    
    // Should have property list (even for single property)
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.properties);
    cypher_ast_node_t *props = pattern->data.node_pattern.properties;
    CU_ASSERT_EQUAL(props->type, AST_PROPERTY_LIST);
    CU_ASSERT_EQUAL(props->data.property_list.count, 1);
    
    // Check the single property in the list
    cypher_ast_node_t *prop = props->data.property_list.properties[0];
    CU_ASSERT_EQUAL(prop->type, AST_PROPERTY);
    CU_ASSERT_STRING_EQUAL(prop->data.property.key, "name");
    
    // Property value should be string literal "John"
    CU_ASSERT_PTR_NOT_NULL(prop->data.property.value);
    CU_ASSERT_EQUAL(prop->data.property.value->type, AST_STRING_LITERAL);
    CU_ASSERT_STRING_EQUAL(prop->data.property.value->data.string_literal.value, "John");
    
    // Clean up
    ast_free(ast);
}

void test_parser_match_simple_node(void) {
    const char *query = "MATCH (n:Person) RETURN n";
    cypher_ast_node_t *ast = parse_query(query);
    
    // Should parse successfully
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    // Should be a compound statement (MATCH + RETURN)
    CU_ASSERT_EQUAL(ast->type, AST_COMPOUND_STATEMENT);
    
    // Should have MATCH statement
    CU_ASSERT_PTR_NOT_NULL(ast->data.compound_stmt.match_stmt);
    cypher_ast_node_t *match = ast->data.compound_stmt.match_stmt;
    CU_ASSERT_EQUAL(match->type, AST_MATCH_STATEMENT);
    
    // MATCH should have node pattern
    CU_ASSERT_PTR_NOT_NULL(match->data.match_stmt.node_pattern);
    cypher_ast_node_t *pattern = match->data.match_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_NODE_PATTERN);
    
    // Should have RETURN statement
    CU_ASSERT_PTR_NOT_NULL(ast->data.compound_stmt.return_stmt);
    cypher_ast_node_t *return_stmt = ast->data.compound_stmt.return_stmt;
    CU_ASSERT_EQUAL(return_stmt->type, AST_RETURN_STATEMENT);
    
    // RETURN should reference variable "n"
    CU_ASSERT_PTR_NOT_NULL(return_stmt->data.return_stmt.variable);
    CU_ASSERT_EQUAL(return_stmt->data.return_stmt.variable->type, AST_VARIABLE);
    CU_ASSERT_STRING_EQUAL(return_stmt->data.return_stmt.variable->data.variable.name, "n");
    
    // Clean up
    ast_free(ast);
}

void test_parser_invalid_query(void) {
    const char *query = "INVALID SYNTAX HERE";
    cypher_ast_node_t *ast = parse_query(query);
    
    // Should fail to parse
    CU_ASSERT_PTR_NULL(ast);
}

void test_parser_create_multiple_properties(void) {
    const char *query = "CREATE (n:Product {name: \"Widget\", price: 100, inStock: true})";
    cypher_ast_node_t *ast = parse_query(query);
    
    // Should parse successfully
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    // Should be a CREATE statement with node pattern
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_PTR_NOT_NULL(pattern);
    
    // Should have property list
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.properties);
    cypher_ast_node_t *props = pattern->data.node_pattern.properties;
    CU_ASSERT_EQUAL(props->type, AST_PROPERTY_LIST);
    
    // Should have 3 properties
    CU_ASSERT_EQUAL(props->data.property_list.count, 3);
    
    // Check first property: name
    cypher_ast_node_t *prop1 = props->data.property_list.properties[0];
    CU_ASSERT_EQUAL(prop1->type, AST_PROPERTY);
    CU_ASSERT_STRING_EQUAL(prop1->data.property.key, "name");
    CU_ASSERT_EQUAL(prop1->data.property.value->type, AST_STRING_LITERAL);
    CU_ASSERT_STRING_EQUAL(prop1->data.property.value->data.string_literal.value, "Widget");
    
    // Check second property: price
    cypher_ast_node_t *prop2 = props->data.property_list.properties[1];
    CU_ASSERT_EQUAL(prop2->type, AST_PROPERTY);
    CU_ASSERT_STRING_EQUAL(prop2->data.property.key, "price");
    CU_ASSERT_EQUAL(prop2->data.property.value->type, AST_INTEGER_LITERAL);
    CU_ASSERT_EQUAL(prop2->data.property.value->data.integer_literal.value, 100);
    
    // Check third property: inStock
    cypher_ast_node_t *prop3 = props->data.property_list.properties[2];
    CU_ASSERT_EQUAL(prop3->type, AST_PROPERTY);
    CU_ASSERT_STRING_EQUAL(prop3->data.property.key, "inStock");
    CU_ASSERT_EQUAL(prop3->data.property.value->type, AST_BOOLEAN_LITERAL);
    CU_ASSERT_EQUAL(prop3->data.property.value->data.boolean_literal.value, 1);
    
    // Clean up
    ast_free(ast);
}

void test_parser_memory_management(void) {
    const char *query = "CREATE (test:Label {key: \"value\"})";
    
    // Parse multiple times to test for memory leaks
    for (int i = 0; i < 10; i++) {
        cypher_ast_node_t *ast = parse_query(query);
        CU_ASSERT_PTR_NOT_NULL(ast);
        if (ast) {
            ast_free(ast);
        }
    }
    
    // Test with multiple properties
    const char *multi_query = "CREATE (n:Product {name: \"Widget\", price: 100, active: true})";
    for (int i = 0; i < 5; i++) {
        cypher_ast_node_t *ast = parse_query(multi_query);
        CU_ASSERT_PTR_NOT_NULL(ast);
        if (ast) {
            ast_free(ast);
        }
    }
    
    // If we get here without crashing, memory management is working
    CU_ASSERT(1);  // Always passes - just testing for crashes
}

// ============================================================================
// AST Utility Tests
// ============================================================================

void test_ast_node_type_names(void) {
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_CREATE_STATEMENT), "CREATE_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_MATCH_STATEMENT), "MATCH_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_RETURN_STATEMENT), "RETURN_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_VARIABLE), "VARIABLE");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_LABEL), "LABEL");
}

// ============================================================================
// Test Suite Setup
// ============================================================================

int add_parser_tests(void) {
    CU_pSuite parser_suite = CU_add_suite("Parser Tests", NULL, NULL);
    if (!parser_suite) {
        return 0;
    }
    
    // Basic parsing tests
    if (!CU_add_test(parser_suite, "CREATE simple node", test_parser_create_simple_node) ||
        !CU_add_test(parser_suite, "CREATE node with property", test_parser_create_node_with_property) ||
        !CU_add_test(parser_suite, "CREATE node with multiple properties", test_parser_create_multiple_properties) ||
        !CU_add_test(parser_suite, "MATCH and RETURN", test_parser_match_simple_node) ||
        !CU_add_test(parser_suite, "Invalid query handling", test_parser_invalid_query) ||
        !CU_add_test(parser_suite, "Memory management", test_parser_memory_management)) {
        return 0;
    }
    
    // AST utility tests
    CU_pSuite ast_suite = CU_add_suite("AST Utility Tests", NULL, NULL);
    if (!ast_suite) {
        return 0;
    }
    
    if (!CU_add_test(ast_suite, "AST node type names", test_ast_node_type_names)) {
        return 0;
    }
    
    return 1;
}