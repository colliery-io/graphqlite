#include "gql_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// ============================================================================
// Keyword Table
// ============================================================================

typedef struct {
    const char *text;
    gql_token_type_t type;
} keyword_entry_t;

static const keyword_entry_t keywords[] = {
    {"MATCH", GQL_TOKEN_MATCH},
    {"WHERE", GQL_TOKEN_WHERE},
    {"RETURN", GQL_TOKEN_RETURN},
    {"CREATE", GQL_TOKEN_CREATE},
    {"SET", GQL_TOKEN_SET},
    {"DELETE", GQL_TOKEN_DELETE},
    {"AS", GQL_TOKEN_AS},
    {"AND", GQL_TOKEN_AND},
    {"OR", GQL_TOKEN_OR},
    {"NOT", GQL_TOKEN_NOT},
    {"IS", GQL_TOKEN_IS},
    {"STARTS", GQL_TOKEN_STARTS},
    {"ENDS", GQL_TOKEN_ENDS},
    {"WITH", GQL_TOKEN_WITH},
    {"CONTAINS", GQL_TOKEN_CONTAINS},
    {"DISTINCT", GQL_TOKEN_DISTINCT},
    {"COUNT", GQL_TOKEN_COUNT},
    {"TRUE", GQL_TOKEN_TRUE},
    {"FALSE", GQL_TOKEN_FALSE},
    {"NULL", GQL_TOKEN_NULL},
    {NULL, GQL_TOKEN_UNKNOWN}
};

// ============================================================================
// Helper Functions
// ============================================================================

static void advance_char(gql_lexer_t *lexer) {
    if (lexer->at_end) return;
    
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    lexer->position++;
    if (lexer->position >= lexer->input_length) {
        lexer->at_end = true;
        lexer->current_char = '\0';
    } else {
        lexer->current_char = lexer->input[lexer->position];
    }
}

static char peek_char(gql_lexer_t *lexer, size_t offset) {
    size_t pos = lexer->position + offset;
    if (pos >= lexer->input_length) {
        return '\0';
    }
    return lexer->input[pos];
}

static void skip_whitespace(gql_lexer_t *lexer) {
    while (!lexer->at_end && isspace(lexer->current_char)) {
        advance_char(lexer);
    }
}

static void skip_comment(gql_lexer_t *lexer) {
    if (lexer->current_char == '/' && peek_char(lexer, 1) == '/') {
        // Single line comment
        while (!lexer->at_end && lexer->current_char != '\n') {
            advance_char(lexer);
        }
    } else if (lexer->current_char == '/' && peek_char(lexer, 1) == '*') {
        // Multi-line comment
        advance_char(lexer); // skip '/'
        advance_char(lexer); // skip '*'
        
        while (!lexer->at_end) {
            if (lexer->current_char == '*' && peek_char(lexer, 1) == '/') {
                advance_char(lexer); // skip '*'
                advance_char(lexer); // skip '/'
                break;
            }
            advance_char(lexer);
        }
    }
}

static gql_token_type_t lookup_keyword(const char *text) {
    for (int i = 0; keywords[i].text != NULL; i++) {
        if (strcasecmp(text, keywords[i].text) == 0) {
            return keywords[i].type;
        }
    }
    return GQL_TOKEN_IDENTIFIER;
}

static char* extract_string(gql_lexer_t *lexer, size_t start_pos, size_t end_pos) {
    size_t length = end_pos - start_pos;
    char *result = malloc(length + 1);
    if (!result) return NULL;
    
    strncpy(result, lexer->input + start_pos, length);
    result[length] = '\0';
    return result;
}

static void set_error(gql_lexer_t *lexer, const char *message) {
    if (lexer->error_message) {
        free(lexer->error_message);
    }
    lexer->error_message = strdup(message);
}

// ============================================================================
// Token Parsing Functions
// ============================================================================

static gql_token_t read_string_literal(gql_lexer_t *lexer) {
    gql_token_t token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.offset = lexer->position;
    
    char quote_char = lexer->current_char;
    advance_char(lexer); // skip opening quote
    
    size_t start_pos = lexer->position;
    size_t capacity = 64;
    char *value = malloc(capacity);
    size_t length = 0;
    
    if (!value) {
        token.type = GQL_TOKEN_ERROR;
        set_error(lexer, "Out of memory");
        return token;
    }
    
    while (!lexer->at_end && lexer->current_char != quote_char) {
        if (lexer->current_char == '\\') {
            // Handle escape sequences
            advance_char(lexer);
            if (lexer->at_end) break;
            
            char escaped;
            switch (lexer->current_char) {
                case 'n': escaped = '\n'; break;
                case 't': escaped = '\t'; break;
                case 'r': escaped = '\r'; break;
                case '\\': escaped = '\\'; break;
                case '\'': escaped = '\''; break;
                case '"': escaped = '"'; break;
                default:
                    escaped = lexer->current_char;
                    break;
            }
            
            if (length >= capacity - 1) {
                capacity *= 2;
                value = realloc(value, capacity);
                if (!value) {
                    token.type = GQL_TOKEN_ERROR;
                    set_error(lexer, "Out of memory");
                    return token;
                }
            }
            value[length++] = escaped;
        } else {
            if (length >= capacity - 1) {
                capacity *= 2;
                value = realloc(value, capacity);
                if (!value) {
                    token.type = GQL_TOKEN_ERROR;
                    set_error(lexer, "Out of memory");
                    return token;
                }
            }
            value[length++] = lexer->current_char;
        }
        advance_char(lexer);
    }
    
    if (lexer->at_end) {
        free(value);
        token.type = GQL_TOKEN_ERROR;
        set_error(lexer, "Unterminated string literal");
        return token;
    }
    
    advance_char(lexer); // skip closing quote
    
    value[length] = '\0';
    token.type = GQL_TOKEN_STRING;
    token.value = value;
    token.length = length;
    
    return token;
}

static gql_token_t read_number(gql_lexer_t *lexer) {
    gql_token_t token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.offset = lexer->position;
    
    size_t start_pos = lexer->position;
    
    // Read digits
    while (!lexer->at_end && isdigit(lexer->current_char)) {
        advance_char(lexer);
    }
    
    token.value = extract_string(lexer, start_pos, lexer->position);
    token.length = lexer->position - start_pos;
    token.type = GQL_TOKEN_INTEGER;
    
    return token;
}

static gql_token_t read_identifier(gql_lexer_t *lexer) {
    gql_token_t token = {0};
    token.line = lexer->line;
    token.column = lexer->column;
    token.offset = lexer->position;
    
    size_t start_pos = lexer->position;
    
    // Read identifier characters
    while (!lexer->at_end && (isalnum(lexer->current_char) || lexer->current_char == '_')) {
        advance_char(lexer);
    }
    
    token.value = extract_string(lexer, start_pos, lexer->position);
    token.length = lexer->position - start_pos;
    
    // Check if it's a keyword
    token.type = lookup_keyword(token.value);
    
    return token;
}

// ============================================================================
// Public API Implementation
// ============================================================================

gql_lexer_t* gql_lexer_create(const char *input) {
    if (!input) return NULL;
    
    gql_lexer_t *lexer = malloc(sizeof(gql_lexer_t));
    if (!lexer) return NULL;
    
    lexer->input = input;
    lexer->input_length = strlen(input);
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->error_message = NULL;
    
    if (lexer->input_length > 0) {
        lexer->current_char = lexer->input[0];
        lexer->at_end = false;
    } else {
        lexer->current_char = '\0';
        lexer->at_end = true;
    }
    
    return lexer;
}

void gql_lexer_destroy(gql_lexer_t *lexer) {
    if (!lexer) return;
    
    if (lexer->error_message) {
        free(lexer->error_message);
    }
    free(lexer);
}

gql_token_t gql_lexer_next_token(gql_lexer_t *lexer) {
    gql_token_t token = {0};
    
    if (!lexer) {
        token.type = GQL_TOKEN_ERROR;
        return token;
    }
    
    // Skip whitespace and comments
    while (!lexer->at_end) {
        skip_whitespace(lexer);
        if (lexer->current_char == '/' && 
            (peek_char(lexer, 1) == '/' || peek_char(lexer, 1) == '*')) {
            skip_comment(lexer);
        } else {
            break;
        }
    }
    
    if (lexer->at_end) {
        token.type = GQL_TOKEN_EOF;
        token.line = lexer->line;
        token.column = lexer->column;
        token.offset = lexer->position;
        return token;
    }
    
    token.line = lexer->line;
    token.column = lexer->column;
    token.offset = lexer->position;
    
    char c = lexer->current_char;
    
    // String literals
    if (c == '\'' || c == '"') {
        return read_string_literal(lexer);
    }
    
    // Numbers
    if (isdigit(c)) {
        return read_number(lexer);
    }
    
    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        return read_identifier(lexer);
    }
    
    // Two-character operators
    char next_c = peek_char(lexer, 1);
    if (c == '<' && next_c == '>') {
        token.type = GQL_TOKEN_NOT_EQUALS;
        token.value = strdup("<>");
        token.length = 2;
        advance_char(lexer);
        advance_char(lexer);
        return token;
    }
    if (c == '!' && next_c == '=') {
        token.type = GQL_TOKEN_NOT_EQUALS;
        token.value = strdup("!=");
        token.length = 2;
        advance_char(lexer);
        advance_char(lexer);
        return token;
    }
    if (c == '<' && next_c == '=') {
        token.type = GQL_TOKEN_LESS_EQUAL;
        token.value = strdup("<=");
        token.length = 2;
        advance_char(lexer);
        advance_char(lexer);
        return token;
    }
    if (c == '>' && next_c == '=') {
        token.type = GQL_TOKEN_GREATER_EQUAL;
        token.value = strdup(">=");
        token.length = 2;
        advance_char(lexer);
        advance_char(lexer);
        return token;
    }
    if (c == '-' && next_c == '>') {
        token.type = GQL_TOKEN_ARROW_RIGHT;
        token.value = strdup("->");
        token.length = 2;
        advance_char(lexer);
        advance_char(lexer);
        return token;
    }
    if (c == '<' && next_c == '-') {
        token.type = GQL_TOKEN_ARROW_LEFT;
        token.value = strdup("<-");
        token.length = 2;
        advance_char(lexer);
        advance_char(lexer);
        return token;
    }
    
    // Single-character tokens
    token.length = 1;
    char *single_char = malloc(2);
    single_char[0] = c;
    single_char[1] = '\0';
    token.value = single_char;
    
    switch (c) {
        case '(': token.type = GQL_TOKEN_LPAREN; break;
        case ')': token.type = GQL_TOKEN_RPAREN; break;
        case '[': token.type = GQL_TOKEN_LBRACKET; break;
        case ']': token.type = GQL_TOKEN_RBRACKET; break;
        case '{': token.type = GQL_TOKEN_LBRACE; break;
        case '}': token.type = GQL_TOKEN_RBRACE; break;
        case ':': token.type = GQL_TOKEN_COLON; break;
        case ',': token.type = GQL_TOKEN_COMMA; break;
        case '.': token.type = GQL_TOKEN_DOT; break;
        case ';': token.type = GQL_TOKEN_SEMICOLON; break;
        case '=': token.type = GQL_TOKEN_EQUALS; break;
        case '<': token.type = GQL_TOKEN_LESS_THAN; break;
        case '>': token.type = GQL_TOKEN_GREATER_THAN; break;
        case '-': token.type = GQL_TOKEN_DASH; break;
        case '&': token.type = GQL_TOKEN_AMPERSAND; break;
        default:
            token.type = GQL_TOKEN_UNKNOWN;
            break;
    }
    
    advance_char(lexer);
    return token;
}

void gql_token_free(gql_token_t *token) {
    if (token && token->value) {
        free(token->value);
        token->value = NULL;
    }
}

const char* gql_token_type_name(gql_token_type_t type) {
    switch (type) {
        case GQL_TOKEN_INTEGER: return "INTEGER";
        case GQL_TOKEN_STRING: return "STRING";
        case GQL_TOKEN_BOOLEAN: return "BOOLEAN";
        case GQL_TOKEN_NULL: return "NULL";
        case GQL_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case GQL_TOKEN_MATCH: return "MATCH";
        case GQL_TOKEN_WHERE: return "WHERE";
        case GQL_TOKEN_RETURN: return "RETURN";
        case GQL_TOKEN_CREATE: return "CREATE";
        case GQL_TOKEN_SET: return "SET";
        case GQL_TOKEN_DELETE: return "DELETE";
        case GQL_TOKEN_AS: return "AS";
        case GQL_TOKEN_AND: return "AND";
        case GQL_TOKEN_OR: return "OR";
        case GQL_TOKEN_NOT: return "NOT";
        case GQL_TOKEN_LPAREN: return "LPAREN";
        case GQL_TOKEN_RPAREN: return "RPAREN";
        case GQL_TOKEN_LBRACKET: return "LBRACKET";
        case GQL_TOKEN_RBRACKET: return "RBRACKET";
        case GQL_TOKEN_LBRACE: return "LBRACE";
        case GQL_TOKEN_RBRACE: return "RBRACE";
        case GQL_TOKEN_COLON: return "COLON";
        case GQL_TOKEN_COMMA: return "COMMA";
        case GQL_TOKEN_DOT: return "DOT";
        case GQL_TOKEN_EQUALS: return "EQUALS";
        case GQL_TOKEN_NOT_EQUALS: return "NOT_EQUALS";
        case GQL_TOKEN_LESS_THAN: return "LESS_THAN";
        case GQL_TOKEN_GREATER_THAN: return "GREATER_THAN";
        case GQL_TOKEN_AMPERSAND: return "AMPERSAND";
        case GQL_TOKEN_ARROW_RIGHT: return "ARROW_RIGHT";
        case GQL_TOKEN_ARROW_LEFT: return "ARROW_LEFT";
        case GQL_TOKEN_DASH: return "DASH";
        case GQL_TOKEN_EOF: return "EOF";
        case GQL_TOKEN_ERROR: return "ERROR";
        case GQL_TOKEN_UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

bool gql_token_is_keyword(gql_token_type_t type) {
    return type >= GQL_TOKEN_MATCH && type <= GQL_TOKEN_FALSE;
}

bool gql_token_is_operator(gql_token_type_t type) {
    return type >= GQL_TOKEN_EQUALS && type <= GQL_TOKEN_DASH;
}

bool gql_token_is_literal(gql_token_type_t type) {
    return type >= GQL_TOKEN_INTEGER && type <= GQL_TOKEN_NULL;
}

const char* gql_lexer_get_error(gql_lexer_t *lexer) {
    return lexer ? lexer->error_message : NULL;
}

bool gql_lexer_has_error(gql_lexer_t *lexer) {
    return lexer && lexer->error_message != NULL;
}