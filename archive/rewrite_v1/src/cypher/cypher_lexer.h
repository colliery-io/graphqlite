#ifndef CYPHER_LEXER_H
#define CYPHER_LEXER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Token types matching our LEMON parser
typedef enum {
    // Keywords
    CYPHER_TOKEN_MATCH = 1,
    CYPHER_TOKEN_OPTIONAL = 2,
    CYPHER_TOKEN_UNWIND = 3,
    CYPHER_TOKEN_AS = 4,
    CYPHER_TOKEN_WITH = 5,
    CYPHER_TOKEN_RETURN = 6,
    CYPHER_TOKEN_WHERE = 7,
    CYPHER_TOKEN_CREATE = 8,
    CYPHER_TOKEN_MERGE = 9,
    CYPHER_TOKEN_SET = 10,
    CYPHER_TOKEN_REMOVE = 11,
    CYPHER_TOKEN_DELETE = 12,
    CYPHER_TOKEN_CALL = 13,
    CYPHER_TOKEN_YIELD = 14,
    CYPHER_TOKEN_ORDER = 15,
    CYPHER_TOKEN_BY = 16,
    CYPHER_TOKEN_SKIP = 17,
    CYPHER_TOKEN_LIMIT = 18,
    CYPHER_TOKEN_ASC = 19,
    CYPHER_TOKEN_DESC = 20,
    CYPHER_TOKEN_ASCENDING = 21,
    CYPHER_TOKEN_DESCENDING = 22,
    CYPHER_TOKEN_AND = 23,
    CYPHER_TOKEN_OR = 24,
    CYPHER_TOKEN_NOT = 25,
    CYPHER_TOKEN_XOR = 26,
    CYPHER_TOKEN_TRUE = 27,
    CYPHER_TOKEN_FALSE = 28,
    CYPHER_TOKEN_NULL_LITERAL = 29,
    CYPHER_TOKEN_UNION = 30,
    CYPHER_TOKEN_ALL = 31,
    CYPHER_TOKEN_DISTINCT = 32,
    CYPHER_TOKEN_STARTS = 33,
    CYPHER_TOKEN_ENDS = 34,
    CYPHER_TOKEN_CONTAINS = 35,
    CYPHER_TOKEN_IN = 36,
    CYPHER_TOKEN_IS = 37,
    
    // Punctuation
    CYPHER_TOKEN_LPAREN = 38,
    CYPHER_TOKEN_RPAREN = 39,
    CYPHER_TOKEN_LBRACE = 40,
    CYPHER_TOKEN_RBRACE = 41,
    CYPHER_TOKEN_LBRACKET = 42,
    CYPHER_TOKEN_RBRACKET = 43,
    CYPHER_TOKEN_DOT = 44,
    CYPHER_TOKEN_COMMA = 45,
    CYPHER_TOKEN_COLON = 46,
    CYPHER_TOKEN_SEMICOLON = 47,
    CYPHER_TOKEN_PLUS = 48,
    CYPHER_TOKEN_MINUS = 49,
    CYPHER_TOKEN_ASTERISK = 50,
    CYPHER_TOKEN_SLASH = 51,
    CYPHER_TOKEN_PERCENT = 52,
    CYPHER_TOKEN_CARET = 53,
    CYPHER_TOKEN_EQUALS = 54,
    CYPHER_TOKEN_NE = 55,
    CYPHER_TOKEN_LT = 56,
    CYPHER_TOKEN_GT = 57,
    CYPHER_TOKEN_LE = 58,
    CYPHER_TOKEN_GE = 59,
    CYPHER_TOKEN_ARROW_LEFT = 60,
    CYPHER_TOKEN_ARROW_RIGHT = 61,
    CYPHER_TOKEN_ARROW_BOTH = 62,
    CYPHER_TOKEN_ARROW_NONE = 63,
    CYPHER_TOKEN_DETACH = 64,
    CYPHER_TOKEN_ON = 65,
    CYPHER_TOKEN_PLUS_EQUALS = 66,
    CYPHER_TOKEN_DOUBLE_DOT = 67,
    CYPHER_TOKEN_CASE = 68,
    CYPHER_TOKEN_WHEN = 69,
    CYPHER_TOKEN_THEN = 70,
    CYPHER_TOKEN_ELSE = 71,
    CYPHER_TOKEN_END = 72,
    CYPHER_TOKEN_REGEX_MATCH = 73,
    CYPHER_TOKEN_COUNT = 74,
    CYPHER_TOKEN_EXISTS = 75,
    CYPHER_TOKEN_VERTICAL_BAR = 76,
    CYPHER_TOKEN_REDUCE = 77,
    CYPHER_TOKEN_ANY = 78,
    CYPHER_TOKEN_SINGLE = 79,
    CYPHER_TOKEN_NONE = 80,
    CYPHER_TOKEN_SHORTESTPATH = 81,
    CYPHER_TOKEN_ALLSHORTESTPATHS = 82,
    CYPHER_TOKEN_TRIM = 83,
    CYPHER_TOKEN_DOLLAR = 84,
    CYPHER_TOKEN_AMPERSAND = 85,
    CYPHER_TOKEN_EXCLAMATION = 86,
    
    // Literals and identifiers
    CYPHER_TOKEN_IDENTIFIER = 87,
    CYPHER_TOKEN_STRING_LITERAL = 88,
    CYPHER_TOKEN_INTEGER_LITERAL = 89,
    CYPHER_TOKEN_FLOAT_LITERAL = 90,
    CYPHER_TOKEN_HEX_LITERAL = 91,
    CYPHER_TOKEN_OCTAL_LITERAL = 92,
    CYPHER_TOKEN_SCIENTIFIC_LITERAL = 93,
    CYPHER_TOKEN_INF = 94,
    CYPHER_TOKEN_INFINITY = 95,
    CYPHER_TOKEN_NAN = 96,
    
    // Special
    CYPHER_TOKEN_UNARY = 97,
    CYPHER_TOKEN_EOF = 0,
    CYPHER_TOKEN_ERROR = -1
} cypher_token_type_t;

typedef struct {
    cypher_token_type_t type;
    char *value;
    size_t length;
    size_t line;
    size_t column;
    
    // For numeric literals
    union {
        int64_t integer_val;
        double float_val;
    } numeric;
} cypher_token_t;

typedef struct {
    const char *input;
    size_t input_length;
    size_t position;
    size_t line;
    size_t column;
    
    // Current token
    cypher_token_t current_token;
    
    // Error handling
    char *error_message;
    bool has_error;
} cypher_lexer_t;

// ============================================================================
// Lexer Lifecycle
// ============================================================================

cypher_lexer_t* cypher_lexer_create(const char *input);
void cypher_lexer_destroy(cypher_lexer_t *lexer);

// ============================================================================
// Tokenization
// ============================================================================

cypher_token_t cypher_lexer_next_token(cypher_lexer_t *lexer);
cypher_token_t cypher_lexer_peek_token(cypher_lexer_t *lexer);
bool cypher_lexer_at_end(cypher_lexer_t *lexer);

// ============================================================================
// Token Management
// ============================================================================

void cypher_token_free(cypher_token_t *token);
char* cypher_token_to_string(cypher_token_t *token);
const char* cypher_token_type_name(cypher_token_type_t type);

// ============================================================================
// Error Handling
// ============================================================================

bool cypher_lexer_has_error(cypher_lexer_t *lexer);
const char* cypher_lexer_get_error(cypher_lexer_t *lexer);

// ============================================================================
// Utility Functions
// ============================================================================

bool cypher_is_keyword(const char *text);
cypher_token_type_t cypher_keyword_type(const char *text);
bool cypher_is_valid_identifier(const char *text);

#ifdef __cplusplus
}
#endif

#endif // CYPHER_LEXER_H