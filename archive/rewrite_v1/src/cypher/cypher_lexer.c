#include "cypher_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// ============================================================================
// Keyword Table
// ============================================================================

typedef struct {
    const char *text;
    cypher_token_type_t token_type;
} keyword_entry_t;

static const keyword_entry_t keywords[] = {
    {"MATCH", CYPHER_TOKEN_MATCH},
    {"OPTIONAL", CYPHER_TOKEN_OPTIONAL},
    {"UNWIND", CYPHER_TOKEN_UNWIND},
    {"AS", CYPHER_TOKEN_AS},
    {"WITH", CYPHER_TOKEN_WITH},
    {"RETURN", CYPHER_TOKEN_RETURN},
    {"WHERE", CYPHER_TOKEN_WHERE},
    {"CREATE", CYPHER_TOKEN_CREATE},
    {"MERGE", CYPHER_TOKEN_MERGE},
    {"SET", CYPHER_TOKEN_SET},
    {"REMOVE", CYPHER_TOKEN_REMOVE},
    {"DELETE", CYPHER_TOKEN_DELETE},
    {"CALL", CYPHER_TOKEN_CALL},
    {"YIELD", CYPHER_TOKEN_YIELD},
    {"ORDER", CYPHER_TOKEN_ORDER},
    {"BY", CYPHER_TOKEN_BY},
    {"SKIP", CYPHER_TOKEN_SKIP},
    {"LIMIT", CYPHER_TOKEN_LIMIT},
    {"ASC", CYPHER_TOKEN_ASC},
    {"DESC", CYPHER_TOKEN_DESC},
    {"ASCENDING", CYPHER_TOKEN_ASCENDING},
    {"DESCENDING", CYPHER_TOKEN_DESCENDING},
    {"AND", CYPHER_TOKEN_AND},
    {"OR", CYPHER_TOKEN_OR},
    {"NOT", CYPHER_TOKEN_NOT},
    {"XOR", CYPHER_TOKEN_XOR},
    {"TRUE", CYPHER_TOKEN_TRUE},
    {"FALSE", CYPHER_TOKEN_FALSE},
    {"NULL", CYPHER_TOKEN_NULL_LITERAL},
    {"UNION", CYPHER_TOKEN_UNION},
    {"ALL", CYPHER_TOKEN_ALL},
    {"DISTINCT", CYPHER_TOKEN_DISTINCT},
    {"STARTS", CYPHER_TOKEN_STARTS},
    {"ENDS", CYPHER_TOKEN_ENDS},
    {"CONTAINS", CYPHER_TOKEN_CONTAINS},
    {"IN", CYPHER_TOKEN_IN},
    {"IS", CYPHER_TOKEN_IS},
    {"DETACH", CYPHER_TOKEN_DETACH},
    {"ON", CYPHER_TOKEN_ON},
    {"CASE", CYPHER_TOKEN_CASE},
    {"WHEN", CYPHER_TOKEN_WHEN},
    {"THEN", CYPHER_TOKEN_THEN},
    {"ELSE", CYPHER_TOKEN_ELSE},
    {"END", CYPHER_TOKEN_END},
    {"COUNT", CYPHER_TOKEN_COUNT},
    {"EXISTS", CYPHER_TOKEN_EXISTS},
    {"REDUCE", CYPHER_TOKEN_REDUCE},
    {"ANY", CYPHER_TOKEN_ANY},
    {"SINGLE", CYPHER_TOKEN_SINGLE},
    {"NONE", CYPHER_TOKEN_NONE},
    {"SHORTESTPATH", CYPHER_TOKEN_SHORTESTPATH},
    {"ALLSHORTESTPATHS", CYPHER_TOKEN_ALLSHORTESTPATHS},
    {"TRIM", CYPHER_TOKEN_TRIM},
    {"INF", CYPHER_TOKEN_INF},
    {"INFINITY", CYPHER_TOKEN_INFINITY},
    {"NAN", CYPHER_TOKEN_NAN},
    {NULL, CYPHER_TOKEN_EOF}
};

// ============================================================================
// Helper Functions
// ============================================================================

static void lexer_error(cypher_lexer_t *lexer, const char *message) {
    if (lexer->error_message) {
        free(lexer->error_message);
    }
    lexer->error_message = strdup(message);
    lexer->has_error = true;
}

static char current_char(cypher_lexer_t *lexer) {
    if (lexer->position >= lexer->input_length) {
        return '\0';
    }
    return lexer->input[lexer->position];
}

static char peek_char(cypher_lexer_t *lexer, size_t offset) {
    size_t pos = lexer->position + offset;
    if (pos >= lexer->input_length) {
        return '\0';
    }
    return lexer->input[pos];
}

static void advance_char(cypher_lexer_t *lexer) {
    if (lexer->position < lexer->input_length) {
        if (lexer->input[lexer->position] == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }
        lexer->position++;
    }
}

static void skip_whitespace(cypher_lexer_t *lexer) {
    while (isspace(current_char(lexer))) {
        advance_char(lexer);
    }
}

static void skip_comment(cypher_lexer_t *lexer) {
    char c = current_char(lexer);
    if (c == '/' && peek_char(lexer, 1) == '/') {
        // Single line comment
        while (current_char(lexer) && current_char(lexer) != '\n') {
            advance_char(lexer);
        }
    } else if (c == '/' && peek_char(lexer, 1) == '*') {
        // Multi-line comment
        advance_char(lexer); // Skip '/'
        advance_char(lexer); // Skip '*'
        while (current_char(lexer)) {
            if (current_char(lexer) == '*' && peek_char(lexer, 1) == '/') {
                advance_char(lexer); // Skip '*'
                advance_char(lexer); // Skip '/'
                break;
            }
            advance_char(lexer);
        }
    }
}

static cypher_token_t make_token(cypher_token_type_t type, const char *value, size_t length, size_t line, size_t column) {
    cypher_token_t token;
    token.type = type;
    token.length = length;
    token.line = line;
    token.column = column;
    
    if (value) {
        token.value = malloc(length + 1);
        if (length > 0) {
            memcpy(token.value, value, length);
        }
        token.value[length] = '\0';
    } else {
        token.value = NULL;
    }
    
    return token;
}

static cypher_token_t scan_string_literal(cypher_lexer_t *lexer) {
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    char quote_char = current_char(lexer);
    advance_char(lexer); // Skip opening quote
    
    size_t start = lexer->position;
    while (current_char(lexer) && current_char(lexer) != quote_char) {
        if (current_char(lexer) == '\\') {
            advance_char(lexer); // Skip escape character
            if (current_char(lexer)) {
                advance_char(lexer); // Skip escaped character
            }
        } else {
            advance_char(lexer);
        }
    }
    
    if (!current_char(lexer)) {
        lexer_error(lexer, "Unterminated string literal");
        return make_token(CYPHER_TOKEN_ERROR, NULL, 0, start_line, start_column);
    }
    
    size_t length = lexer->position - start;
    cypher_token_t token = make_token(CYPHER_TOKEN_STRING_LITERAL, 
                                     lexer->input + start, length, 
                                     start_line, start_column);
    
    advance_char(lexer); // Skip closing quote
    return token;
}

static cypher_token_t scan_number(cypher_lexer_t *lexer) {
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    size_t start = lexer->position;
    
    cypher_token_type_t token_type = CYPHER_TOKEN_INTEGER_LITERAL;
    
    // Handle hex literals
    if (current_char(lexer) == '0' && (peek_char(lexer, 1) == 'x' || peek_char(lexer, 1) == 'X')) {
        advance_char(lexer); // Skip '0'
        advance_char(lexer); // Skip 'x'
        while (isxdigit(current_char(lexer))) {
            advance_char(lexer);
        }
        token_type = CYPHER_TOKEN_HEX_LITERAL;
    }
    // Handle octal literals
    else if (current_char(lexer) == '0' && isdigit(peek_char(lexer, 1))) {
        while (isdigit(current_char(lexer)) && current_char(lexer) < '8') {
            advance_char(lexer);
        }
        token_type = CYPHER_TOKEN_OCTAL_LITERAL;
    }
    // Handle decimal numbers
    else {
        while (isdigit(current_char(lexer))) {
            advance_char(lexer);
        }
        
        // Check for decimal point
        if (current_char(lexer) == '.' && isdigit(peek_char(lexer, 1))) {
            token_type = CYPHER_TOKEN_FLOAT_LITERAL;
            advance_char(lexer); // Skip '.'
            while (isdigit(current_char(lexer))) {
                advance_char(lexer);
            }
        }
        
        // Check for scientific notation
        if (current_char(lexer) == 'e' || current_char(lexer) == 'E') {
            token_type = CYPHER_TOKEN_SCIENTIFIC_LITERAL;
            advance_char(lexer); // Skip 'e'
            if (current_char(lexer) == '+' || current_char(lexer) == '-') {
                advance_char(lexer); // Skip sign
            }
            while (isdigit(current_char(lexer))) {
                advance_char(lexer);
            }
        }
    }
    
    size_t length = lexer->position - start;
    return make_token(token_type, lexer->input + start, length, start_line, start_column);
}

static cypher_token_t scan_identifier_or_keyword(cypher_lexer_t *lexer) {
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    size_t start = lexer->position;
    
    // Handle backtick-quoted identifiers
    if (current_char(lexer) == '`') {
        advance_char(lexer); // Skip opening backtick
        size_t content_start = lexer->position;
        
        while (current_char(lexer) && current_char(lexer) != '`') {
            advance_char(lexer);
        }
        
        if (!current_char(lexer)) {
            lexer_error(lexer, "Unterminated backtick identifier");
            return make_token(CYPHER_TOKEN_ERROR, NULL, 0, start_line, start_column);
        }
        
        size_t content_length = lexer->position - content_start;
        cypher_token_t token = make_token(CYPHER_TOKEN_IDENTIFIER, 
                                         lexer->input + content_start, content_length, 
                                         start_line, start_column);
        
        advance_char(lexer); // Skip closing backtick
        return token;
    }
    
    // Regular identifier
    while (isalnum(current_char(lexer)) || current_char(lexer) == '_') {
        advance_char(lexer);
    }
    
    size_t length = lexer->position - start;
    
    // Check if it's a keyword
    char *text = malloc(length + 1);
    memcpy(text, lexer->input + start, length);
    text[length] = '\0';
    
    // Convert to uppercase for keyword lookup
    for (size_t i = 0; i < length; i++) {
        text[i] = toupper(text[i]);
    }
    
    cypher_token_type_t token_type = CYPHER_TOKEN_IDENTIFIER;
    for (const keyword_entry_t *entry = keywords; entry->text; entry++) {
        if (strcmp(text, entry->text) == 0) {
            token_type = entry->token_type;
            break;
        }
    }
    
    cypher_token_t token = make_token(token_type, lexer->input + start, length, start_line, start_column);
    free(text);
    return token;
}

// ============================================================================
// Public Interface
// ============================================================================

cypher_lexer_t* cypher_lexer_create(const char *input) {
    cypher_lexer_t *lexer = malloc(sizeof(cypher_lexer_t));
    lexer->input = input;
    lexer->input_length = strlen(input);
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->error_message = NULL;
    lexer->has_error = false;
    
    memset(&lexer->current_token, 0, sizeof(cypher_token_t));
    
    return lexer;
}

void cypher_lexer_destroy(cypher_lexer_t *lexer) {
    if (lexer) {
        if (lexer->error_message) {
            free(lexer->error_message);
        }
        cypher_token_free(&lexer->current_token);
        free(lexer);
    }
}

cypher_token_t cypher_lexer_next_token(cypher_lexer_t *lexer) {
    if (lexer->has_error) {
        return make_token(CYPHER_TOKEN_ERROR, NULL, 0, lexer->line, lexer->column);
    }
    
    // Skip whitespace and comments
    while (true) {
        skip_whitespace(lexer);
        
        char c = current_char(lexer);
        if (c == '/' && (peek_char(lexer, 1) == '/' || peek_char(lexer, 1) == '*')) {
            skip_comment(lexer);
        } else {
            break;
        }
    }
    
    size_t start_line = lexer->line;
    size_t start_column = lexer->column;
    char c = current_char(lexer);
    
    if (c == '\0') {
        return make_token(CYPHER_TOKEN_EOF, NULL, 0, start_line, start_column);
    }
    
    // String literals
    if (c == '"' || c == '\'') {
        return scan_string_literal(lexer);
    }
    
    // Numbers
    if (isdigit(c)) {
        return scan_number(lexer);
    }
    
    // Identifiers and keywords
    if (isalpha(c) || c == '_' || c == '`') {
        return scan_identifier_or_keyword(lexer);
    }
    
    // Two-character operators
    if (c == '<' && peek_char(lexer, 1) == '=') {
        advance_char(lexer);
        advance_char(lexer);
        return make_token(CYPHER_TOKEN_LE, "<=", 2, start_line, start_column);
    }
    if (c == '>' && peek_char(lexer, 1) == '=') {
        advance_char(lexer);
        advance_char(lexer);
        return make_token(CYPHER_TOKEN_GE, ">=", 2, start_line, start_column);
    }
    if (c == '<' && peek_char(lexer, 1) == '>') {
        advance_char(lexer);
        advance_char(lexer);
        return make_token(CYPHER_TOKEN_NE, "<>", 2, start_line, start_column);
    }
    if (c == '=' && peek_char(lexer, 1) == '~') {
        advance_char(lexer);
        advance_char(lexer);
        return make_token(CYPHER_TOKEN_REGEX_MATCH, "=~", 2, start_line, start_column);
    }
    if (c == '+' && peek_char(lexer, 1) == '=') {
        advance_char(lexer);
        advance_char(lexer);
        return make_token(CYPHER_TOKEN_PLUS_EQUALS, "+=", 2, start_line, start_column);
    }
    if (c == '.' && peek_char(lexer, 1) == '.') {
        advance_char(lexer);
        advance_char(lexer);
        return make_token(CYPHER_TOKEN_DOUBLE_DOT, "..", 2, start_line, start_column);
    }
    
    // Single-character tokens
    advance_char(lexer);
    switch (c) {
        case '(': return make_token(CYPHER_TOKEN_LPAREN, "(", 1, start_line, start_column);
        case ')': return make_token(CYPHER_TOKEN_RPAREN, ")", 1, start_line, start_column);
        case '{': return make_token(CYPHER_TOKEN_LBRACE, "{", 1, start_line, start_column);
        case '}': return make_token(CYPHER_TOKEN_RBRACE, "}", 1, start_line, start_column);
        case '[': return make_token(CYPHER_TOKEN_LBRACKET, "[", 1, start_line, start_column);
        case ']': return make_token(CYPHER_TOKEN_RBRACKET, "]", 1, start_line, start_column);
        case '.': return make_token(CYPHER_TOKEN_DOT, ".", 1, start_line, start_column);
        case ',': return make_token(CYPHER_TOKEN_COMMA, ",", 1, start_line, start_column);
        case ':': return make_token(CYPHER_TOKEN_COLON, ":", 1, start_line, start_column);
        case ';': return make_token(CYPHER_TOKEN_SEMICOLON, ";", 1, start_line, start_column);
        case '+': return make_token(CYPHER_TOKEN_PLUS, "+", 1, start_line, start_column);
        case '-': return make_token(CYPHER_TOKEN_MINUS, "-", 1, start_line, start_column);
        case '*': return make_token(CYPHER_TOKEN_ASTERISK, "*", 1, start_line, start_column);
        case '/': return make_token(CYPHER_TOKEN_SLASH, "/", 1, start_line, start_column);
        case '%': return make_token(CYPHER_TOKEN_PERCENT, "%", 1, start_line, start_column);
        case '^': return make_token(CYPHER_TOKEN_CARET, "^", 1, start_line, start_column);
        case '=': return make_token(CYPHER_TOKEN_EQUALS, "=", 1, start_line, start_column);
        case '<': return make_token(CYPHER_TOKEN_LT, "<", 1, start_line, start_column);
        case '>': return make_token(CYPHER_TOKEN_GT, ">", 1, start_line, start_column);
        case '$': return make_token(CYPHER_TOKEN_DOLLAR, "$", 1, start_line, start_column);
        case '&': return make_token(CYPHER_TOKEN_AMPERSAND, "&", 1, start_line, start_column);
        case '!': return make_token(CYPHER_TOKEN_EXCLAMATION, "!", 1, start_line, start_column);
        case '|': return make_token(CYPHER_TOKEN_VERTICAL_BAR, "|", 1, start_line, start_column);
        default:
            lexer_error(lexer, "Unexpected character");
            return make_token(CYPHER_TOKEN_ERROR, NULL, 0, start_line, start_column);
    }
}

cypher_token_t cypher_lexer_peek_token(cypher_lexer_t *lexer) {
    // Save current state
    size_t saved_position = lexer->position;
    size_t saved_line = lexer->line;
    size_t saved_column = lexer->column;
    
    // Get next token
    cypher_token_t token = cypher_lexer_next_token(lexer);
    
    // Restore state
    lexer->position = saved_position;
    lexer->line = saved_line;
    lexer->column = saved_column;
    
    return token;
}

bool cypher_lexer_at_end(cypher_lexer_t *lexer) {
    return lexer->position >= lexer->input_length;
}

void cypher_token_free(cypher_token_t *token) {
    if (token && token->value) {
        free(token->value);
        token->value = NULL;
    }
}

char* cypher_token_to_string(cypher_token_t *token) {
    if (!token) return NULL;
    
    const char *type_name = cypher_token_type_name(token->type);
    if (token->value) {
        char *result = malloc(strlen(type_name) + strlen(token->value) + 10);
        sprintf(result, "%s(%s)", type_name, token->value);
        return result;
    } else {
        return strdup(type_name);
    }
}

const char* cypher_token_type_name(cypher_token_type_t type) {
    switch (type) {
        case CYPHER_TOKEN_MATCH: return "MATCH";
        case CYPHER_TOKEN_OPTIONAL: return "OPTIONAL";
        case CYPHER_TOKEN_UNWIND: return "UNWIND";
        case CYPHER_TOKEN_AS: return "AS";
        case CYPHER_TOKEN_WITH: return "WITH";
        case CYPHER_TOKEN_RETURN: return "RETURN";
        case CYPHER_TOKEN_WHERE: return "WHERE";
        case CYPHER_TOKEN_CREATE: return "CREATE";
        case CYPHER_TOKEN_MERGE: return "MERGE";
        case CYPHER_TOKEN_SET: return "SET";
        case CYPHER_TOKEN_REMOVE: return "REMOVE";
        case CYPHER_TOKEN_DELETE: return "DELETE";
        case CYPHER_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case CYPHER_TOKEN_STRING_LITERAL: return "STRING_LITERAL";
        case CYPHER_TOKEN_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case CYPHER_TOKEN_FLOAT_LITERAL: return "FLOAT_LITERAL";
        case CYPHER_TOKEN_LPAREN: return "LPAREN";
        case CYPHER_TOKEN_RPAREN: return "RPAREN";
        case CYPHER_TOKEN_EOF: return "EOF";
        case CYPHER_TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

bool cypher_lexer_has_error(cypher_lexer_t *lexer) {
    return lexer->has_error;
}

const char* cypher_lexer_get_error(cypher_lexer_t *lexer) {
    return lexer->error_message;
}

bool cypher_is_keyword(const char *text) {
    char *upper_text = malloc(strlen(text) + 1);
    for (size_t i = 0; text[i]; i++) {
        upper_text[i] = toupper(text[i]);
    }
    upper_text[strlen(text)] = '\0';
    
    for (const keyword_entry_t *entry = keywords; entry->text; entry++) {
        if (strcmp(upper_text, entry->text) == 0) {
            free(upper_text);
            return true;
        }
    }
    
    free(upper_text);
    return false;
}

cypher_token_type_t cypher_keyword_type(const char *text) {
    char *upper_text = malloc(strlen(text) + 1);
    for (size_t i = 0; text[i]; i++) {
        upper_text[i] = toupper(text[i]);
    }
    upper_text[strlen(text)] = '\0';
    
    for (const keyword_entry_t *entry = keywords; entry->text; entry++) {
        if (strcmp(upper_text, entry->text) == 0) {
            free(upper_text);
            return entry->token_type;
        }
    }
    
    free(upper_text);
    return CYPHER_TOKEN_IDENTIFIER;
}

bool cypher_is_valid_identifier(const char *text) {
    if (!text || !*text) return false;
    
    if (!isalpha(*text) && *text != '_') return false;
    
    for (const char *p = text + 1; *p; p++) {
        if (!isalnum(*p) && *p != '_') return false;
    }
    
    return true;
}