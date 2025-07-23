#ifndef GQL_PARSER_H
#define GQL_PARSER_H

#include "gql_lexer.h"
#include "gql_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Parser Structure
// ============================================================================

typedef struct {
    gql_lexer_t *lexer;
    gql_token_t current_token;
    gql_token_t peek_token;
    
    // Error handling
    char *error_message;
    size_t error_line;
    size_t error_column;
    bool has_error;
    
    // Parser state
    bool at_end;
} gql_parser_t;

// ============================================================================
// Parser Lifecycle
// ============================================================================

gql_parser_t* gql_parser_create(const char *input);
void gql_parser_destroy(gql_parser_t *parser);

// ============================================================================
// Main Parsing Function
// ============================================================================

gql_ast_node_t* gql_parser_parse(gql_parser_t *parser);

// ============================================================================
// Query Parsing Functions
// ============================================================================

gql_ast_node_t* parse_query(gql_parser_t *parser);
gql_ast_node_t* parse_match_query(gql_parser_t *parser);
gql_ast_node_t* parse_create_query(gql_parser_t *parser);
gql_ast_node_t* parse_set_query(gql_parser_t *parser);
gql_ast_node_t* parse_delete_query(gql_parser_t *parser);

// ============================================================================
// Pattern Parsing Functions
// ============================================================================

gql_ast_node_t* parse_pattern_list(gql_parser_t *parser);
gql_ast_node_t* parse_pattern(gql_parser_t *parser);
gql_ast_node_t* parse_node_pattern(gql_parser_t *parser);
gql_ast_node_t* parse_edge_pattern(gql_parser_t *parser);
gql_ast_node_t* parse_property_map(gql_parser_t *parser);

// ============================================================================
// Expression Parsing Functions
// ============================================================================

gql_ast_node_t* parse_expression(gql_parser_t *parser);
gql_ast_node_t* parse_or_expression(gql_parser_t *parser);
gql_ast_node_t* parse_and_expression(gql_parser_t *parser);
gql_ast_node_t* parse_not_expression(gql_parser_t *parser);
gql_ast_node_t* parse_comparison_expression(gql_parser_t *parser);
gql_ast_node_t* parse_primary_expression(gql_parser_t *parser);

// ============================================================================
// Clause Parsing Functions
// ============================================================================

gql_ast_node_t* parse_where_clause(gql_parser_t *parser);
gql_ast_node_t* parse_return_clause(gql_parser_t *parser);
gql_ast_node_t* parse_set_clause(gql_parser_t *parser);

// ============================================================================
// Utility Functions
// ============================================================================

bool expect_token(gql_parser_t *parser, gql_token_type_t expected_type);
bool match_token(gql_parser_t *parser, gql_token_type_t token_type);
void advance_token(gql_parser_t *parser);
gql_operator_t token_to_operator(gql_token_type_t token_type);

// ============================================================================
// Error Handling
// ============================================================================

void parser_error(gql_parser_t *parser, const char *message);
const char* gql_parser_get_error(gql_parser_t *parser);
bool gql_parser_has_error(gql_parser_t *parser);
void parser_error_at_token(gql_parser_t *parser, const char *message);

#ifdef __cplusplus
}
#endif

#endif // GQL_PARSER_H