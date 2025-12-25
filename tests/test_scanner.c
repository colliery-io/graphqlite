#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "parser/cypher_scanner.h"
#include "parser/cypher_keywords.h"
#include "parser/cypher_tokens.h"
#include "test_scanner.h"

/* Helper function to create scanner from string */
static CypherScannerState* create_string_scanner(const char *input)
{
    CypherScannerState *scanner = cypher_scanner_create();
    if (scanner) {
        cypher_scanner_set_input_string(scanner, input);
    }
    return scanner;
}

/* Test basic scanner creation and destruction */
static void test_scanner_lifecycle(void)
{
    CypherScannerState *scanner = cypher_scanner_create();
    CU_ASSERT_PTR_NOT_NULL(scanner);
    CU_ASSERT_FALSE(cypher_scanner_has_error(scanner));
    
    cypher_scanner_destroy(scanner);
}

/* Test whitespace and comment handling */
static void test_whitespace_and_comments(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Test whitespace skipping */
    scanner = create_string_scanner("   \t\n\r  match");
    CU_ASSERT_PTR_NOT_NULL(scanner);
    
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_KEYWORD);
    CU_ASSERT_STRING_EQUAL(token.value.string, "match");
    
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test comment skipping */
    scanner = create_string_scanner("// This is a comment\nmatch");
    CU_ASSERT_PTR_NOT_NULL(scanner);
    
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_KEYWORD);
    CU_ASSERT_STRING_EQUAL(token.value.string, "match");
    
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test integer literal scanning */
static void test_integer_literals(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Decimal integer */
    scanner = create_string_scanner("123");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_INTEGER);
    CU_ASSERT_EQUAL(token.value.integer, 123);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Hexadecimal integer */
    scanner = create_string_scanner("0x1F");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_INTEGER);
    CU_ASSERT_EQUAL(token.value.integer, 31);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Zero */
    scanner = create_string_scanner("0");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_INTEGER);
    CU_ASSERT_EQUAL(token.value.integer, 0);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test decimal literal scanning */
static void test_decimal_literals(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Basic decimal */
    scanner = create_string_scanner("123.45");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_DECIMAL);
    CU_ASSERT_DOUBLE_EQUAL(token.value.decimal, 123.45, 0.001);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Scientific notation */
    scanner = create_string_scanner("1.23e-4");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_DECIMAL);
    CU_ASSERT_DOUBLE_EQUAL(token.value.decimal, 0.000123, 0.0000001);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test string literal scanning */
static void test_string_literals(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Double-quoted string */
    scanner = create_string_scanner("\"hello world\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello world");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Single-quoted string */
    scanner = create_string_scanner("'hello world'");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello world");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Empty string */
    scanner = create_string_scanner("\"\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test string escape sequences */
static void test_string_escapes(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Test basic escape sequences */
    scanner = create_string_scanner("\"hello\\nworld\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\nworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test tab escape */
    scanner = create_string_scanner("\"hello\\tworld\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\tworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test carriage return escape */
    scanner = create_string_scanner("\"hello\\rworld\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\rworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test backslash escape */
    scanner = create_string_scanner("\"hello\\\\world\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\\world");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test quote escapes in double-quoted string */
    scanner = create_string_scanner("\"He said \\\"Hello\\\"\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "He said \"Hello\"");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test quote escapes in single-quoted string */
    scanner = create_string_scanner("'He said \\'Hello\\''");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "He said 'Hello'");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test backspace escape */
    scanner = create_string_scanner("\"hello\\bworld\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\bworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test form feed escape */
    scanner = create_string_scanner("\"hello\\fworld\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\fworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test unicode escape - simple ASCII */
    scanner = create_string_scanner("\"hello\\u0041world\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "helloAworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test multiple escapes in one string */
    scanner = create_string_scanner("\"line1\\nline2\\ttab\\\\backslash\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "line1\nline2\ttab\\backslash");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test invalid escape sequence (should be treated as literal) */
    scanner = create_string_scanner("\"hello\\zworld\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_STRING_EQUAL(token.value.string, "hello\\zworld");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test identifier scanning */
static void test_identifiers(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Simple identifier */
    scanner = create_string_scanner("variable_name");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL(token.value.string, "variable_name");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Identifier with numbers */
    scanner = create_string_scanner("var123");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL(token.value.string, "var123");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Backtick-quoted identifier */
    scanner = create_string_scanner("`special name`");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_BQIDENT);
    CU_ASSERT_STRING_EQUAL(token.value.string, "special name");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test keyword recognition */
static void test_keyword_recognition(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Test common keywords */
    const char *keywords[] = {"match", "create", "where", "return", "with", "asc", "desc", NULL};
    
    for (int i = 0; keywords[i]; i++) {
        scanner = create_string_scanner(keywords[i]);
        token = cypher_scanner_next_token(scanner);
        CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_KEYWORD);
        CU_ASSERT_STRING_EQUAL(token.value.string, keywords[i]);
        CU_ASSERT(token.token_id >= 0); /* Should have valid token ID */
        cypher_token_free(&token);
        cypher_scanner_destroy(scanner);
    }
    
    /* Test case insensitive keywords */
    scanner = create_string_scanner("MATCH");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_KEYWORD);
    CU_ASSERT_STRING_EQUAL(token.value.string, "MATCH");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test parameter scanning */
static void test_parameters(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    scanner = create_string_scanner("$param_name");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_PARAMETER);
    CU_ASSERT_STRING_EQUAL(token.value.string, "param_name");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test operator scanning */
static void test_operators(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Multi-character operators */
    struct {
        const char *input;
        CypherTokenType expected_type;
    } operators[] = {
        {"!=", CYPHER_TOKEN_NOT_EQ},
        {"<>", CYPHER_TOKEN_NOT_EQ},
        {"<=", CYPHER_TOKEN_LT_EQ},
        {">=", CYPHER_TOKEN_GT_EQ},
        {"..", CYPHER_TOKEN_DOT_DOT},
        {"::", CYPHER_TOKEN_TYPECAST},
        {"+=", CYPHER_TOKEN_PLUS_EQ},
        {"+", CYPHER_TOKEN_OPERATOR},
        {"*", CYPHER_TOKEN_OPERATOR},
        {"%", CYPHER_TOKEN_OPERATOR},
        {NULL, 0}
    };
    
    for (int i = 0; operators[i].input; i++) {
        scanner = create_string_scanner(operators[i].input);
        token = cypher_scanner_next_token(scanner);
        CU_ASSERT_EQUAL(token.type, operators[i].expected_type);
        cypher_token_free(&token);
        cypher_scanner_destroy(scanner);
    }
    
    /* Single character tokens */
    scanner = create_string_scanner("(");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_CHAR);
    CU_ASSERT_EQUAL(token.value.character, '(');
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test token sequences */
static void test_token_sequences(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    scanner = create_string_scanner("MATCH (n) RETURN n");
    
    /* MATCH keyword */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_KEYWORD);
    CU_ASSERT_STRING_EQUAL(token.value.string, "MATCH");
    cypher_token_free(&token);
    
    /* ( character */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_CHAR);
    CU_ASSERT_EQUAL(token.value.character, '(');
    cypher_token_free(&token);
    
    /* n identifier */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL(token.value.string, "n");
    cypher_token_free(&token);
    
    /* ) character */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_CHAR);
    CU_ASSERT_EQUAL(token.value.character, ')');
    cypher_token_free(&token);
    
    /* RETURN keyword */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_KEYWORD);
    CU_ASSERT_STRING_EQUAL(token.value.string, "RETURN");
    cypher_token_free(&token);
    
    /* n identifier */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL(token.value.string, "n");
    cypher_token_free(&token);
    
    /* EOF */
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_EOF);
    cypher_token_free(&token);
    
    cypher_scanner_destroy(scanner);
}

/* Test error handling */
static void test_error_handling(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Test null scanner */
    token = cypher_scanner_next_token(NULL);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_EOF);
    CU_ASSERT_STRING_EQUAL(token.text, "");
    cypher_token_free(&token);
    
    /* Test unknown character */
    scanner = create_string_scanner("@unknown");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_EOF);
    CU_ASSERT_TRUE(cypher_scanner_has_error(scanner));
    
    const CypherScannerError *error = cypher_scanner_get_error(scanner);
    CU_ASSERT_PTR_NOT_NULL(error);
    CU_ASSERT_PTR_NOT_NULL(error->message);
    
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test null scanner parameter handling */
    CU_ASSERT_TRUE(cypher_scanner_has_error(NULL));
    CU_ASSERT_PTR_NULL(cypher_scanner_get_error(NULL));
}


/* Test edge cases */
static void test_edge_cases(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Test empty input */
    scanner = create_string_scanner("");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_EOF);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test only whitespace */
    scanner = create_string_scanner("   \t\n   ");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_EOF);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test multiple operators */
    scanner = create_string_scanner("+-*/");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_OPERATOR);
    CU_ASSERT_STRING_EQUAL(token.value.string, "+-*/");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test hexadecimal with lowercase */
    scanner = create_string_scanner("0xabc");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_INTEGER);
    CU_ASSERT_EQUAL(token.value.integer, 0xabc);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test scientific notation with positive exponent */
    scanner = create_string_scanner("1.5e+3");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_DECIMAL);
    CU_ASSERT_DOUBLE_EQUAL(token.value.decimal, 1500.0, 0.001);
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test all special characters */
    const char *special_chars = "(){}[],;.";
    for (int i = 0; special_chars[i]; i++) {
        char input[2] = {special_chars[i], '\0'};
        scanner = create_string_scanner(input);
        token = cypher_scanner_next_token(scanner);
        CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_CHAR);
        CU_ASSERT_EQUAL(token.value.character, special_chars[i]);
        cypher_token_free(&token);
        cypher_scanner_destroy(scanner);
    }
    
    /* Test backtick identifier with spaces */
    scanner = create_string_scanner("`node with spaces`");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_BQIDENT);
    CU_ASSERT_STRING_EQUAL(token.value.string, "node with spaces");
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Test scanner state management */
static void test_scanner_state_management(void)
{
    CypherScannerState *scanner;
    
    /* Test destroy with null */
    cypher_scanner_destroy(NULL);
    
    /* Test error clearing */
    scanner = create_string_scanner("@invalid");
    CypherToken token = cypher_scanner_next_token(scanner);
    CU_ASSERT_TRUE(cypher_scanner_has_error(scanner));
    
    cypher_scanner_clear_error(scanner);
    CU_ASSERT_FALSE(cypher_scanner_has_error(scanner));
    
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
    
    /* Test clear error with null */
    cypher_scanner_clear_error(NULL);
    
    /* Test set input string with null parameters */
    scanner = cypher_scanner_create();
    int result = cypher_scanner_set_input_string(NULL, "test");
    CU_ASSERT_EQUAL(result, -1);
    
    result = cypher_scanner_set_input_string(scanner, NULL);
    CU_ASSERT_EQUAL(result, -1);
    
    cypher_scanner_destroy(scanner);
}

/* Test token memory management */
static void test_token_memory_management(void)
{
    CypherScannerState *scanner;
    CypherToken token;
    
    /* Test token freeing with null */
    cypher_token_free(NULL);
    
    /* Test token with string value */
    scanner = create_string_scanner("\"test string\"");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_STRING);
    CU_ASSERT_PTR_NOT_NULL(token.text);
    CU_ASSERT_PTR_NOT_NULL(token.value.string);
    
    /* Free should work properly */
    cypher_token_free(&token);
    CU_ASSERT_PTR_NULL(token.text);
    CU_ASSERT_PTR_NULL(token.value.string);
    
    cypher_scanner_destroy(scanner);
    
    /* Test character token */
    scanner = create_string_scanner("(");
    token = cypher_scanner_next_token(scanner);
    CU_ASSERT_EQUAL(token.type, CYPHER_TOKEN_CHAR);
    CU_ASSERT_EQUAL(token.value.character, '(');
    
    cypher_token_free(&token);
    cypher_scanner_destroy(scanner);
}

/* Helper function to scan all tokens from a query and verify no errors */
static void scan_complete_query(const char *query)
{
    CypherScannerState *scanner = create_string_scanner(query);
    CU_ASSERT_PTR_NOT_NULL(scanner);
    
    CypherToken token;
    int token_count = 0;
    
    do {
        token = cypher_scanner_next_token(scanner);
        CU_ASSERT_FALSE(cypher_scanner_has_error(scanner));
        token_count++;
        cypher_token_free(&token);
        
        /* Safety check to prevent infinite loops */
        CU_ASSERT(token_count < 1000);
        if (token_count >= 1000) break;
        
    } while (token.type != CYPHER_TOKEN_EOF);
    
    CU_ASSERT_FALSE(cypher_scanner_has_error(scanner));
    cypher_scanner_destroy(scanner);
}

/* Test AGE regression test queries from cypher_match.sql */
static void test_age_match_queries(void)
{
    /* Basic node creation and matching */
    scan_complete_query("CREATE (:v)");
    scan_complete_query("CREATE (:v {i: 0})");
    scan_complete_query("CREATE (:v {i: 1})");
    scan_complete_query("MATCH (n:v) RETURN n");
    scan_complete_query("MATCH (n:v) RETURN n.i");
    
    /* Complex property queries */
    scan_complete_query("CREATE ({string_key: \"test\", int_key: 1, float_key: 3.14, map_key: {key: \"value\"}, list_key: [1, 2, 3]})");
    scan_complete_query("CREATE ({lst: [1, NULL, 3.14, \"string\", {key: \"value\"}, []]})");
    scan_complete_query("MATCH (n {string_key: NULL}) RETURN n");
    scan_complete_query("MATCH (n {string_key: \"wrong value\"}) RETURN n");
    scan_complete_query("MATCH (n {string_key: \"test\"}) RETURN n");
}

/* Test AGE regression test queries from cypher_create.sql */
static void test_age_create_queries(void)
{
    /* Basic creation patterns */
    scan_complete_query("CREATE ()");
    scan_complete_query("CREATE (:v)");
    scan_complete_query("CREATE (:v {})");
    scan_complete_query("CREATE (:v {key: 'value'})");
    
    /* Relationship creation */
    scan_complete_query("CREATE (:v {id:\"right rel, initial node\"})-[:e {id:\"right rel\"}]->(:v {id:\"right rel, end node\"})");
    scan_complete_query("CREATE (:v {id:\"left rel, initial node\"})<-[:e {id:\"left rel\"}]-(:v {id:\"left rel, end node\"})");
    
    /* Complex patterns */
    scan_complete_query("CREATE (:v {id:\"middle rel, initial node\"})-[:e {id:\"middle rel\"}]-(:v {id:\"middle rel, end node\"})");
}

/* Test queries with various operators and expressions */
static void test_age_expression_queries(void)
{
    /* Mathematical expressions */
    scan_complete_query("RETURN 1 + 2");
    scan_complete_query("RETURN 5 - 3");
    scan_complete_query("RETURN 4 * 6");
    scan_complete_query("RETURN 10 / 2");
    scan_complete_query("RETURN 10 % 3");
    scan_complete_query("RETURN 2 ^ 3");
    
    /* Comparison operators */
    scan_complete_query("MATCH (n) WHERE n.age > 18 RETURN n");
    scan_complete_query("MATCH (n) WHERE n.age >= 18 RETURN n");
    scan_complete_query("MATCH (n) WHERE n.age < 65 RETURN n");
    scan_complete_query("MATCH (n) WHERE n.age <= 65 RETURN n");
    scan_complete_query("MATCH (n) WHERE n.name = 'John' RETURN n");
    scan_complete_query("MATCH (n) WHERE n.name != 'Jane' RETURN n");
    scan_complete_query("MATCH (n) WHERE n.name <> 'Jane' RETURN n");
    
    /* Logical operators */
    scan_complete_query("MATCH (n) WHERE n.age > 18 AND n.age < 65 RETURN n");
    scan_complete_query("MATCH (n) WHERE n.name = 'John' OR n.name = 'Jane' RETURN n");
    scan_complete_query("MATCH (n) WHERE NOT n.retired RETURN n");
}

/* Test complex cypher queries from AGE tests */
static void test_age_complex_queries(void)
{
    /* WITH clauses */
    scan_complete_query("MATCH (n) WITH n.age AS age WHERE age > 18 RETURN age");
    scan_complete_query("MATCH (n:Person) WITH n ORDER BY n.age LIMIT 5 RETURN n.name");
    
    /* UNION queries */
    scan_complete_query("MATCH (n:Person) RETURN n.name UNION MATCH (n:Company) RETURN n.name");
    scan_complete_query("MATCH (n:Person) RETURN n.name UNION ALL MATCH (n:Person) RETURN n.name");
    
    /* Subqueries */
    scan_complete_query("MATCH (n:Person) WHERE EXISTS { MATCH (n)-[:FRIEND]->(f:Person) } RETURN n");
    
    /* Variable length paths */
    scan_complete_query("MATCH (a)-[*1..3]->(b) RETURN a, b");
    scan_complete_query("MATCH (a)-[r*2..5]->(b) RETURN a, r, b");
    
    /* Complex patterns */
    scan_complete_query("MATCH (a:Person)-[:FRIEND]->(b:Person)-[:WORKS_FOR]->(c:Company) RETURN a.name, c.name");
}

/* Test edge cases and boundary conditions */
static void test_age_edge_cases(void)
{
    /* Empty patterns */
    scan_complete_query("RETURN {}");
    scan_complete_query("RETURN []");
    
    /* Nested structures */
    scan_complete_query("RETURN {a: {b: {c: 'nested'}}}");
    scan_complete_query("RETURN [1, [2, [3, 4]], 5]");
    
    /* Mixed quotes and escapes */
    scan_complete_query("RETURN \"'mixed quotes'\"");
    scan_complete_query("RETURN '\"mixed quotes\"'");
    
    /* Large numbers */
    scan_complete_query("RETURN 9223372036854775807");
    scan_complete_query("RETURN 1.7976931348623157e+308");
    
    /* Keywords as identifiers (backtick quoted) */
    scan_complete_query("MATCH (`match`:`create`) RETURN `match`");
    scan_complete_query("CREATE (:`return` {`where`: 'value'})");
}

/* Test utility functions */
static void test_utility_functions(void)
{
    /* Test token type names */
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_EOF), "EOF");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_INTEGER), "INTEGER");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_DECIMAL), "DECIMAL");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_STRING), "STRING");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_IDENTIFIER), "IDENTIFIER");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_PARAMETER), "PARAMETER");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_BQIDENT), "BQIDENT");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_OPERATOR), "OPERATOR");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_CHAR), "CHAR");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_NOT_EQ), "NOT_EQ");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_LT_EQ), "LT_EQ");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_GT_EQ), "GT_EQ");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_DOT_DOT), "DOT_DOT");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_TYPECAST), "TYPECAST");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_PLUS_EQ), "PLUS_EQ");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(CYPHER_TOKEN_KEYWORD), "KEYWORD");
    CU_ASSERT_STRING_EQUAL(cypher_token_type_name(999), "UNKNOWN");
}

/* Initialize the scanner test suite */
int init_scanner_suite(void)
{
    CU_pSuite suite = CU_add_suite("Scanner", NULL, NULL);
    if (!suite) {
        return CU_get_error();
    }
    
    /* Add tests to suite */
    if (!CU_add_test(suite, "Scanner lifecycle", test_scanner_lifecycle) ||
        !CU_add_test(suite, "Whitespace and comments", test_whitespace_and_comments) ||
        !CU_add_test(suite, "Integer literals", test_integer_literals) ||
        !CU_add_test(suite, "Decimal literals", test_decimal_literals) ||
        !CU_add_test(suite, "String literals", test_string_literals) ||
        !CU_add_test(suite, "String escape sequences", test_string_escapes) ||
        !CU_add_test(suite, "Identifiers", test_identifiers) ||
        !CU_add_test(suite, "Keyword recognition", test_keyword_recognition) ||
        !CU_add_test(suite, "Parameters", test_parameters) ||
        !CU_add_test(suite, "Operators", test_operators) ||
        !CU_add_test(suite, "Token sequences", test_token_sequences) ||
        !CU_add_test(suite, "Error handling", test_error_handling) ||
        !CU_add_test(suite, "Edge cases", test_edge_cases) ||
        !CU_add_test(suite, "Scanner state management", test_scanner_state_management) ||
        !CU_add_test(suite, "Token memory management", test_token_memory_management) ||
        !CU_add_test(suite, "AGE Match Queries", test_age_match_queries) ||
        !CU_add_test(suite, "AGE Create Queries", test_age_create_queries) ||
        !CU_add_test(suite, "AGE Expression Queries", test_age_expression_queries) ||
        !CU_add_test(suite, "AGE Complex Queries", test_age_complex_queries) ||
        !CU_add_test(suite, "AGE Edge Cases", test_age_edge_cases) ||
        !CU_add_test(suite, "Utility functions", test_utility_functions))
    {
        return CU_get_error();
    }
    
    return CUE_SUCCESS;
}