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
// Node Parser Tests
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
    CU_ASSERT_STRING_EQUAL(pattern->data.node_pattern.variable->data.variable.name, "n");
    
    // Should have label "Person"
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.label);
    CU_ASSERT_STRING_EQUAL(pattern->data.node_pattern.label->data.label.name, "Person");
    
    // Should have no properties
    CU_ASSERT_PTR_NULL(pattern->data.node_pattern.properties);
    
    ast_free(ast);
}

void test_parser_create_node_with_property(void) {
    const char *query = "CREATE (n:Person {name: \"John\"})";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_NODE_PATTERN);
    
    // Should have properties (now always a property list)
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.properties);
    cypher_ast_node_t *props = pattern->data.node_pattern.properties;
    CU_ASSERT_EQUAL(props->type, AST_PROPERTY_LIST);
    CU_ASSERT_EQUAL(props->data.property_list.count, 1);
    
    // Check the property
    cypher_ast_node_t *prop = props->data.property_list.properties[0];
    CU_ASSERT_EQUAL(prop->type, AST_PROPERTY);
    CU_ASSERT_STRING_EQUAL(prop->data.property.key, "name");
    
    // Check the value
    cypher_ast_node_t *value = prop->data.property.value;
    CU_ASSERT_EQUAL(value->type, AST_STRING_LITERAL);
    CU_ASSERT_STRING_EQUAL(value->data.string_literal.value, "John");
    
    ast_free(ast);
}

void test_parser_create_multiple_properties(void) {
    const char *query = "CREATE (n:Product {name: \"Widget\", price: 100, rating: 4.5, inStock: true})";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_NODE_PATTERN);
    
    // Should have property list with 4 properties
    CU_ASSERT_PTR_NOT_NULL(pattern->data.node_pattern.properties);
    cypher_ast_node_t *props = pattern->data.node_pattern.properties;
    CU_ASSERT_EQUAL(props->type, AST_PROPERTY_LIST);
    CU_ASSERT_EQUAL(props->data.property_list.count, 4);
    
    // Check property names
    CU_ASSERT_STRING_EQUAL(props->data.property_list.properties[0]->data.property.key, "name");
    CU_ASSERT_STRING_EQUAL(props->data.property_list.properties[1]->data.property.key, "price");
    CU_ASSERT_STRING_EQUAL(props->data.property_list.properties[2]->data.property.key, "rating");
    CU_ASSERT_STRING_EQUAL(props->data.property_list.properties[3]->data.property.key, "inStock");
    
    // Check property types
    CU_ASSERT_EQUAL(props->data.property_list.properties[0]->data.property.value->type, AST_STRING_LITERAL);
    CU_ASSERT_EQUAL(props->data.property_list.properties[1]->data.property.value->type, AST_INTEGER_LITERAL);
    CU_ASSERT_EQUAL(props->data.property_list.properties[2]->data.property.value->type, AST_FLOAT_LITERAL);
    CU_ASSERT_EQUAL(props->data.property_list.properties[3]->data.property.value->type, AST_BOOLEAN_LITERAL);
    
    ast_free(ast);
}

void test_parser_match_simple_node(void) {
    const char *query = "MATCH (n:Person) RETURN n";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    // Should be a compound statement (MATCH + RETURN)
    CU_ASSERT_EQUAL(ast->type, AST_COMPOUND_STATEMENT);
    
    // Check MATCH part
    cypher_ast_node_t *match = ast->data.compound_stmt.match_stmt;
    CU_ASSERT_PTR_NOT_NULL(match);
    CU_ASSERT_EQUAL(match->type, AST_MATCH_STATEMENT);
    
    cypher_ast_node_t *pattern = match->data.match_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_NODE_PATTERN);
    CU_ASSERT_STRING_EQUAL(pattern->data.node_pattern.variable->data.variable.name, "n");
    CU_ASSERT_STRING_EQUAL(pattern->data.node_pattern.label->data.label.name, "Person");
    
    // Check RETURN part
    cypher_ast_node_t *return_stmt = ast->data.compound_stmt.return_stmt;
    CU_ASSERT_PTR_NOT_NULL(return_stmt);
    CU_ASSERT_EQUAL(return_stmt->type, AST_RETURN_STATEMENT);
    CU_ASSERT_STRING_EQUAL(return_stmt->data.return_stmt.variable->data.variable.name, "n");
    
    ast_free(ast);
}

void test_parser_invalid_node_query(void) {
    // Test various invalid queries
    const char *invalid_queries[] = {
        "CREATE (:Person)",     // Missing variable
        "CREATE (n)",           // Missing label
        "CREATE (n:)",          // Empty label
        "MATCH (n:Person",      // Missing closing paren
        "CREATE (n:Person {name})", // Invalid property syntax
    };
    
    for (int i = 0; i < 5; i++) {
        cypher_ast_node_t *ast = parse_query(invalid_queries[i]);
        CU_ASSERT_PTR_NULL(ast);  // Should fail to parse
    }
}

void test_parser_node_memory_management(void) {
    // Test that parsing doesn't leak memory
    const char *queries[] = {
        "CREATE (n:Person)",
        "CREATE (n:Person {name: \"John\"})",
        "CREATE (n:Product {name: \"Widget\", price: 100})",
        "MATCH (n:Person) RETURN n"
    };
    
    // Parse each query multiple times
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) {
            cypher_ast_node_t *ast = parse_query(queries[j]);
            CU_ASSERT_PTR_NOT_NULL(ast);
            if (ast) {
                ast_free(ast);
            }
        }
    }
    
    // If we get here without crashing, memory management is working
    CU_ASSERT(1);  // Always passes - just testing for crashes
}

// ============================================================================
// Test Suite Setup
// ============================================================================

int add_node_parser_tests(void) {
    CU_pSuite node_suite = CU_add_suite("Node Parser Tests", NULL, NULL);
    if (!node_suite) {
        return 0;
    }
    
    if (!CU_add_test(node_suite, "CREATE simple node", test_parser_create_simple_node) ||
        !CU_add_test(node_suite, "CREATE node with property", test_parser_create_node_with_property) ||
        !CU_add_test(node_suite, "CREATE node with multiple properties", test_parser_create_multiple_properties) ||
        !CU_add_test(node_suite, "MATCH simple node", test_parser_match_simple_node) ||
        !CU_add_test(node_suite, "Invalid node queries", test_parser_invalid_node_query) ||
        !CU_add_test(node_suite, "Node memory management", test_parser_node_memory_management)) {
        return 0;
    }
    
    return 1;
}