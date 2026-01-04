#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_debug.h"
#include "parser/cypher_keywords.h"
#include "cypher_gram.tab.h"
#include "test_parser.h"

/* Helper to get first label string from a node pattern */
static const char* get_first_label(cypher_node_pattern *node)
{
    if (!node || !node->labels || node->labels->count == 0) return NULL;
    ast_node *label_node = node->labels->items[0];
    if (!label_node || label_node->type != AST_NODE_LITERAL) return NULL;
    cypher_literal *lit = (cypher_literal*)label_node;
    if (lit->literal_type != LITERAL_STRING) return NULL;
    return lit->value.string;
}

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

/* Test ORDER BY DESC parsing */
static void test_order_by_desc_parsing(void)
{
    /* Test that ASC keyword is being properly recognized */
    int asc_token = cypher_keyword_lookup("asc");
    int desc_token = cypher_keyword_lookup("desc");
    printf("\nASC token: %d, DESC token: %d\n", asc_token, desc_token);
    CU_ASSERT(asc_token >= 0);
    CU_ASSERT(desc_token >= 0);
    
    /* Test simple ORDER BY (should work) */
    const char *simple_query = "MATCH (n) RETURN n ORDER BY n.name";
    ast_node *result = parse_cypher_query(simple_query);
    if (!result) {
        printf("\nSimple ORDER BY parse failed\n");
    } else {
        printf("\nSimple ORDER BY query parsed successfully\n");
    }
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_parser_free_result(result);
    }
    
    /* Test ORDER BY ASC */
    const char *asc_query = "MATCH (n) RETURN n ORDER BY n.name ASC";
    result = parse_cypher_query(asc_query);
    if (!result) {
        printf("\nORDER BY ASC parse failed\n");
    } else {
        printf("\nORDER BY ASC query parsed successfully\n");
    }
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
        cypher_parser_free_result(result);
    }
    
    /* Test ORDER BY DESC */
    const char *desc_query = "MATCH (n) RETURN n ORDER BY n.name DESC";
    result = parse_cypher_query(desc_query);
    if (!result) {
        printf("\nORDER BY DESC parse failed\n");
    } else {
        printf("\nORDER BY DESC query parsed successfully\n");
    }
    CU_ASSERT_PTR_NOT_NULL(result);
    if (result) {
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
        
        CU_ASSERT_PTR_NOT_NULL(node->labels);
        CU_ASSERT_EQUAL(node->labels->count, 1);
        CU_ASSERT_STRING_EQUAL(get_first_label(node), "Person");
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
        CU_ASSERT_PTR_NOT_NULL(node->labels);
        CU_ASSERT_EQUAL(node->labels->count, 1);
        CU_ASSERT_STRING_EQUAL(get_first_label(node), "Person");
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
        CU_ASSERT_PTR_NOT_NULL(node->labels);
        CU_ASSERT_EQUAL(node->labels->count, 1);
        CU_ASSERT_STRING_EQUAL(get_first_label(node), "Person");
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
        CU_ASSERT_PTR_NULL(node->labels); /* No label */
        CU_ASSERT_PTR_NOT_NULL(node->properties);

        cypher_parser_free_result(result);
    }
}

/* Test CREATE with multiple labels */
static void test_create_multiple_labels(void)
{
    const char *query = "CREATE (n:Person:Employee {name: 'Alice'})";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];

        CU_ASSERT_PTR_NOT_NULL(node->labels);
        CU_ASSERT_EQUAL(node->labels->count, 2);
        CU_ASSERT_STRING_EQUAL(get_first_label(node), "Person");

        /* Check second label */
        cypher_literal *second_label = (cypher_literal*)node->labels->items[1];
        CU_ASSERT_PTR_NOT_NULL(second_label);
        CU_ASSERT_STRING_EQUAL(second_label->value.string, "Employee");

        cypher_parser_free_result(result);
    }
}

/* Test CREATE with three labels */
static void test_create_three_labels(void)
{
    const char *query = "CREATE (n:Person:Employee:Manager)";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_create *create = (cypher_create*)query_ast->clauses->items[0];
        cypher_path *path = (cypher_path*)create->pattern->items[0];
        cypher_node_pattern *node = (cypher_node_pattern*)path->elements->items[0];

        CU_ASSERT_PTR_NOT_NULL(node->labels);
        CU_ASSERT_EQUAL(node->labels->count, 3);

        /* Check all three labels */
        CU_ASSERT_STRING_EQUAL(get_first_label(node), "Person");
        cypher_literal *second_label = (cypher_literal*)node->labels->items[1];
        CU_ASSERT_STRING_EQUAL(second_label->value.string, "Employee");
        cypher_literal *third_label = (cypher_literal*)node->labels->items[2];
        CU_ASSERT_STRING_EQUAL(third_label->value.string, "Manager");

        cypher_parser_free_result(result);
    }
}

/* Test REMOVE property parsing */
static void test_remove_property_parsing(void)
{
    const char *query = "MATCH (n) REMOVE n.age RETURN n";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 3); /* MATCH, REMOVE, RETURN */

        /* Check that second clause is REMOVE */
        ast_node *remove_node = query_ast->clauses->items[1];
        CU_ASSERT_EQUAL(remove_node->type, AST_NODE_REMOVE);

        cypher_remove *remove = (cypher_remove*)remove_node;
        CU_ASSERT_EQUAL(remove->items->count, 1);

        /* Check the remove item is a property */
        cypher_remove_item *item = (cypher_remove_item*)remove->items->items[0];
        CU_ASSERT_PTR_NOT_NULL(item->target);
        CU_ASSERT_EQUAL(item->target->type, AST_NODE_PROPERTY);

        cypher_property *prop = (cypher_property*)item->target;
        CU_ASSERT_STRING_EQUAL(prop->property_name, "age");

        cypher_parser_free_result(result);
    }
}

/* Test REMOVE label parsing */
static void test_remove_label_parsing(void)
{
    const char *query = "MATCH (n) REMOVE n:Admin RETURN n";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 3);

        /* Check that second clause is REMOVE */
        ast_node *remove_node = query_ast->clauses->items[1];
        CU_ASSERT_EQUAL(remove_node->type, AST_NODE_REMOVE);

        cypher_remove *remove = (cypher_remove*)remove_node;
        CU_ASSERT_EQUAL(remove->items->count, 1);

        /* Check the remove item is a label expression */
        cypher_remove_item *item = (cypher_remove_item*)remove->items->items[0];
        CU_ASSERT_PTR_NOT_NULL(item->target);
        CU_ASSERT_EQUAL(item->target->type, AST_NODE_LABEL_EXPR);

        cypher_label_expr *label_expr = (cypher_label_expr*)item->target;
        CU_ASSERT_STRING_EQUAL(label_expr->label_name, "Admin");

        cypher_parser_free_result(result);
    }
}

/* Test multiple REMOVE items parsing */
static void test_remove_multiple_items_parsing(void)
{
    const char *query = "MATCH (n) REMOVE n.age, n.name, n:Admin RETURN n";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 3);

        /* Check that second clause is REMOVE */
        ast_node *remove_node = query_ast->clauses->items[1];
        CU_ASSERT_EQUAL(remove_node->type, AST_NODE_REMOVE);

        cypher_remove *remove = (cypher_remove*)remove_node;
        CU_ASSERT_EQUAL(remove->items->count, 3);

        /* First item: n.age */
        cypher_remove_item *item1 = (cypher_remove_item*)remove->items->items[0];
        CU_ASSERT_EQUAL(item1->target->type, AST_NODE_PROPERTY);

        /* Second item: n.name */
        cypher_remove_item *item2 = (cypher_remove_item*)remove->items->items[1];
        CU_ASSERT_EQUAL(item2->target->type, AST_NODE_PROPERTY);

        /* Third item: n:Admin */
        cypher_remove_item *item3 = (cypher_remove_item*)remove->items->items[2];
        CU_ASSERT_EQUAL(item3->target->type, AST_NODE_LABEL_EXPR);

        cypher_parser_free_result(result);
    }
}

/* Test regex matching operator parsing */
static void test_regex_match_parsing(void)
{
    const char *query = "MATCH (n) WHERE n.name =~ \"A.*\" RETURN n";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2); /* MATCH (with WHERE), RETURN */

        /* Get the WHERE clause expression from MATCH */
        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
        CU_ASSERT_PTR_NOT_NULL(match->where);

        /* The WHERE expression should be a binary operation */
        CU_ASSERT_EQUAL(match->where->type, AST_NODE_BINARY_OP);

        cypher_binary_op *binary = (cypher_binary_op*)match->where;
        CU_ASSERT_EQUAL(binary->op_type, BINARY_OP_REGEX_MATCH);

        /* Left should be a property access */
        CU_ASSERT_EQUAL(binary->left->type, AST_NODE_PROPERTY);

        /* Right should be a string literal */
        CU_ASSERT_EQUAL(binary->right->type, AST_NODE_LITERAL);

        cypher_parser_free_result(result);
    }
}

/* Test modulo operator parsing */
static void test_modulo_operator_parsing(void)
{
    const char *query = "RETURN 10 % 3";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 1); /* RETURN */

        /* Get the RETURN clause */
        cypher_return *ret = (cypher_return*)query_ast->clauses->items[0];
        CU_ASSERT_PTR_NOT_NULL(ret->items);
        CU_ASSERT_EQUAL(ret->items->count, 1);

        /* Get the return item expression */
        cypher_return_item *item = (cypher_return_item*)ret->items->items[0];
        CU_ASSERT_PTR_NOT_NULL(item->expr);

        /* The expression should be a binary operation */
        CU_ASSERT_EQUAL(item->expr->type, AST_NODE_BINARY_OP);

        cypher_binary_op *binary = (cypher_binary_op*)item->expr;
        CU_ASSERT_EQUAL(binary->op_type, BINARY_OP_MOD);

        /* Left should be integer literal 10 */
        CU_ASSERT_EQUAL(binary->left->type, AST_NODE_LITERAL);
        cypher_literal *left_lit = (cypher_literal*)binary->left;
        CU_ASSERT_EQUAL(left_lit->literal_type, LITERAL_INTEGER);
        CU_ASSERT_EQUAL(left_lit->value.integer, 10);

        /* Right should be integer literal 3 */
        CU_ASSERT_EQUAL(binary->right->type, AST_NODE_LITERAL);
        cypher_literal *right_lit = (cypher_literal*)binary->right;
        CU_ASSERT_EQUAL(right_lit->literal_type, LITERAL_INTEGER);
        CU_ASSERT_EQUAL(right_lit->value.integer, 3);

        cypher_parser_free_result(result);
        printf("Modulo operator parsing test passed\n");
    }
}

/* Test FOREACH clause parsing */
static void test_foreach_parsing(void)
{
    const char *query = "FOREACH (x IN [1, 2, 3] | CREATE (n {val: x}))";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 1); /* FOREACH */

        /* Get the FOREACH clause */
        ast_node *clause = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(clause->type, AST_NODE_FOREACH);

        cypher_foreach *foreach = (cypher_foreach*)clause;
        CU_ASSERT_PTR_NOT_NULL(foreach->variable);
        CU_ASSERT_STRING_EQUAL(foreach->variable, "x");

        /* List expression should be a list */
        CU_ASSERT_PTR_NOT_NULL(foreach->list_expr);
        CU_ASSERT_EQUAL(foreach->list_expr->type, AST_NODE_LIST);

        /* Body should have 1 clause (CREATE) */
        CU_ASSERT_PTR_NOT_NULL(foreach->body);
        CU_ASSERT_EQUAL(foreach->body->count, 1);

        ast_node *body_clause = foreach->body->items[0];
        CU_ASSERT_EQUAL(body_clause->type, AST_NODE_CREATE);

        cypher_parser_free_result(result);
        printf("FOREACH clause parsing test passed\n");
    }
}

/* Test nested FOREACH parsing */
static void test_foreach_nested_parsing(void)
{
    const char *query = "FOREACH (x IN [1, 2] | FOREACH (y IN [3, 4] | CREATE (n {x: x, y: y})))";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 1);

        cypher_foreach *outer = (cypher_foreach*)query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(outer->base.type, AST_NODE_FOREACH);
        CU_ASSERT_STRING_EQUAL(outer->variable, "x");

        /* Body should have nested FOREACH */
        CU_ASSERT_EQUAL(outer->body->count, 1);
        ast_node *inner_node = outer->body->items[0];
        CU_ASSERT_EQUAL(inner_node->type, AST_NODE_FOREACH);

        cypher_foreach *inner = (cypher_foreach*)inner_node;
        CU_ASSERT_STRING_EQUAL(inner->variable, "y");

        cypher_parser_free_result(result);
        printf("Nested FOREACH parsing test passed\n");
    }
}

/* Test LOAD CSV basic parsing */
static void test_load_csv_parsing(void)
{
    const char *query = "LOAD CSV FROM 'data.csv' AS row RETURN row";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2); /* LOAD CSV + RETURN */

        /* Get the LOAD CSV clause */
        ast_node *clause = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(clause->type, AST_NODE_LOAD_CSV);

        cypher_load_csv *load_csv = (cypher_load_csv*)clause;
        CU_ASSERT_PTR_NOT_NULL(load_csv->file_path);
        CU_ASSERT_STRING_EQUAL(load_csv->file_path, "data.csv");
        CU_ASSERT_PTR_NOT_NULL(load_csv->variable);
        CU_ASSERT_STRING_EQUAL(load_csv->variable, "row");
        CU_ASSERT_FALSE(load_csv->with_headers);
        CU_ASSERT_PTR_NULL(load_csv->fieldterminator);

        cypher_parser_free_result(result);
        printf("LOAD CSV basic parsing test passed\n");
    }
}

/* Test LOAD CSV WITH HEADERS parsing */
static void test_load_csv_with_headers_parsing(void)
{
    const char *query = "LOAD CSV WITH HEADERS FROM 'users.csv' AS user RETURN user.name";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2); /* LOAD CSV + RETURN */

        /* Get the LOAD CSV clause */
        ast_node *clause = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(clause->type, AST_NODE_LOAD_CSV);

        cypher_load_csv *load_csv = (cypher_load_csv*)clause;
        CU_ASSERT_PTR_NOT_NULL(load_csv->file_path);
        CU_ASSERT_STRING_EQUAL(load_csv->file_path, "users.csv");
        CU_ASSERT_PTR_NOT_NULL(load_csv->variable);
        CU_ASSERT_STRING_EQUAL(load_csv->variable, "user");
        CU_ASSERT_TRUE(load_csv->with_headers);
        CU_ASSERT_PTR_NULL(load_csv->fieldterminator);

        cypher_parser_free_result(result);
        printf("LOAD CSV WITH HEADERS parsing test passed\n");
    }
}

/* Test LOAD CSV with FIELDTERMINATOR parsing */
static void test_load_csv_fieldterminator_parsing(void)
{
    const char *query = "LOAD CSV FROM 'data.csv' AS row FIELDTERMINATOR ';' RETURN row";

    ast_node *result = parse_cypher_query(query);
    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2); /* LOAD CSV + RETURN */

        /* Get the LOAD CSV clause */
        ast_node *clause = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(clause->type, AST_NODE_LOAD_CSV);

        cypher_load_csv *load_csv = (cypher_load_csv*)clause;
        CU_ASSERT_PTR_NOT_NULL(load_csv->file_path);
        CU_ASSERT_STRING_EQUAL(load_csv->file_path, "data.csv");
        CU_ASSERT_PTR_NOT_NULL(load_csv->variable);
        CU_ASSERT_STRING_EQUAL(load_csv->variable, "row");
        CU_ASSERT_FALSE(load_csv->with_headers);
        CU_ASSERT_PTR_NOT_NULL(load_csv->fieldterminator);
        CU_ASSERT_STRING_EQUAL(load_csv->fieldterminator, ";");

        cypher_parser_free_result(result);
        printf("LOAD CSV FIELDTERMINATOR parsing test passed\n");
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

/* DELETE clause parsing tests */
static void test_delete_clause_parsing(void)
{
    const char *query = "MATCH (a)-[r:KNOWS]->(b) DELETE r";
    
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    if (!result) return;
    
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
    
    /* Should be a query with 2 clauses: MATCH and DELETE */
    cypher_query *query_ast = (cypher_query*)result;
    CU_ASSERT_PTR_NOT_NULL(query_ast->clauses);
    CU_ASSERT_EQUAL(query_ast->clauses->count, 2);
    
    /* First clause should be MATCH */
    ast_node *first_clause = query_ast->clauses->items[0];
    CU_ASSERT_EQUAL(first_clause->type, AST_NODE_MATCH);
    
    /* Second clause should be DELETE */
    ast_node *second_clause = query_ast->clauses->items[1];
    CU_ASSERT_EQUAL(second_clause->type, AST_NODE_DELETE);
    
    /* Validate DELETE clause structure */
    cypher_delete *delete_clause = (cypher_delete*)second_clause;
    CU_ASSERT_PTR_NOT_NULL(delete_clause->items);
    CU_ASSERT_EQUAL(delete_clause->items->count, 1);
    
    /* Validate DELETE item */
    cypher_delete_item *item = (cypher_delete_item*)delete_clause->items->items[0];
    CU_ASSERT_PTR_NOT_NULL(item);
    CU_ASSERT_PTR_NOT_NULL(item->variable);
    CU_ASSERT_STRING_EQUAL(item->variable, "r");
    
    printf("DELETE clause parsing test passed: variable='%s'\n", item->variable);
    
    cypher_parser_free_result(result);
}

static void test_delete_node_parsing(void)
{
    const char *query = "MATCH (n:Person) DELETE n";
    
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
    
    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2);
        
        ast_node *delete_clause_node = query_ast->clauses->items[1];
        CU_ASSERT_EQUAL(delete_clause_node->type, AST_NODE_DELETE);
        
        cypher_delete *delete_clause = (cypher_delete*)delete_clause_node;
        CU_ASSERT_EQUAL(delete_clause->items->count, 1);
        
        cypher_delete_item *item = (cypher_delete_item*)delete_clause->items->items[0];
        CU_ASSERT_STRING_EQUAL(item->variable, "n");
        
        printf("DELETE node parsing test passed: variable='%s'\n", item->variable);
        
        cypher_parser_free_result(result);
    }
}

static void test_detach_delete_parsing(void)
{
    const char *query = "MATCH (n:Person) DELETE n";  /* Skip DETACH for now since it's not implemented */
    
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
    
    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        
        ast_node *delete_clause_node = query_ast->clauses->items[1];
        cypher_delete *delete_clause = (cypher_delete*)delete_clause_node;
        
        /* Just verify basic DELETE works for now */
        CU_ASSERT_EQUAL(delete_clause->items->count, 1);
        
        printf("Basic DELETE parsing test passed (DETACH not yet implemented)\n");
        
        cypher_parser_free_result(result);
    }
}

/* Test OPTIONAL MATCH parsing */
static void test_optional_match_parsing(void)
{
    /* Test 1: Simple OPTIONAL MATCH */
    const char *query1 = "MATCH (p:Person) OPTIONAL MATCH (p)-[:MANAGES]->(subordinate) RETURN p.name, subordinate.name";
    
    ast_node *result1 = parse_cypher_query(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    CU_ASSERT_EQUAL(result1->type, AST_NODE_QUERY);
    
    if (result1) {
        cypher_query *query_ast = (cypher_query*)result1;
        
        /* Should have 3 clauses: MATCH, OPTIONAL MATCH, RETURN */
        CU_ASSERT_EQUAL(query_ast->clauses->count, 3);
        
        /* Check first MATCH clause */
        ast_node *match1_node = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(match1_node->type, AST_NODE_MATCH);
        cypher_match *match1 = (cypher_match*)match1_node;
        CU_ASSERT_FALSE(match1->optional); /* Regular MATCH */
        
        /* Check OPTIONAL MATCH clause */
        ast_node *match2_node = query_ast->clauses->items[1];
        CU_ASSERT_EQUAL(match2_node->type, AST_NODE_MATCH);
        cypher_match *match2 = (cypher_match*)match2_node;
        CU_ASSERT_TRUE(match2->optional); /* OPTIONAL MATCH */
        
        /* Check RETURN clause */
        ast_node *return_node = query_ast->clauses->items[2];
        CU_ASSERT_EQUAL(return_node->type, AST_NODE_RETURN);
        
        printf("OPTIONAL MATCH parsing test passed: optional flag=%s\n", 
               match2->optional ? "true" : "false");
        
        cypher_parser_free_result(result1);
    }
    
    /* Test 2: Standalone OPTIONAL MATCH */
    const char *query2 = "OPTIONAL MATCH (n)-[r]->(m) RETURN n, r, m";
    
    ast_node *result2 = parse_cypher_query(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);
    
    if (result2) {
        cypher_query *query_ast = (cypher_query*)result2;
        
        /* Should have 2 clauses: OPTIONAL MATCH, RETURN */
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2);
        
        /* Check OPTIONAL MATCH clause */
        ast_node *match_node = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(match_node->type, AST_NODE_MATCH);
        cypher_match *match = (cypher_match*)match_node;
        CU_ASSERT_TRUE(match->optional); /* OPTIONAL MATCH */
        
        printf("Standalone OPTIONAL MATCH parsing test passed\n");

        cypher_parser_free_result(result2);
    }
}

/* Test MATCH FROM clause parsing for multi-graph support */
static void test_match_from_clause_parsing(void)
{
    /* Test 1: Simple MATCH FROM */
    const char *query1 = "MATCH (n:Person) FROM social RETURN n.name";

    ast_node *result1 = parse_cypher_query(query1);
    CU_ASSERT_PTR_NOT_NULL(result1);
    CU_ASSERT_EQUAL(result1->type, AST_NODE_QUERY);

    if (result1) {
        cypher_query *query_ast = (cypher_query*)result1;

        /* Should have 2 clauses: MATCH, RETURN */
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2);

        /* Check MATCH clause has from_graph set */
        ast_node *match_node = query_ast->clauses->items[0];
        CU_ASSERT_EQUAL(match_node->type, AST_NODE_MATCH);
        cypher_match *match = (cypher_match*)match_node;
        CU_ASSERT_PTR_NOT_NULL(match->from_graph);
        CU_ASSERT_STRING_EQUAL(match->from_graph, "social");
        CU_ASSERT_FALSE(match->optional);

        printf("MATCH FROM parsing test passed: from_graph=%s\n", match->from_graph);

        cypher_parser_free_result(result1);
    }

    /* Test 2: MATCH FROM with WHERE */
    const char *query2 = "MATCH (n:Person) FROM mygraph WHERE n.age > 21 RETURN n";

    ast_node *result2 = parse_cypher_query(query2);
    CU_ASSERT_PTR_NOT_NULL(result2);

    if (result2) {
        cypher_query *query_ast = (cypher_query*)result2;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 2);

        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
        CU_ASSERT_PTR_NOT_NULL(match->from_graph);
        CU_ASSERT_STRING_EQUAL(match->from_graph, "mygraph");
        CU_ASSERT_PTR_NOT_NULL(match->where); /* WHERE should be set */

        printf("MATCH FROM with WHERE parsing test passed\n");

        cypher_parser_free_result(result2);
    }

    /* Test 3: OPTIONAL MATCH FROM */
    const char *query3 = "OPTIONAL MATCH (n:Person) FROM analytics RETURN n";

    ast_node *result3 = parse_cypher_query(query3);
    CU_ASSERT_PTR_NOT_NULL(result3);

    if (result3) {
        cypher_query *query_ast = (cypher_query*)result3;

        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
        CU_ASSERT_TRUE(match->optional);
        CU_ASSERT_PTR_NOT_NULL(match->from_graph);
        CU_ASSERT_STRING_EQUAL(match->from_graph, "analytics");

        printf("OPTIONAL MATCH FROM parsing test passed\n");

        cypher_parser_free_result(result3);
    }

    /* Test 4: MATCH without FROM (backward compatibility) */
    const char *query4 = "MATCH (n:Person) RETURN n";

    ast_node *result4 = parse_cypher_query(query4);
    CU_ASSERT_PTR_NOT_NULL(result4);

    if (result4) {
        cypher_query *query_ast = (cypher_query*)result4;

        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
        CU_ASSERT_PTR_NULL(match->from_graph); /* No FROM clause */

        printf("MATCH without FROM backward compat test passed\n");

        cypher_parser_free_result(result4);
    }
}

/* Test multiple relationship type syntax */
static void test_multiple_relationship_types(void)
{
    const char *query = "MATCH (a)-[:WORKS_FOR|CONSULTS_FOR]->(b) RETURN a, b";
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
    
    cypher_query *query_ast = (cypher_query*)result;
    CU_ASSERT_PTR_NOT_NULL(query_ast->clauses);
    CU_ASSERT_EQUAL(query_ast->clauses->count, 2); /* MATCH and RETURN */
    
    /* Validate MATCH clause */
    ast_node *match_node = query_ast->clauses->items[0];
    CU_ASSERT_EQUAL(match_node->type, AST_NODE_MATCH);
    
    cypher_match *match = (cypher_match*)match_node;
    CU_ASSERT_PTR_NOT_NULL(match->pattern);
    CU_ASSERT_EQUAL(match->pattern->count, 1);
    
    /* Validate path structure */
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    CU_ASSERT_PTR_NOT_NULL(path->elements);
    CU_ASSERT_EQUAL(path->elements->count, 3); /* node, rel, node */
    
    /* Validate relationship with multiple types */
    ast_node *rel_node = path->elements->items[1];
    CU_ASSERT_EQUAL(rel_node->type, AST_NODE_REL_PATTERN);
    
    cypher_rel_pattern *rel = (cypher_rel_pattern*)rel_node;
    CU_ASSERT_PTR_NULL(rel->type); /* Should be NULL for multi-type */
    CU_ASSERT_PTR_NOT_NULL(rel->types); /* Should have types list */
    CU_ASSERT_EQUAL(rel->types->count, 2);
    
    /* Validate the two relationship types */
    cypher_literal *type1 = (cypher_literal*)rel->types->items[0];
    cypher_literal *type2 = (cypher_literal*)rel->types->items[1];
    CU_ASSERT_EQUAL(type1->literal_type, LITERAL_STRING);
    CU_ASSERT_EQUAL(type2->literal_type, LITERAL_STRING);
    CU_ASSERT_STRING_EQUAL(type1->value.string, "WORKS_FOR");
    CU_ASSERT_STRING_EQUAL(type2->value.string, "CONSULTS_FOR");
    
    cypher_parser_free_result(result);
}

/* Test three relationship types */
static void test_three_relationship_types(void)
{
    const char *query = "MATCH (a)-[:TYPE1|TYPE2|TYPE3]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    
    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];
    
    CU_ASSERT_PTR_NOT_NULL(rel->types);
    CU_ASSERT_EQUAL(rel->types->count, 3);
    
    cypher_literal *type1 = (cypher_literal*)rel->types->items[0];
    cypher_literal *type2 = (cypher_literal*)rel->types->items[1];
    cypher_literal *type3 = (cypher_literal*)rel->types->items[2];
    
    CU_ASSERT_STRING_EQUAL(type1->value.string, "TYPE1");
    CU_ASSERT_STRING_EQUAL(type2->value.string, "TYPE2");
    CU_ASSERT_STRING_EQUAL(type3->value.string, "TYPE3");
    
    cypher_parser_free_result(result);
}

static void test_path_variable_assignment(void)
{
    const char *query = "MATCH path = (a)-[r]->(b) RETURN path";
    ast_node *result = parse_cypher_query(query);
    
    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
    
    cypher_query *query_ast = (cypher_query*)result;
    CU_ASSERT_PTR_NOT_NULL(query_ast->clauses);
    CU_ASSERT_EQUAL(query_ast->clauses->count, 2); /* MATCH and RETURN */
    
    /* Validate MATCH clause */
    ast_node *match_node = query_ast->clauses->items[0];
    CU_ASSERT_EQUAL(match_node->type, AST_NODE_MATCH);
    
    cypher_match *match = (cypher_match*)match_node;
    CU_ASSERT_PTR_NOT_NULL(match->pattern);
    CU_ASSERT_EQUAL(match->pattern->count, 1);
    
    /* Validate path has variable name assigned */
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    CU_ASSERT_PTR_NOT_NULL(path);
    CU_ASSERT_EQUAL(path->base.type, AST_NODE_PATH);
    CU_ASSERT_PTR_NOT_NULL(path->var_name);
    CU_ASSERT_STRING_EQUAL(path->var_name, "path");
    
    /* Validate path structure: node, rel, node */
    CU_ASSERT_PTR_NOT_NULL(path->elements);
    CU_ASSERT_EQUAL(path->elements->count, 3);
    
    /* Validate first node */
    ast_node *node1 = path->elements->items[0];
    CU_ASSERT_EQUAL(node1->type, AST_NODE_NODE_PATTERN);
    cypher_node_pattern *np1 = (cypher_node_pattern*)node1;
    CU_ASSERT_STRING_EQUAL(np1->variable, "a");
    
    /* Validate relationship */
    ast_node *rel_node = path->elements->items[1];
    CU_ASSERT_EQUAL(rel_node->type, AST_NODE_REL_PATTERN);
    cypher_rel_pattern *rel = (cypher_rel_pattern*)rel_node;
    CU_ASSERT_STRING_EQUAL(rel->variable, "r");
    
    /* Validate second node */
    ast_node *node2 = path->elements->items[2];
    CU_ASSERT_EQUAL(node2->type, AST_NODE_NODE_PATTERN);
    cypher_node_pattern *np2 = (cypher_node_pattern*)node2;
    CU_ASSERT_STRING_EQUAL(np2->variable, "b");
    
    cypher_parser_free_result(result);
}

/* Variable-length relationship tests */
static void test_varlen_basic(void)
{
    const char *query = "MATCH (a)-[*]->(b) RETURN a, b";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);
    CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];

    /* Validate relationship has varlen */
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];
    CU_ASSERT_PTR_NOT_NULL(rel->varlen);

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 1);  /* Default min */
    CU_ASSERT_EQUAL(range->max_hops, -1); /* Unbounded */

    cypher_parser_free_result(result);
}

static void test_varlen_bounded(void)
{
    const char *query = "MATCH (a)-[*1..5]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 1);
    CU_ASSERT_EQUAL(range->max_hops, 5);

    cypher_parser_free_result(result);
}

static void test_varlen_min_bounded(void)
{
    const char *query = "MATCH (a)-[*2..]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 2);
    CU_ASSERT_EQUAL(range->max_hops, -1); /* Unbounded max */

    cypher_parser_free_result(result);
}

static void test_varlen_max_bounded(void)
{
    const char *query = "MATCH (a)-[*..3]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 1);  /* Default min */
    CU_ASSERT_EQUAL(range->max_hops, 3);

    cypher_parser_free_result(result);
}

static void test_varlen_exact_hops(void)
{
    const char *query = "MATCH (a)-[*3]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 3);
    CU_ASSERT_EQUAL(range->max_hops, 3); /* Exact: min == max */

    cypher_parser_free_result(result);
}

static void test_varlen_with_type(void)
{
    const char *query = "MATCH (a)-[:KNOWS*]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    CU_ASSERT_PTR_NOT_NULL(rel->type);
    CU_ASSERT_STRING_EQUAL(rel->type, "KNOWS");

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 1);
    CU_ASSERT_EQUAL(range->max_hops, -1);

    cypher_parser_free_result(result);
}

static void test_varlen_type_and_range(void)
{
    const char *query = "MATCH (a)-[:KNOWS*1..3]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    CU_ASSERT_PTR_NOT_NULL(rel->type);
    CU_ASSERT_STRING_EQUAL(rel->type, "KNOWS");

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 1);
    CU_ASSERT_EQUAL(range->max_hops, 3);

    cypher_parser_free_result(result);
}

static void test_varlen_with_variable(void)
{
    const char *query = "MATCH (a)-[r*1..5]->(b) RETURN r";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    CU_ASSERT_PTR_NOT_NULL(rel->variable);
    CU_ASSERT_STRING_EQUAL(rel->variable, "r");

    cypher_varlen_range *range = (cypher_varlen_range*)rel->varlen;
    CU_ASSERT_EQUAL(range->min_hops, 1);
    CU_ASSERT_EQUAL(range->max_hops, 5);

    cypher_parser_free_result(result);
}

static void test_varlen_incoming_direction(void)
{
    const char *query = "MATCH (a)<-[*]-(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    CU_ASSERT_TRUE(rel->left_arrow);
    CU_ASSERT_FALSE(rel->right_arrow);

    cypher_parser_free_result(result);
}

static void test_varlen_undirected(void)
{
    const char *query = "MATCH (a)-[*]-(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NOT_NULL(rel->varlen);
    CU_ASSERT_FALSE(rel->left_arrow);
    CU_ASSERT_FALSE(rel->right_arrow);

    cypher_parser_free_result(result);
}

static void test_varlen_no_varlen(void)
{
    /* Verify regular relationships don't have varlen set */
    const char *query = "MATCH (a)-[:KNOWS]->(b) RETURN a";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    cypher_query *query_ast = (cypher_query*)result;
    cypher_match *match = (cypher_match*)query_ast->clauses->items[0];
    cypher_path *path = (cypher_path*)match->pattern->items[0];
    cypher_rel_pattern *rel = (cypher_rel_pattern*)path->elements->items[1];

    CU_ASSERT_PTR_NULL(rel->varlen); /* Should NOT have varlen */
    CU_ASSERT_PTR_NOT_NULL(rel->type);
    CU_ASSERT_STRING_EQUAL(rel->type, "KNOWS");

    cypher_parser_free_result(result);
}

/* Test IS NULL parsing */
static void test_is_null_parsing(void)
{
    const char *query = "MATCH (n) WHERE n.name IS NULL RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];

        /* Check WHERE clause exists */
        CU_ASSERT_PTR_NOT_NULL(match->where);

        /* Check WHERE expression is NULL_CHECK */
        CU_ASSERT_EQUAL(match->where->type, AST_NODE_NULL_CHECK);

        cypher_null_check *null_check = (cypher_null_check*)match->where;
        CU_ASSERT_FALSE(null_check->is_not_null); /* IS NULL, not IS NOT NULL */

        /* Check the expression is a property access */
        CU_ASSERT_EQUAL(null_check->expr->type, AST_NODE_PROPERTY);

        cypher_parser_free_result(result);
    }

    printf("IS NULL parsing test passed\n");
}

/* Test IS NOT NULL parsing */
static void test_is_not_null_parsing(void)
{
    const char *query = "MATCH (n) WHERE n.age IS NOT NULL RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];

        /* Check WHERE clause exists */
        CU_ASSERT_PTR_NOT_NULL(match->where);

        /* Check WHERE expression is NULL_CHECK */
        CU_ASSERT_EQUAL(match->where->type, AST_NODE_NULL_CHECK);

        cypher_null_check *null_check = (cypher_null_check*)match->where;
        CU_ASSERT_TRUE(null_check->is_not_null); /* IS NOT NULL */

        /* Check the expression is a property access */
        CU_ASSERT_EQUAL(null_check->expr->type, AST_NODE_PROPERTY);

        cypher_parser_free_result(result);
    }

    printf("IS NOT NULL parsing test passed\n");
}

/* Test IS NULL with combined conditions */
static void test_is_null_combined(void)
{
    const char *query = "MATCH (n:Person) WHERE n.name IS NOT NULL AND n.age IS NULL RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_match *match = (cypher_match*)query_ast->clauses->items[0];

        /* Check WHERE clause exists */
        CU_ASSERT_PTR_NOT_NULL(match->where);

        /* Check WHERE expression is a binary AND operation */
        CU_ASSERT_EQUAL(match->where->type, AST_NODE_BINARY_OP);

        cypher_binary_op *and_op = (cypher_binary_op*)match->where;
        CU_ASSERT_EQUAL(and_op->op_type, BINARY_OP_AND);

        /* Left side should be IS NOT NULL */
        CU_ASSERT_EQUAL(and_op->left->type, AST_NODE_NULL_CHECK);
        cypher_null_check *left_check = (cypher_null_check*)and_op->left;
        CU_ASSERT_TRUE(left_check->is_not_null);

        /* Right side should be IS NULL */
        CU_ASSERT_EQUAL(and_op->right->type, AST_NODE_NULL_CHECK);
        cypher_null_check *right_check = (cypher_null_check*)and_op->right;
        CU_ASSERT_FALSE(right_check->is_not_null);

        cypher_parser_free_result(result);
    }

    printf("IS NULL combined conditions test passed\n");
}

/* Test basic WITH clause parsing */
static void test_with_clause_basic(void)
{
    const char *query = "MATCH (n) WITH n RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 3);

        /* First clause is MATCH */
        CU_ASSERT_EQUAL(query_ast->clauses->items[0]->type, AST_NODE_MATCH);

        /* Second clause is WITH */
        CU_ASSERT_EQUAL(query_ast->clauses->items[1]->type, AST_NODE_WITH);

        /* Third clause is RETURN */
        CU_ASSERT_EQUAL(query_ast->clauses->items[2]->type, AST_NODE_RETURN);

        cypher_parser_free_result(result);
    }

    printf("WITH clause basic parsing test passed\n");
}

/* Test WITH clause with alias */
static void test_with_clause_alias(void)
{
    const char *query = "MATCH (n) WITH n AS person RETURN person";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        CU_ASSERT_EQUAL(query_ast->clauses->count, 3);

        /* Check WITH clause */
        cypher_with *with = (cypher_with*)query_ast->clauses->items[1];
        CU_ASSERT_EQUAL(with->base.type, AST_NODE_WITH);
        CU_ASSERT_PTR_NOT_NULL(with->items);
        CU_ASSERT_EQUAL(with->items->count, 1);

        /* Check alias */
        cypher_return_item *item = (cypher_return_item*)with->items->items[0];
        CU_ASSERT_PTR_NOT_NULL(item->alias);
        CU_ASSERT_STRING_EQUAL(item->alias, "person");

        cypher_parser_free_result(result);
    }

    printf("WITH clause alias parsing test passed\n");
}

/* Test WITH DISTINCT */
static void test_with_clause_distinct(void)
{
    const char *query = "MATCH (n) WITH DISTINCT n RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_with *with = (cypher_with*)query_ast->clauses->items[1];
        CU_ASSERT_TRUE(with->distinct);

        cypher_parser_free_result(result);
    }

    printf("WITH DISTINCT parsing test passed\n");
}

/* Test WITH clause with WHERE */
static void test_with_clause_where(void)
{
    const char *query = "MATCH (n) WITH n WHERE n.age > 18 RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_with *with = (cypher_with*)query_ast->clauses->items[1];
        CU_ASSERT_PTR_NOT_NULL(with->where);

        cypher_parser_free_result(result);
    }

    printf("WITH clause WHERE parsing test passed\n");
}

/* Test WITH clause with ORDER BY and LIMIT */
static void test_with_clause_order_limit(void)
{
    const char *query = "MATCH (n) WITH n ORDER BY n.name LIMIT 10 RETURN n";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        cypher_query *query_ast = (cypher_query*)result;
        cypher_with *with = (cypher_with*)query_ast->clauses->items[1];
        CU_ASSERT_PTR_NOT_NULL(with->order_by);
        CU_ASSERT_PTR_NOT_NULL(with->limit);

        cypher_parser_free_result(result);
    }

    printf("WITH clause ORDER BY LIMIT parsing test passed\n");
}

/* Test CASE expression basic parsing */
static void test_case_expression_basic(void)
{
    const char *query = "RETURN CASE WHEN true THEN 1 END";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }

    printf("CASE expression basic parsing test passed\n");
}

/* Test CASE expression with ELSE */
static void test_case_expression_else(void)
{
    const char *query = "RETURN CASE WHEN false THEN 1 ELSE 2 END";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }

    printf("CASE expression with ELSE parsing test passed\n");
}

/* Test CASE expression with multiple WHEN clauses */
static void test_case_expression_multiple_when(void)
{
    const char *query = "RETURN CASE WHEN 1 = 2 THEN 'a' WHEN 2 = 2 THEN 'b' ELSE 'c' END";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }

    printf("CASE expression with multiple WHEN clauses test passed\n");
}

/* Test CASE expression with property access */
static void test_case_expression_with_property(void)
{
    const char *query = "MATCH (n) RETURN CASE WHEN n.age > 18 THEN 'adult' ELSE 'minor' END";
    ast_node *result = parse_cypher_query(query);

    CU_ASSERT_PTR_NOT_NULL(result);

    if (result) {
        CU_ASSERT_EQUAL(result->type, AST_NODE_QUERY);
        cypher_parser_free_result(result);
    }

    printf("CASE expression with property access test passed\n");
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
        !CU_add_test(suite, "ORDER BY DESC parsing", test_order_by_desc_parsing) ||
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
        !CU_add_test(suite, "Parser complex nesting", test_parser_complex_nesting) ||
        !CU_add_test(suite, "DELETE clause parsing", test_delete_clause_parsing) ||
        !CU_add_test(suite, "DELETE node parsing", test_delete_node_parsing) ||
        !CU_add_test(suite, "DETACH DELETE parsing", test_detach_delete_parsing) ||
        !CU_add_test(suite, "OPTIONAL MATCH parsing", test_optional_match_parsing) ||
        !CU_add_test(suite, "MATCH FROM clause parsing", test_match_from_clause_parsing) ||
        !CU_add_test(suite, "Multiple relationship types", test_multiple_relationship_types) ||
        !CU_add_test(suite, "Three relationship types", test_three_relationship_types) ||
        !CU_add_test(suite, "Path variable assignment", test_path_variable_assignment) ||
        !CU_add_test(suite, "Variable-length basic [*]", test_varlen_basic) ||
        !CU_add_test(suite, "Variable-length bounded [*1..5]", test_varlen_bounded) ||
        !CU_add_test(suite, "Variable-length min bounded [*2..]", test_varlen_min_bounded) ||
        !CU_add_test(suite, "Variable-length max bounded [*..3]", test_varlen_max_bounded) ||
        !CU_add_test(suite, "Variable-length exact hops [*3]", test_varlen_exact_hops) ||
        !CU_add_test(suite, "Variable-length with type [:TYPE*]", test_varlen_with_type) ||
        !CU_add_test(suite, "Variable-length type and range", test_varlen_type_and_range) ||
        !CU_add_test(suite, "Variable-length with variable [r*]", test_varlen_with_variable) ||
        !CU_add_test(suite, "Variable-length incoming direction", test_varlen_incoming_direction) ||
        !CU_add_test(suite, "Variable-length undirected", test_varlen_undirected) ||
        !CU_add_test(suite, "Regular relationship no varlen", test_varlen_no_varlen) ||
        !CU_add_test(suite, "IS NULL parsing", test_is_null_parsing) ||
        !CU_add_test(suite, "IS NOT NULL parsing", test_is_not_null_parsing) ||
        !CU_add_test(suite, "IS NULL combined conditions", test_is_null_combined) ||
        !CU_add_test(suite, "WITH clause basic", test_with_clause_basic) ||
        !CU_add_test(suite, "WITH clause alias", test_with_clause_alias) ||
        !CU_add_test(suite, "WITH DISTINCT", test_with_clause_distinct) ||
        !CU_add_test(suite, "WITH clause WHERE", test_with_clause_where) ||
        !CU_add_test(suite, "WITH clause ORDER BY LIMIT", test_with_clause_order_limit) ||
        !CU_add_test(suite, "CASE expression basic", test_case_expression_basic) ||
        !CU_add_test(suite, "CASE expression with ELSE", test_case_expression_else) ||
        !CU_add_test(suite, "CASE expression multiple WHEN", test_case_expression_multiple_when) ||
        !CU_add_test(suite, "CASE expression with property", test_case_expression_with_property) ||
        !CU_add_test(suite, "CREATE multiple labels", test_create_multiple_labels) ||
        !CU_add_test(suite, "CREATE three labels", test_create_three_labels) ||
        !CU_add_test(suite, "REMOVE property parsing", test_remove_property_parsing) ||
        !CU_add_test(suite, "REMOVE label parsing", test_remove_label_parsing) ||
        !CU_add_test(suite, "REMOVE multiple items parsing", test_remove_multiple_items_parsing) ||
        !CU_add_test(suite, "Regex match operator parsing", test_regex_match_parsing) ||
        !CU_add_test(suite, "Modulo operator parsing", test_modulo_operator_parsing) ||
        !CU_add_test(suite, "FOREACH clause parsing", test_foreach_parsing) ||
        !CU_add_test(suite, "Nested FOREACH parsing", test_foreach_nested_parsing) ||
        !CU_add_test(suite, "LOAD CSV parsing", test_load_csv_parsing) ||
        !CU_add_test(suite, "LOAD CSV WITH HEADERS parsing", test_load_csv_with_headers_parsing) ||
        !CU_add_test(suite, "LOAD CSV FIELDTERMINATOR parsing", test_load_csv_fieldterminator_parsing))
    {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}