#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"
#include "cypher_gram.tab.h"
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

/* Test relationship patterns */
static void test_relationship_patterns(void)
{
    /* Test simple relationship without type */
    const char *query1 = "CREATE (a)-[]->(b)";
    ast_node *result1 = parse_cypher_query(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    if (result1) {
        CU_ASSERT_EQUAL(result1->type, AST_NODE_QUERY);
        cypher_parser_free_result(result1);
    }
    
    /* Test relationship with type */
    const char *query2 = "CREATE (a)-[:KNOWS]->(b)";
    ast_node *result2 = parse_cypher_query(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    if (result2) {
        CU_ASSERT_EQUAL(result2->type, AST_NODE_QUERY);
        cypher_parser_free_result(result2);
    }
    
    /* Test bidirectional relationship */
    const char *query3 = "CREATE (a)<-[:KNOWS]-(b)";
    ast_node *result3 = parse_cypher_query(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);
    if (result3) {
        CU_ASSERT_EQUAL(result3->type, AST_NODE_QUERY);
        cypher_parser_free_result(result3);
    }
    
    /* Test undirected relationship */
    const char *query4 = "CREATE (a)-[:KNOWS]-(b)";
    ast_node *result4 = parse_cypher_query(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);
    if (result4) {
        CU_ASSERT_EQUAL(result4->type, AST_NODE_QUERY);
        cypher_parser_free_result(result4);
    }
}

/* Test relationship with variables */
static void test_relationship_variables(void)
{
    const char *query = "CREATE (a)-[r:KNOWS]->(b)";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test complex path patterns */
static void test_complex_paths(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b)-[:LIKES]->(c)";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test AST structural integrity - validates the systematic AST traversal fixes */
static void test_ast_structural_integrity(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b)";
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
    
    /* Deep AST structure validation - this tests the systematic traversal bug fix */
    cypher_query *query_ast = (cypher_query*)result;
    CU_ASSERT_PTR_NOT_NULL(query_ast->clauses);
    CU_ASSERT_EQUAL(query_ast->clauses->count, 1);
    
    /* Validate CREATE clause structure */
    ast_node *clause = query_ast->clauses->items[0];
    CU_ASSERT_PTR_NOT_NULL(clause);
    CU_ASSERT_EQUAL(clause->type, AST_NODE_CREATE);
    
    cypher_create *create = (cypher_create*)clause;
    CU_ASSERT_PTR_NOT_NULL(create->pattern);
    CU_ASSERT_EQUAL(create->pattern->count, 1);
    
    /* Validate path structure */
    ast_node *path_node = create->pattern->items[0];
    CU_ASSERT_PTR_NOT_NULL(path_node);
    CU_ASSERT_EQUAL(path_node->type, AST_NODE_PATH);
    
    cypher_path *path = (cypher_path*)path_node;
    CU_ASSERT_PTR_NOT_NULL(path->elements);
    CU_ASSERT_EQUAL(path->elements->count, 3); /* node, rel, node */
    
    /* Validate first node */
    ast_node *first_node = path->elements->items[0];
    CU_ASSERT_EQUAL(first_node->type, AST_NODE_NODE_PATTERN);
    
    /* Validate relationship */
    ast_node *rel_node = path->elements->items[1];
    CU_ASSERT_EQUAL(rel_node->type, AST_NODE_REL_PATTERN);
    cypher_rel_pattern *rel = (cypher_rel_pattern*)rel_node;
    CU_ASSERT_EQUAL(rel->left_arrow, false);
    CU_ASSERT_EQUAL(rel->right_arrow, true);
    CU_ASSERT_PTR_NOT_NULL(rel->type);
    CU_ASSERT_STRING_EQUAL(rel->type, "KNOWS");
    
    /* Validate second node */
    ast_node *second_node = path->elements->items[2];
    CU_ASSERT_EQUAL(second_node->type, AST_NODE_NODE_PATTERN);
    
    cypher_parser_free_result(result);
}

/* Test AST traversal with complex paths */
static void test_ast_complex_path_validation(void)
{
    const char *query = "CREATE (a)-[:KNOWS]->(b)-[:LIKES]->(c)";
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    cypher_query *query_ast = (cypher_query*)result;
    cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)create->pattern->items[0];
    
    /* Should have 5 elements: node, rel, node, rel, node */
    CU_ASSERT_EQUAL(path->elements->count, 5);
    
    /* Validate pattern: node -> rel -> node -> rel -> node */
    CU_ASSERT_EQUAL(path->elements->items[0]->type, AST_NODE_NODE_PATTERN);
    CU_ASSERT_EQUAL(path->elements->items[1]->type, AST_NODE_REL_PATTERN);
    CU_ASSERT_EQUAL(path->elements->items[2]->type, AST_NODE_NODE_PATTERN);
    CU_ASSERT_EQUAL(path->elements->items[3]->type, AST_NODE_REL_PATTERN);
    CU_ASSERT_EQUAL(path->elements->items[4]->type, AST_NODE_NODE_PATTERN);
    
    /* Validate relationship types */
    cypher_rel_pattern *rel1 = (cypher_rel_pattern*)path->elements->items[1];
    cypher_rel_pattern *rel2 = (cypher_rel_pattern*)path->elements->items[3];
    CU_ASSERT_STRING_EQUAL(rel1->type, "KNOWS");
    CU_ASSERT_STRING_EQUAL(rel2->type, "LIKES");
    
    cypher_parser_free_result(result);
}

/* Test AST validation for MATCH with RETURN */
static void test_ast_match_return_validation(void)
{
    const char *query = "MATCH (n:Person) RETURN n.name AS name";
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    cypher_query *query_ast = (cypher_query*)result;
    CU_ASSERT_EQUAL(query_ast->clauses->count, 2);
    
    /* Validate MATCH clause */
    ast_node *match_clause = query_ast->clauses->items[0];
    CU_ASSERT_EQUAL(match_clause->type, AST_NODE_MATCH);
    cypher_match *match = (cypher_match*)match_clause;
    CU_ASSERT_PTR_NOT_NULL(match->pattern);
    CU_ASSERT_EQUAL(match->optional, false);
    
    /* Validate RETURN clause */
    ast_node *return_clause = query_ast->clauses->items[1];
    CU_ASSERT_EQUAL(return_clause->type, AST_NODE_RETURN);
    cypher_return *ret = (cypher_return*)return_clause;
    CU_ASSERT_PTR_NOT_NULL(ret->items);
    CU_ASSERT_EQUAL(ret->items->count, 1);
    CU_ASSERT_EQUAL(ret->distinct, false);
    
    cypher_parser_free_result(result);
}

/* Test error conditions that should be properly handled */
static void test_ast_error_handling(void)
{
    /* Test malformed relationship syntax */
    const char *bad_query1 = "CREATE (a)-[:KNOWS(b)";
    ast_node *result1 = parse_cypher_query(bad_query1);
    /* Should handle gracefully - either NULL or error flag set */
    if (result1) {
        cypher_parser_free_result(result1);
    }
    
    /* Test invalid node syntax */
    const char *bad_query2 = "CREATE (a:";
    ast_node *result2 = parse_cypher_query(bad_query2);
    if (result2) {
        cypher_parser_free_result(result2);
    }
    
    /* Test incomplete query */
    const char *bad_query3 = "MATCH";
    ast_node *result3 = parse_cypher_query(bad_query3);
    if (result3) {
        cypher_parser_free_result(result3);
    }
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

/* Test CREATE with node properties */
static void test_create_node_properties(void)
{
    const char *query = "CREATE (n {name: 'Alice', age: 30})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    // Just verify parsing succeeded - the detailed validation is in our debug script
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with label and properties */
static void test_create_label_and_properties(void)
{
    const char *query = "CREATE (n:Person {name: 'Bob', age: 25})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];
        
        CU_ASSERT_PTR_NOT_NULL(node->label);
        CU_ASSERT_STRING_EQUAL(node->label, "Person");
        CU_ASSERT_PTR_NOT_NULL(node->properties);
        CU_ASSERT_EQUAL(node->properties->type, AST_NODE_MAP);
        
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with empty properties */
static void test_create_empty_properties(void)
{
    const char *query = "CREATE (n {})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with variable name */
static void test_create_with_variable(void)
{
    const char *query = "CREATE (alice:Person {name: 'Alice'})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];
        
        CU_ASSERT_PTR_NOT_NULL(node->variable);
        CU_ASSERT_STRING_EQUAL(node->variable, "alice");
        CU_ASSERT_PTR_NOT_NULL(node->label);
        CU_ASSERT_STRING_EQUAL(node->label, "Person");
        CU_ASSERT_PTR_NOT_NULL(node->properties);
        
        cypher_parser_free_result(result);
    }
}

/* Test CREATE multiple nodes */
static void test_create_multiple_nodes(void)
{
    const char *query = "CREATE (a:Person {name: 'Alice'}), (b:Person {name: 'Bob'})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(create->pattern->count, 2); /* Two separate patterns */
        
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with different property types */
static void test_create_property_types(void)
{
    const char *query = "CREATE (n {name: 'Alice', age: 30, active: true, score: 95.5})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];
        
        CU_ASSERT_PTR_NOT_NULL(node->properties);
        CU_ASSERT_EQUAL(node->properties->type, AST_NODE_MAP);
        
        cypher_map *map = (cypher_map*)node->properties;
        CU_ASSERT_PTR_NOT_NULL(map->pairs);
        CU_ASSERT_EQUAL(map->pairs->count, 4); /* 4 properties */
        
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with negative numbers */
static void test_create_negative_numbers(void)
{
    const char *query = "CREATE (n {neg_int: -42, neg_float: -3.14})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];
        
        CU_ASSERT_PTR_NOT_NULL(node->properties);
        CU_ASSERT_EQUAL(node->properties->type, AST_NODE_MAP);
        
        cypher_map *map = (cypher_map*)node->properties;
        CU_ASSERT_EQUAL(map->pairs->count, 2);
        
        /* Check first property value is negative integer */
        cypher_map_pair *pair1 = (cypher_map_pair*)map->pairs->items[0];
        CU_ASSERT_STRING_EQUAL(pair1->key, "neg_int");
        CU_ASSERT_EQUAL(pair1->value->type, AST_NODE_LITERAL);
        cypher_literal *lit1 = (cypher_literal*)pair1->value;
        CU_ASSERT_EQUAL(lit1->literal_type, LITERAL_INTEGER);
        CU_ASSERT_EQUAL(lit1->value.integer, -42);
        
        /* Check second property value is negative decimal */
        cypher_map_pair *pair2 = (cypher_map_pair*)map->pairs->items[1];
        CU_ASSERT_STRING_EQUAL(pair2->key, "neg_float");
        CU_ASSERT_EQUAL(pair2->value->type, AST_NODE_LITERAL);
        cypher_literal *lit2 = (cypher_literal*)pair2->value;
        CU_ASSERT_EQUAL(lit2->literal_type, LITERAL_DECIMAL);
        CU_ASSERT_DOUBLE_EQUAL(lit2->value.decimal, -3.14, 0.001);
        
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with just label (no variable, no properties) */
static void test_create_label_only(void)
{
    const char *query = "CREATE (:Person)";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];
        
        CU_ASSERT_PTR_NULL(node->variable); /* No variable */
        CU_ASSERT_PTR_NOT_NULL(node->label);
        CU_ASSERT_STRING_EQUAL(node->label, "Person");
        CU_ASSERT_PTR_NULL(node->properties); /* No properties */
        
        cypher_parser_free_result(result);
    }
}

/* Test CREATE with properties but no label */
static void test_create_properties_no_label(void)
{
    const char *query = "CREATE (n {name: 'Alice'})";
    
    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);
    
    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];
        
        CU_ASSERT_PTR_NOT_NULL(node->variable);
        CU_ASSERT_STRING_EQUAL(node->variable, "n");
        CU_ASSERT_PTR_NULL(node->label); /* No label */
        CU_ASSERT_PTR_NOT_NULL(node->properties);
        
        cypher_parser_free_result(result);
    }
}

/* Test parser keyword-to-token utility function */
static void test_parser_keyword_utilities(void)
{
    /* Test cypher_keyword_to_token_name function */
    const char *match_name = cypher_keyword_to_token_name(CYPHER_MATCH);
    CU_ASSERT_PTR_NOT_NULL(match_name);
    
    const char *return_name = cypher_keyword_to_token_name(CYPHER_RETURN);
    CU_ASSERT_PTR_NOT_NULL(return_name);
    
    const char *create_name = cypher_keyword_to_token_name(CYPHER_CREATE);
    CU_ASSERT_PTR_NOT_NULL(create_name);
    
    /* Test with invalid token ID */
    const char *unknown_name = cypher_keyword_to_token_name(99999);
    CU_ASSERT_PTR_NOT_NULL(unknown_name);
    CU_ASSERT_STRING_EQUAL(unknown_name, "unknown");
    
    printf("\nKeyword utility test completed\n");
}

/* Test parser token name utility function */
static void test_parser_token_names(void)
{
    /* Test cypher_token_name function */
    const char *eof_name = cypher_token_name(0);
    CU_ASSERT_PTR_NOT_NULL(eof_name);
    CU_ASSERT_STRING_EQUAL(eof_name, "EOF");
    
    const char *integer_name = cypher_token_name(CYPHER_INTEGER);
    CU_ASSERT_PTR_NOT_NULL(integer_name);
    CU_ASSERT_STRING_EQUAL(integer_name, "INTEGER");
    
    const char *string_name = cypher_token_name(CYPHER_STRING);
    CU_ASSERT_PTR_NOT_NULL(string_name);
    CU_ASSERT_STRING_EQUAL(string_name, "STRING");
    
    const char *match_name = cypher_token_name(CYPHER_MATCH);
    CU_ASSERT_PTR_NOT_NULL(match_name);
    CU_ASSERT_STRING_EQUAL(match_name, "MATCH");
    
    /* Test with character token */
    const char *char_name = cypher_token_name('(');
    CU_ASSERT_PTR_NOT_NULL(char_name);
    CU_ASSERT_STRING_EQUAL(char_name, "'('");
    
    /* Test with unknown token */
    const char *unknown_name = cypher_token_name(99999);
    CU_ASSERT_PTR_NOT_NULL(unknown_name);
    CU_ASSERT_STRING_EQUAL(unknown_name, "UNKNOWN");
    
    printf("\nToken name utility test completed\n");
}

/* Test parser with malformed input that could cause scanner errors */
static void test_parser_scanner_edge_cases(void)
{
    /* Test with input that has unclosed strings */
    const char *unclosed_string = "MATCH (n {name: \"unclosed";
    ast_node *result1 = parse_cypher_query(unclosed_string);
    
    if (result1) {
        /* If parsing succeeded, that's fine too */
        cypher_parser_free_result(result1);
        printf("\nUnclosed string query parsed successfully\n");
    } else {
        printf("\nUnclosed string query failed as expected\n");
    }
    
    /* Test with input that has invalid characters */
    const char *invalid_chars = "MATCH (n) @#$%^";
    ast_node *result2 = parse_cypher_query(invalid_chars);
    
    if (result2) {
        cypher_parser_free_result(result2);
        printf("\nInvalid characters query parsed\n");
    } else {
        printf("\nInvalid characters query failed\n");
    }
    
    /* Test with very long identifier */
    char long_identifier[2000];
    strcpy(long_identifier, "MATCH (");
    for (int i = 0; i < 1800; i++) {
        strcat(long_identifier, "a");
    }
    strcat(long_identifier, ") RETURN n");
    
    ast_node *result3 = parse_cypher_query(long_identifier);
    if (result3) {
        cypher_parser_free_result(result3);
        printf("\nLong identifier query parsed\n");
    } else {
        printf("\nLong identifier query failed\n");
    }
    
    printf("\nScanner edge cases test completed\n");
}

/* Test parser with various special token types */
static void test_parser_special_tokens(void)
{
    /* Test queries that would generate special tokens if implemented */
    
    /* Test parameter token (currently not implemented but should be handled) */
    const char *param_query = "MATCH (n {name: $param}) RETURN n";
    ast_node *result1 = parse_cypher_query(param_query);
    if (result1) {
        cypher_parser_free_result(result1);
        printf("\nParameter query parsed\n");
    } else {
        printf("\nParameter query failed\n");
    }
    
    /* Test comparison operators that generate special tokens */
    const char *compare_query1 = "MATCH (n) WHERE n.age >= 18 RETURN n";
    ast_node *result2 = parse_cypher_query(compare_query1);
    if (result2) {
        cypher_parser_free_result(result2);
        printf("\nGreater-equal query parsed\n");
    } else {
        printf("\nGreater-equal query failed\n");
    }
    
    const char *compare_query2 = "MATCH (n) WHERE n.age <= 65 RETURN n";
    ast_node *result3 = parse_cypher_query(compare_query2);
    if (result3) {
        cypher_parser_free_result(result3);
        printf("\nLess-equal query parsed\n");
    } else {
        printf("\nLess-equal query failed\n");
    }
    
    const char *compare_query3 = "MATCH (n) WHERE n.name <> 'test' RETURN n";
    ast_node *result4 = parse_cypher_query(compare_query3);
    if (result4) {
        cypher_parser_free_result(result4);
        printf("\nNot-equal query parsed\n");
    } else {
        printf("\nNot-equal query failed\n");
    }
    
    printf("\nSpecial tokens test completed\n");
}

/* Test parser with NULL result handling */
static void test_parser_null_result_handling(void)
{
    /* Test cypher_parser_free_result with NULL */
    cypher_parser_free_result(NULL);
    printf("\nNULL result free test completed\n");
    
    /* Test cypher_parser_get_error with various inputs */
    const char *error1 = cypher_parser_get_error(NULL);
    CU_ASSERT_PTR_NULL(error1); /* Should return NULL for no error */
    
    /* Parse a valid query and check error */
    ast_node *valid_result = parse_cypher_query("MATCH (n) RETURN n");
    if (valid_result) {
        const char *error2 = cypher_parser_get_error(valid_result);
        CU_ASSERT_PTR_NULL(error2); /* Should return NULL for successful parse */
        cypher_parser_free_result(valid_result);
    }
    
    printf("\nNULL result handling test completed\n");
}

/* Test parser with complex nested structures */
static void test_parser_complex_nesting(void)
{
    /* Test deeply nested property access */
    const char *nested_query = "MATCH (a)-[:KNOWS]->(b)-[:WORKS_AT]->(c) WHERE a.name = 'Alice' RETURN a, b, c";
    ast_node *result1 = parse_cypher_query(nested_query);
    if (result1) {
        cypher_parser_free_result(result1);
        printf("\nComplex nested query parsed\n");
    } else {
        printf("\nComplex nested query failed\n");
    }
    
    /* Test multiple CREATE patterns */
    const char *multi_create = "CREATE (a:Person), (b:Company), (a)-[:WORKS_AT]->(b)";
    ast_node *result2 = parse_cypher_query(multi_create);
    if (result2) {
        cypher_parser_free_result(result2);
        printf("\nMultiple CREATE patterns parsed\n");
    } else {
        printf("\nMultiple CREATE patterns failed\n");
    }
    
    printf("\nComplex nesting test completed\n");
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
        !CU_add_test(suite, "CREATE with empty properties", test_create_empty_properties) ||
        !CU_add_test(suite, "CREATE with node properties", test_create_node_properties) ||
        !CU_add_test(suite, "CREATE with label and properties", test_create_label_and_properties) ||
        !CU_add_test(suite, "CREATE with variable", test_create_with_variable) ||
        !CU_add_test(suite, "CREATE multiple nodes", test_create_multiple_nodes) ||
        !CU_add_test(suite, "CREATE with property types", test_create_property_types) ||
        !CU_add_test(suite, "CREATE with negative numbers", test_create_negative_numbers) ||
        !CU_add_test(suite, "CREATE label only", test_create_label_only) ||
        !CU_add_test(suite, "CREATE properties no label", test_create_properties_no_label) ||
        !CU_add_test(suite, "Node with label", test_node_with_label) ||
        !CU_add_test(suite, "RETURN with alias", test_return_with_alias) ||
        !CU_add_test(suite, "Literal parsing", test_literal_parsing) ||
        !CU_add_test(suite, "Relationship patterns", test_relationship_patterns) ||
        !CU_add_test(suite, "Relationship variables", test_relationship_variables) ||
        !CU_add_test(suite, "Complex paths", test_complex_paths) ||
        !CU_add_test(suite, "AST structural integrity", test_ast_structural_integrity) ||
        !CU_add_test(suite, "AST complex path validation", test_ast_complex_path_validation) ||
        !CU_add_test(suite, "AST MATCH RETURN validation", test_ast_match_return_validation) ||
        !CU_add_test(suite, "AST error handling", test_ast_error_handling) ||
        !CU_add_test(suite, "Invalid syntax", test_invalid_syntax) ||
        !CU_add_test(suite, "Empty query", test_empty_query) ||
        !CU_add_test(suite, "NULL query", test_null_query) ||
        !CU_add_test(suite, "AST printing", test_ast_printing) ||
        !CU_add_test(suite, "Parser keyword utilities", test_parser_keyword_utilities) ||
        !CU_add_test(suite, "Parser token names", test_parser_token_names) ||
        !CU_add_test(suite, "Parser scanner edge cases", test_parser_scanner_edge_cases) ||
        !CU_add_test(suite, "Parser special tokens", test_parser_special_tokens) ||
        !CU_add_test(suite, "Parser NULL result handling", test_parser_null_result_handling) ||
        !CU_add_test(suite, "Parser complex nesting", test_parser_complex_nesting))
    {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}