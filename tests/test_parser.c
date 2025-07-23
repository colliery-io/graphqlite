#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

// ============================================================================
// AST Utility Tests  
// ============================================================================

void test_ast_node_type_names(void) {
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_CREATE_STATEMENT), "CREATE_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_MATCH_STATEMENT), "MATCH_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_RETURN_STATEMENT), "RETURN_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_COMPOUND_STATEMENT), "COMPOUND_STATEMENT");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_NODE_PATTERN), "NODE_PATTERN");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_RELATIONSHIP_PATTERN), "RELATIONSHIP_PATTERN");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_PATH_PATTERN), "PATH_PATTERN");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_EDGE_PATTERN), "EDGE_PATTERN");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_VARIABLE), "VARIABLE");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_LABEL), "LABEL");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_PROPERTY), "PROPERTY");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_PROPERTY_LIST), "PROPERTY_LIST");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_STRING_LITERAL), "STRING_LITERAL");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_INTEGER_LITERAL), "INTEGER_LITERAL");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_FLOAT_LITERAL), "FLOAT_LITERAL");
    CU_ASSERT_STRING_EQUAL(ast_node_type_name(AST_BOOLEAN_LITERAL), "BOOLEAN_LITERAL");
}

void test_ast_memory_management(void) {
    // Test creating and freeing various AST nodes
    cypher_ast_node_t *var = ast_create_variable("test");
    CU_ASSERT_PTR_NOT_NULL(var);
    CU_ASSERT_EQUAL(var->type, AST_VARIABLE);
    CU_ASSERT_STRING_EQUAL(var->data.variable.name, "test");
    ast_free(var);
    
    cypher_ast_node_t *label = ast_create_label("Person");
    CU_ASSERT_PTR_NOT_NULL(label);
    CU_ASSERT_EQUAL(label->type, AST_LABEL);
    CU_ASSERT_STRING_EQUAL(label->data.label.name, "Person");
    ast_free(label);
    
    cypher_ast_node_t *str = ast_create_string_literal("hello");
    CU_ASSERT_PTR_NOT_NULL(str);
    CU_ASSERT_EQUAL(str->type, AST_STRING_LITERAL);
    CU_ASSERT_STRING_EQUAL(str->data.string_literal.value, "hello");
    ast_free(str);
    
    cypher_ast_node_t *int_val = ast_create_integer_literal("42");
    CU_ASSERT_PTR_NOT_NULL(int_val);
    CU_ASSERT_EQUAL(int_val->type, AST_INTEGER_LITERAL);
    CU_ASSERT_EQUAL(int_val->data.integer_literal.value, 42);
    ast_free(int_val);
    
    cypher_ast_node_t *float_val = ast_create_float_literal("3.14");
    CU_ASSERT_PTR_NOT_NULL(float_val);
    CU_ASSERT_EQUAL(float_val->type, AST_FLOAT_LITERAL);
    CU_ASSERT_EQUAL(float_val->data.float_literal.value, 3.14);
    ast_free(float_val);
    
    cypher_ast_node_t *bool_val = ast_create_boolean_literal(1);
    CU_ASSERT_PTR_NOT_NULL(bool_val);
    CU_ASSERT_EQUAL(bool_val->type, AST_BOOLEAN_LITERAL);
    CU_ASSERT_EQUAL(bool_val->data.boolean_literal.value, 1);
    ast_free(bool_val);
}

void test_ast_property_list(void) {
    // Create a property list
    cypher_ast_node_t *list = ast_create_property_list();
    CU_ASSERT_PTR_NOT_NULL(list);
    CU_ASSERT_EQUAL(list->type, AST_PROPERTY_LIST);
    CU_ASSERT_EQUAL(list->data.property_list.count, 0);
    CU_ASSERT_PTR_NULL(list->data.property_list.properties);
    
    // Add a property
    cypher_ast_node_t *value = ast_create_string_literal("John");
    cypher_ast_node_t *prop = ast_create_property("name", value);
    list = ast_add_property_to_list(list, prop);
    
    CU_ASSERT_EQUAL(list->data.property_list.count, 1);
    CU_ASSERT_PTR_NOT_NULL(list->data.property_list.properties);
    CU_ASSERT_PTR_EQUAL(list->data.property_list.properties[0], prop);
    
    // Add another property
    cypher_ast_node_t *age_val = ast_create_integer_literal("30");
    cypher_ast_node_t *age_prop = ast_create_property("age", age_val);
    list = ast_add_property_to_list(list, age_prop);
    
    CU_ASSERT_EQUAL(list->data.property_list.count, 2);
    CU_ASSERT_PTR_EQUAL(list->data.property_list.properties[1], age_prop);
    
    ast_free(list);
}

void test_ast_edge_patterns(void) {
    // Test edge pattern creation
    cypher_ast_node_t *var = ast_create_variable("r");
    cypher_ast_node_t *label = ast_create_label("KNOWS");
    cypher_ast_node_t *edge = ast_create_edge_pattern(var, label, NULL);
    
    CU_ASSERT_PTR_NOT_NULL(edge);
    CU_ASSERT_EQUAL(edge->type, AST_EDGE_PATTERN);
    CU_ASSERT_PTR_EQUAL(edge->data.edge_pattern.variable, var);
    CU_ASSERT_PTR_EQUAL(edge->data.edge_pattern.label, label);
    CU_ASSERT_PTR_NULL(edge->data.edge_pattern.properties);
    
    ast_free(edge);
}

void test_ast_relationship_patterns(void) {
    // Create nodes for relationship
    cypher_ast_node_t *var_a = ast_create_variable("a");
    cypher_ast_node_t *label_person = ast_create_label("Person");
    cypher_ast_node_t *left_node = ast_create_node_pattern(var_a, label_person, NULL);
    
    cypher_ast_node_t *var_b = ast_create_variable("b");
    cypher_ast_node_t *label_person2 = ast_create_label("Person");
    cypher_ast_node_t *right_node = ast_create_node_pattern(var_b, label_person2, NULL);
    
    // Create edge
    cypher_ast_node_t *edge_label = ast_create_label("KNOWS");
    cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, edge_label, NULL);
    
    // Create relationship pattern
    cypher_ast_node_t *rel = ast_create_relationship_pattern(left_node, edge, right_node, 1);
    
    CU_ASSERT_PTR_NOT_NULL(rel);
    CU_ASSERT_EQUAL(rel->type, AST_RELATIONSHIP_PATTERN);
    CU_ASSERT_PTR_EQUAL(rel->data.relationship_pattern.left_node, left_node);
    CU_ASSERT_PTR_EQUAL(rel->data.relationship_pattern.edge, edge);
    CU_ASSERT_PTR_EQUAL(rel->data.relationship_pattern.right_node, right_node);
    CU_ASSERT_EQUAL(rel->data.relationship_pattern.direction, 1);
    
    ast_free(rel);
}

// ============================================================================
// Test Suite Setup
// ============================================================================

int add_parser_tests(void) {
    CU_pSuite ast_suite = CU_add_suite("AST Utility Tests", NULL, NULL);
    if (!ast_suite) {
        return 0;
    }
    
    if (!CU_add_test(ast_suite, "AST node type names", test_ast_node_type_names) ||
        !CU_add_test(ast_suite, "AST memory management", test_ast_memory_management) ||
        !CU_add_test(ast_suite, "AST property list", test_ast_property_list) ||
        !CU_add_test(ast_suite, "AST edge patterns", test_ast_edge_patterns) ||
        !CU_add_test(ast_suite, "AST relationship patterns", test_ast_relationship_patterns)) {
        return 0;
    }
    
    return 1;
}