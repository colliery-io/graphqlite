#ifndef TEST_CYPHER_GRAMMAR_H
#define TEST_CYPHER_GRAMMAR_H

#include <CUnit/CUnit.h>

#ifdef __cplusplus
extern "C" {
#endif

// Test suite setup function
int add_cypher_grammar_tests(void);

// Basic clause tests
void test_match_clauses(void);
void test_create_clauses(void);
void test_return_clauses(void);
void test_where_clauses(void);
void test_with_clauses(void);
void test_order_by_clauses(void);

// Pattern tests
void test_node_patterns(void);
void test_relationship_patterns(void);
void test_path_patterns(void);
void test_variable_length_patterns(void);

// Expression tests
void test_literal_expressions(void);
void test_arithmetic_expressions(void);
void test_comparison_expressions(void);
void test_logical_expressions(void);
void test_function_expressions(void);
void test_case_expressions(void);

// Complex query tests
void test_composite_statements(void);
void test_union_queries(void);
void test_subqueries(void);
void test_aggregation_queries(void);

// Error handling tests
void test_syntax_errors(void);
void test_invalid_patterns(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_CYPHER_GRAMMAR_H