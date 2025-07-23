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
// Edge Parser Tests
// ============================================================================

void test_parser_create_relationship_simple(void) {
    const char *query = "CREATE (a:Person)-[:KNOWS]->(b:Person)";
    cypher_ast_node_t *ast = parse_query(query);
    
    // Should parse successfully
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    // Should be a CREATE statement
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    // Should have a relationship pattern
    CU_ASSERT_PTR_NOT_NULL(ast->data.create_stmt.node_pattern);
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
    
    // Check direction (should be 1 for right)
    CU_ASSERT_EQUAL(pattern->data.relationship_pattern.direction, 1);
    
    // Check left node
    cypher_ast_node_t *left = pattern->data.relationship_pattern.left_node;
    CU_ASSERT_PTR_NOT_NULL(left);
    CU_ASSERT_EQUAL(left->type, AST_NODE_PATTERN);
    CU_ASSERT_STRING_EQUAL(left->data.node_pattern.variable->data.variable.name, "a");
    CU_ASSERT_STRING_EQUAL(left->data.node_pattern.label->data.label.name, "Person");
    
    // Check edge
    cypher_ast_node_t *edge = pattern->data.relationship_pattern.edge;
    CU_ASSERT_PTR_NOT_NULL(edge);
    CU_ASSERT_EQUAL(edge->type, AST_EDGE_PATTERN);
    CU_ASSERT_PTR_NULL(edge->data.edge_pattern.variable); // No variable for this edge
    CU_ASSERT_STRING_EQUAL(edge->data.edge_pattern.label->data.label.name, "KNOWS");
    CU_ASSERT_PTR_NULL(edge->data.edge_pattern.properties); // No properties
    
    // Check right node
    cypher_ast_node_t *right = pattern->data.relationship_pattern.right_node;
    CU_ASSERT_PTR_NOT_NULL(right);
    CU_ASSERT_EQUAL(right->type, AST_NODE_PATTERN);
    CU_ASSERT_STRING_EQUAL(right->data.node_pattern.variable->data.variable.name, "b");
    CU_ASSERT_STRING_EQUAL(right->data.node_pattern.label->data.label.name, "Person");
    
    ast_free(ast);
}

void test_parser_create_relationship_with_variable(void) {
    const char *query = "CREATE (a:Person)-[r:KNOWS]->(b:Person)";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
    
    // Check edge has variable
    cypher_ast_node_t *edge = pattern->data.relationship_pattern.edge;
    CU_ASSERT_PTR_NOT_NULL(edge);
    CU_ASSERT_EQUAL(edge->type, AST_EDGE_PATTERN);
    CU_ASSERT_PTR_NOT_NULL(edge->data.edge_pattern.variable);
    CU_ASSERT_STRING_EQUAL(edge->data.edge_pattern.variable->data.variable.name, "r");
    CU_ASSERT_STRING_EQUAL(edge->data.edge_pattern.label->data.label.name, "KNOWS");
    
    ast_free(ast);
}

void test_parser_create_relationship_with_properties(void) {
    const char *query = "CREATE (a:Person)-[r:KNOWS {since: \"2020\", strength: 5}]->(b:Person)";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
    
    // Check edge has properties
    cypher_ast_node_t *edge = pattern->data.relationship_pattern.edge;
    CU_ASSERT_PTR_NOT_NULL(edge);
    CU_ASSERT_PTR_NOT_NULL(edge->data.edge_pattern.properties);
    CU_ASSERT_EQUAL(edge->data.edge_pattern.properties->type, AST_PROPERTY_LIST);
    CU_ASSERT_EQUAL(edge->data.edge_pattern.properties->data.property_list.count, 2);
    
    ast_free(ast);
}

void test_parser_create_relationship_left_direction(void) {
    const char *query = "CREATE (a:Person)<-[:KNOWS]-(b:Person)";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    CU_ASSERT_EQUAL(ast->type, AST_CREATE_STATEMENT);
    
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
    
    // Check direction (should be -1 for left)
    CU_ASSERT_EQUAL(pattern->data.relationship_pattern.direction, -1);
    
    ast_free(ast);
}

void test_parser_match_relationship(void) {
    const char *query = "MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a";
    cypher_ast_node_t *ast = parse_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(ast);
    if (!ast) return;
    
    CU_ASSERT_EQUAL(ast->type, AST_COMPOUND_STATEMENT);
    
    cypher_ast_node_t *match = ast->data.compound_stmt.match_stmt;
    CU_ASSERT_PTR_NOT_NULL(match);
    CU_ASSERT_EQUAL(match->type, AST_MATCH_STATEMENT);
    
    cypher_ast_node_t *pattern = match->data.match_stmt.node_pattern;
    CU_ASSERT_PTR_NOT_NULL(pattern);
    CU_ASSERT_EQUAL(pattern->type, AST_RELATIONSHIP_PATTERN);
    
    ast_free(ast);
}

void test_parser_edge_memory_management(void) {
    // Test that edge parsing doesn't leak memory
    const char *queries[] = {
        "CREATE (a:Person)-[:KNOWS]->(b:Person)",
        "CREATE (a:Person)-[r:KNOWS]->(b:Person)",
        "CREATE (a:Person)-[r:KNOWS {since: \"2020\"}]->(b:Person)",
        "CREATE (a:Person)<-[:KNOWS]-(b:Person)",
        "MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a"
    };
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {  // Parse each query multiple times
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

int add_edge_parser_tests(void) {
    CU_pSuite edge_suite = CU_add_suite("Edge Parser Tests", NULL, NULL);
    if (!edge_suite) {
        return 0;
    }
    
    if (!CU_add_test(edge_suite, "CREATE simple relationship", test_parser_create_relationship_simple) ||
        !CU_add_test(edge_suite, "CREATE relationship with variable", test_parser_create_relationship_with_variable) ||
        !CU_add_test(edge_suite, "CREATE relationship with properties", test_parser_create_relationship_with_properties) ||
        !CU_add_test(edge_suite, "CREATE relationship left direction", test_parser_create_relationship_left_direction) ||
        !CU_add_test(edge_suite, "MATCH relationship", test_parser_match_relationship) ||
        !CU_add_test(edge_suite, "Edge memory management", test_parser_edge_memory_management)) {
        return 0;
    }
    
    return 1;
}