#ifndef GQL_LEXER_H
#define GQL_LEXER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Token Types
// ============================================================================

typedef enum {
    // Literals
    GQL_TOKEN_INTEGER,
    GQL_TOKEN_STRING,
    GQL_TOKEN_BOOLEAN,
    GQL_TOKEN_NULL,
    GQL_TOKEN_IDENTIFIER,
    
    // Keywords
    GQL_TOKEN_MATCH,
    GQL_TOKEN_WHERE,
    GQL_TOKEN_RETURN,
    GQL_TOKEN_CREATE,
    GQL_TOKEN_SET,
    GQL_TOKEN_DELETE,
    GQL_TOKEN_AS,
    GQL_TOKEN_AND,
    GQL_TOKEN_OR,
    GQL_TOKEN_NOT,
    GQL_TOKEN_IS,
    GQL_TOKEN_STARTS,
    GQL_TOKEN_ENDS,
    GQL_TOKEN_WITH,
    GQL_TOKEN_CONTAINS,
    GQL_TOKEN_DISTINCT,
    GQL_TOKEN_COUNT,
    GQL_TOKEN_TRUE,
    GQL_TOKEN_FALSE,
    
    // Punctuation
    GQL_TOKEN_LPAREN,          // (
    GQL_TOKEN_RPAREN,          // )
    GQL_TOKEN_LBRACKET,        // [
    GQL_TOKEN_RBRACKET,        // ]
    GQL_TOKEN_LBRACE,          // {
    GQL_TOKEN_RBRACE,          // }
    GQL_TOKEN_COLON,           // :
    GQL_TOKEN_COMMA,           // ,
    GQL_TOKEN_DOT,             // .
    GQL_TOKEN_SEMICOLON,       // ;
    
    // Operators
    GQL_TOKEN_EQUALS,          // =
    GQL_TOKEN_NOT_EQUALS,      // <> or !=
    GQL_TOKEN_LESS_THAN,       // <
    GQL_TOKEN_LESS_EQUAL,      // <=
    GQL_TOKEN_GREATER_THAN,    // >
    GQL_TOKEN_GREATER_EQUAL,   // >=
    GQL_TOKEN_AMPERSAND,       // &
    
    // Graph operators
    GQL_TOKEN_ARROW_RIGHT,     // ->
    GQL_TOKEN_ARROW_LEFT,      // <-
    GQL_TOKEN_DASH,            // -
    
    // Special
    GQL_TOKEN_EOF,
    GQL_TOKEN_ERROR,
    GQL_TOKEN_UNKNOWN
} gql_token_type_t;

// ============================================================================
// Token Structure
// ============================================================================

typedef struct {
    gql_token_type_t type;
    char *value;               // Token text (malloc'd, must be freed)
    size_t length;             // Length of value
    
    // Position information
    size_t line;
    size_t column;
    size_t offset;             // Byte offset in source
} gql_token_t;

// ============================================================================
// Lexer Structure
// ============================================================================

typedef struct {
    const char *input;         // Input string
    size_t input_length;       // Total input length
    size_t position;           // Current position
    size_t line;               // Current line number (1-based)
    size_t column;             // Current column number (1-based)
    
    // Current character
    char current_char;
    bool at_end;
    
    // Error handling
    char *error_message;       // Last error message
} gql_lexer_t;

// ============================================================================
// Function Declarations
// ============================================================================

// Lexer lifecycle
gql_lexer_t* gql_lexer_create(const char *input);
void gql_lexer_destroy(gql_lexer_t *lexer);

// Token operations
gql_token_t gql_lexer_next_token(gql_lexer_t *lexer);
gql_token_t gql_lexer_peek_token(gql_lexer_t *lexer);
void gql_token_free(gql_token_t *token);

// Utility functions
const char* gql_token_type_name(gql_token_type_t type);
bool gql_token_is_keyword(gql_token_type_t type);
bool gql_token_is_operator(gql_token_type_t type);
bool gql_token_is_literal(gql_token_type_t type);

// Error handling
const char* gql_lexer_get_error(gql_lexer_t *lexer);
bool gql_lexer_has_error(gql_lexer_t *lexer);

#ifdef __cplusplus
}
#endif

#endif // GQL_LEXER_H