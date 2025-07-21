#include "../src/gql/gql_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test helper functions
void test_case(const char *test_name, bool (*test_func)(void)) {
    printf("Testing %s... ", test_name);
    if (test_func()) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}

// Test cases
bool test_simple_match_query(void) {
    const char *query = "MATCH (n:Person) RETURN n.name";
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    bool success = (ast != NULL && 
                   ast->type == GQL_AST_MATCH_QUERY &&
                   !gql_parser_has_error(parser));
    
    if (ast) {
        printf("\nAST for: %s\n", query);
        gql_ast_print(ast, 0);
        gql_ast_free_recursive(ast);
    } else if (gql_parser_has_error(parser)) {
        printf("\nParser error: %s\n", gql_parser_get_error(parser));
    }
    
    gql_parser_destroy(parser);
    return success;
}

bool test_match_with_edge(void) {
    const char *query = "MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a.name, b.name";
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    bool success = (ast != NULL && 
                   ast->type == GQL_AST_MATCH_QUERY &&
                   !gql_parser_has_error(parser));
    
    if (ast) {
        printf("\nAST for: %s\n", query);
        gql_ast_print(ast, 0);
        gql_ast_free_recursive(ast);
    } else if (gql_parser_has_error(parser)) {
        printf("\nParser error: %s\n", gql_parser_get_error(parser));
    }
    
    gql_parser_destroy(parser);
    return success;
}

bool test_match_with_where(void) {
    const char *query = "MATCH (n:Person) WHERE n.age > 25 RETURN n.name";
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    bool success = (ast != NULL && 
                   ast->type == GQL_AST_MATCH_QUERY &&
                   !gql_parser_has_error(parser));
    
    if (ast) {
        printf("\nAST for: %s\n", query);
        gql_ast_print(ast, 0);
        gql_ast_free_recursive(ast);
    } else if (gql_parser_has_error(parser)) {
        printf("\nParser error: %s\n", gql_parser_get_error(parser));
    }
    
    gql_parser_destroy(parser);
    return success;
}

bool test_create_query(void) {
    const char *query = "CREATE (n:Person {name: \"Alice\", age: 30})";
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    bool success = (ast != NULL && 
                   ast->type == GQL_AST_CREATE_QUERY &&
                   !gql_parser_has_error(parser));
    
    if (ast) {
        printf("\nAST for: %s\n", query);
        gql_ast_print(ast, 0);
        gql_ast_free_recursive(ast);
    } else if (gql_parser_has_error(parser)) {
        printf("\nParser error: %s\n", gql_parser_get_error(parser));
    }
    
    gql_parser_destroy(parser);
    return success;
}

bool test_create_with_edge(void) {
    const char *query = "CREATE (a:Person {name: \"Alice\"})-[r:KNOWS]->(b:Person {name: \"Bob\"})";
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    bool success = (ast != NULL && 
                   ast->type == GQL_AST_CREATE_QUERY &&
                   !gql_parser_has_error(parser));
    
    if (ast) {
        printf("\nAST for: %s\n", query);
        gql_ast_print(ast, 0);
        gql_ast_free_recursive(ast);
    } else if (gql_parser_has_error(parser)) {
        printf("\nParser error: %s\n", gql_parser_get_error(parser));
    }
    
    gql_parser_destroy(parser);
    return success;
}

bool test_complex_where_clause(void) {
    const char *query = "MATCH (n:Person) WHERE n.age > 25 AND n.name STARTS WITH \"A\" RETURN n";
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    bool success = (ast != NULL && 
                   ast->type == GQL_AST_MATCH_QUERY &&
                   !gql_parser_has_error(parser));
    
    if (ast) {
        printf("\nAST for: %s\n", query);
        gql_ast_print(ast, 0);
        gql_ast_free_recursive(ast);
    } else if (gql_parser_has_error(parser)) {
        printf("\nParser error: %s\n", gql_parser_get_error(parser));
    }
    
    gql_parser_destroy(parser);
    return success;
}

bool test_lexer_basic(void) {
    const char *input = "MATCH (n:Person) RETURN n.name";
    
    gql_lexer_t *lexer = gql_lexer_create(input);
    if (!lexer) return false;
    
    // Test a few tokens
    gql_token_t token = gql_lexer_next_token(lexer);
    bool success = (token.type == GQL_TOKEN_MATCH);
    gql_token_free(&token);
    
    if (success) {
        token = gql_lexer_next_token(lexer);
        success = (token.type == GQL_TOKEN_LPAREN);
        gql_token_free(&token);
    }
    
    if (success) {
        token = gql_lexer_next_token(lexer);
        success = (token.type == GQL_TOKEN_IDENTIFIER && 
                  strcmp(token.value, "n") == 0);
        gql_token_free(&token);
    }
    
    gql_lexer_destroy(lexer);
    return success;
}

bool test_error_handling(void) {
    const char *query = "MATCH (n:Person RETURN n.name"; // Missing closing paren
    
    gql_parser_t *parser = gql_parser_create(query);
    if (!parser) return false;
    
    gql_ast_node_t *ast = gql_parser_parse(parser);
    
    // Should fail with error
    bool success = (ast == NULL && gql_parser_has_error(parser));
    
    if (gql_parser_has_error(parser)) {
        printf("\nExpected error: %s\n", gql_parser_get_error(parser));
    }
    
    if (ast) {
        gql_ast_free_recursive(ast);
    }
    
    gql_parser_destroy(parser);
    return success;
}

int main(void) {
    printf("=== GraphQLite GQL Parser Tests ===\n\n");
    
    // Lexer tests
    test_case("Lexer Basic Tokenization", test_lexer_basic);
    
    // Parser tests
    test_case("Simple MATCH Query", test_simple_match_query);
    test_case("MATCH with Edge Pattern", test_match_with_edge);
    test_case("MATCH with WHERE Clause", test_match_with_where);
    test_case("CREATE Node Query", test_create_query);
    test_case("CREATE with Edge", test_create_with_edge);
    test_case("Complex WHERE Clause", test_complex_where_clause);
    test_case("Error Handling", test_error_handling);
    
    printf("\n=== Tests Complete ===\n");
    return 0;
}