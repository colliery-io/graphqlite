#ifndef TEST_CYPHER_LEXER_H
#define TEST_CYPHER_LEXER_H

#include <CUnit/CUnit.h>

#ifdef __cplusplus
extern "C" {
#endif

// Test suite setup function
int add_cypher_lexer_tests(void);

// Individual test functions
void test_basic_keywords(void);
void test_basic_literals(void);
void test_punctuation_tokens(void);
void test_string_literals(void);
void test_number_literals(void);
void test_cypher_query_tokens(void);
void test_parser_basic(void);
void test_parser_create(void);
void test_executor_basic(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_CYPHER_LEXER_H