%{
/*
 * Cypher Lexical Scanner for GraphQLite
 * Based on OpenCypher specification and AGE scanner
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "parser/cypher_scanner.h"
#include "parser/cypher_keywords.h"
#include "parser/cypher_tokens.h"

/* Forward declaration */
static void prepare_token(CypherTokenType type, const char *text);
static void prepare_string_token(CypherTokenType type, const char *text, const char *value);
static void prepare_integer_token(const char *text);
static void prepare_decimal_token(const char *text);
static void scanner_error(const char *message);

/* Global variables for current scanner state */
CypherScannerState *current_scanner = NULL;
static int current_line = 1;
static int current_column = 1;

/* Current token being built */
static CypherToken current_token;

/* Update location tracking */
#define YY_USER_ACTION \
    current_column += yyleng;

/* Token preparation instead of return */
#define RETURN_TOKEN(type) do { \
    prepare_token(type, yytext); \
    return (int)type; \
} while(0)

#define RETURN_STRING_TOKEN(type, value) do { \
    prepare_string_token(type, yytext, value); \
    return (int)type; \
} while(0)

%}

%option 8bit
%option never-interactive
%option noyywrap
%option nounistd
%option warn
%option nodefault

/* Remove unused functions to avoid warnings */
%option noinput
%option nounput

/* Define character classes */
digit       [0-9]
hexdigit    [0-9A-Fa-f]
letter      [A-Za-z_]
alnum       [A-Za-z0-9_]

/* Whitespace - basic set for now */
whitespace  [ \t\r\n]+

/* Numbers */
integer     {digit}+
hexint      0[xX]{hexdigit}+
decimal     {digit}+\.{digit}+([eE][+-]?{digit}+)?

/* Identifiers */
identifier  {letter}{alnum}*

/* String literals */
string      \"([^\"\\]|\\.)*\"
sstring     '([^'\\]|\\.)*'

/* Operators */
not_eq      (!=|<>)
lt_eq       <=
gt_eq       >=
dot_dot     \.\.
typecast    ::
plus_eq     \+=

/* Single-line comments */
comment     "//"[^\n\r]*

/* Multi-character operators that need special handling */
operator    [+\-*/%^<>=!&|~]+

%%

{whitespace}    {
                    /* Count newlines for line tracking */
                    for (int i = 0; i < (int)yyleng; i++) {
                        if (yytext[i] == '\n') {
                            current_line++;
                            current_column = 1;
                        }
                    }
                    /* Skip whitespace - return next token */
                    continue;
                }

{comment}       {
                    /* Skip comments */
                    continue;
                }

{not_eq}        { RETURN_TOKEN(CYPHER_TOKEN_NOT_EQ); }
{lt_eq}         { RETURN_TOKEN(CYPHER_TOKEN_LT_EQ); }
{gt_eq}         { RETURN_TOKEN(CYPHER_TOKEN_GT_EQ); }
{dot_dot}       { RETURN_TOKEN(CYPHER_TOKEN_DOT_DOT); }
{typecast}      { RETURN_TOKEN(CYPHER_TOKEN_TYPECAST); }
{plus_eq}       { RETURN_TOKEN(CYPHER_TOKEN_PLUS_EQ); }

{hexint}        { prepare_integer_token(yytext); return CYPHER_TOKEN_INTEGER; }
{integer}       { prepare_integer_token(yytext); return CYPHER_TOKEN_INTEGER; }
{decimal}       { prepare_decimal_token(yytext); return CYPHER_TOKEN_DECIMAL; }

{string}        {
                    /* Remove quotes and handle escapes */
                    int len = yyleng - 2; /* Remove quotes */
                    char *value = malloc(len + 1);
                    if (!value) {
                        scanner_error("Out of memory");
                        return CYPHER_TOKEN_EOF;
                    }
                    
                    /* Simple unescaping - copy content between quotes */
                    strncpy(value, yytext + 1, len);
                    value[len] = '\0';
                    
                    prepare_string_token(CYPHER_TOKEN_STRING, yytext, value);
                    free(value);
                    return CYPHER_TOKEN_STRING;
                }

{sstring}       {
                    /* Same as double-quoted strings */
                    int len = yyleng - 2;
                    char *value = malloc(len + 1);
                    if (!value) {
                        scanner_error("Out of memory");
                        return CYPHER_TOKEN_EOF;
                    }
                    
                    strncpy(value, yytext + 1, len);
                    value[len] = '\0';
                    
                    prepare_string_token(CYPHER_TOKEN_STRING, yytext, value);
                    free(value);
                    return CYPHER_TOKEN_STRING;
                }

\$[A-Za-z_][A-Za-z0-9_]* {
                    /* Parameters start with $ */
                    prepare_string_token(CYPHER_TOKEN_PARAMETER, yytext, yytext + 1);
                    return CYPHER_TOKEN_PARAMETER;
                }

`[^`]*`         {
                    /* Backtick-quoted identifiers */
                    int len = yyleng - 2;
                    char *value = malloc(len + 1);
                    if (!value) {
                        scanner_error("Out of memory");
                        return CYPHER_TOKEN_EOF;
                    }
                    
                    strncpy(value, yytext + 1, len);
                    value[len] = '\0';
                    
                    prepare_string_token(CYPHER_TOKEN_BQIDENT, yytext, value);
                    free(value);
                    return CYPHER_TOKEN_BQIDENT;
                }

{identifier}    {
                    /* Check if this is a keyword */
                    int keyword_token = cypher_keyword_lookup(yytext);
                    if (keyword_token >= 0) {
                        prepare_string_token(CYPHER_TOKEN_KEYWORD, yytext, yytext);
                        current_token.token_id = keyword_token;
                        return CYPHER_TOKEN_KEYWORD;
                    } else {
                        prepare_string_token(CYPHER_TOKEN_IDENTIFIER, yytext, yytext);
                        return CYPHER_TOKEN_IDENTIFIER;
                    }
                }

{operator}      { prepare_string_token(CYPHER_TOKEN_OPERATOR, yytext, yytext); return CYPHER_TOKEN_OPERATOR; }

:               { prepare_token(CYPHER_TOKEN_CHAR, yytext); return CYPHER_TOKEN_CHAR; }
[(){}[\],;.]    { prepare_token(CYPHER_TOKEN_CHAR, yytext); return CYPHER_TOKEN_CHAR; }

<<EOF>>         { prepare_token(CYPHER_TOKEN_EOF, ""); return CYPHER_TOKEN_EOF; }

.               {
                    char msg[100];
                    snprintf(msg, sizeof(msg), "Unexpected character: '%c' (0x%02x)", 
                            yytext[0], (unsigned char)yytext[0]);
                    scanner_error(msg);
                    prepare_token(CYPHER_TOKEN_EOF, "");
                    return CYPHER_TOKEN_EOF;
                }

%%

/* Helper functions */

static void prepare_token(CypherTokenType type, const char *text)
{
    current_token.type = type;
    current_token.token_id = 0;
    current_token.line = current_line;
    current_token.column = current_column - strlen(text);
    current_token.text = strdup(text ? text : "");
    
    /* Initialize value based on type */
    switch (type) {
        case CYPHER_TOKEN_CHAR:
            current_token.value.character = text[0];
            break;
        default:
            current_token.value.string = NULL;
            break;
    }
}

static void prepare_string_token(CypherTokenType type, const char *text, const char *value)
{
    prepare_token(type, text);
    current_token.value.string = strdup(value ? value : "");
}

static void prepare_integer_token(const char *text)
{
    prepare_token(CYPHER_TOKEN_INTEGER, text);
    
    /* Parse integer with proper base detection */
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        /* Hexadecimal */
        current_token.value.integer = strtol(text, NULL, 16);
    } else if (text[0] == '0' && strlen(text) > 1) {
        /* Octal */
        current_token.value.integer = strtol(text, NULL, 8);
    } else {
        /* Decimal */
        current_token.value.integer = strtol(text, NULL, 10);
    }
}

static void prepare_decimal_token(const char *text)
{
    prepare_token(CYPHER_TOKEN_DECIMAL, text);
    current_token.value.decimal = strtod(text, NULL);
}

static void scanner_error(const char *message)
{
    if (current_scanner) {
        current_scanner->has_error = true;
        current_scanner->last_error.line = current_line;
        current_scanner->last_error.column = current_column;
        
        /* Free previous error message if any */
        if (current_scanner->last_error.message) {
            free(current_scanner->last_error.message);
        }
        
        current_scanner->last_error.message = strdup(message);
    }
}

/* Function to get the current prepared token */
CypherToken cypher_scanner_get_current_token(void)
{
    return current_token;
}