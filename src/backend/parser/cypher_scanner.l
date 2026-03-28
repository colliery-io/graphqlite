%{
/*
 * Cypher Lexical Scanner for GraphQLite
 * Based on OpenCypher specification and AGE scanner
 * Reentrant: no global state - enables concurrent parsing from multiple threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include "parser/cypher_scanner.h"
#include "parser/cypher_keywords.h"
#include "parser/cypher_tokens.h"

/* Forward declarations - take state (yyextra) for reentrant operation */
static void prepare_token(CypherScannerState *state, CypherTokenType type, const char *text);
static void prepare_string_token(CypherScannerState *state, CypherTokenType type, const char *text, const char *value);
static void prepare_integer_token(CypherScannerState *state, const char *text);
static void prepare_decimal_token(CypherScannerState *state, const char *text);
static void scanner_error(CypherScannerState *state, const char *message);
static char* unescape_string(const char *escaped_str, int length);

/* Update location tracking - yyextra is CypherScannerState* in reentrant mode */
#define YY_USER_ACTION \
    yyextra->current_column += yyleng;

/* Token preparation macros - yyextra is the scanner state */
#define RETURN_TOKEN(type) do { \
    prepare_token(yyextra, type, yytext); \
    return (int)type; \
} while(0)

#define RETURN_STRING_TOKEN(type, value) do { \
    prepare_string_token(yyextra, type, yytext, value); \
    return (int)type; \
} while(0)

%}

%option 8bit
%option never-interactive
%option noyywrap
%option nounistd
%option warn
%option nodefault

/* Reentrant: no global state - enables concurrent parsing */
%option reentrant
%option extra-type="CypherScannerState *"

/* Remove unused functions to avoid warnings */
%option noinput
%option nounput

/* Exclusive start condition for block comments */
%x BLOCK_COMMENT

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
regex_match =~

/* Single-line comments */
comment     "//"[^\n\r]*


/* Multi-character operators that need special handling */
/* Note: < and > are handled as individual chars for relationship arrows */
/* Note: - is handled as individual char for bare relationship patterns (-->, <--, --) */
operator    [+*/%^=!&|~]+

%%

{whitespace}    {
                    /* Count newlines for line tracking */
                    for (int i = 0; i < (int)yyleng; i++) {
                        if (yytext[i] == '\n') {
                            yyextra->current_line++;
                            yyextra->current_column = 1;
                        }
                    }
                    /* Skip whitespace - return next token */
                    continue;
                }

{comment}       {
                    /* Skip line comments */
                    continue;
                }

"/*"            {
                    /* Start of block comment */
                    BEGIN(BLOCK_COMMENT);
                }

<BLOCK_COMMENT>"*/" {
                    /* End of block comment */
                    BEGIN(INITIAL);
                }

<BLOCK_COMMENT>\n {
                    /* Count newlines in block comments */
                    yyextra->current_line++;
                    yyextra->current_column = 1;
                }

<BLOCK_COMMENT>.  {
                    /* Consume any character in block comment */
                }

{not_eq}        { RETURN_TOKEN(CYPHER_TOKEN_NOT_EQ); }
{lt_eq}         { RETURN_TOKEN(CYPHER_TOKEN_LT_EQ); }
{gt_eq}         { RETURN_TOKEN(CYPHER_TOKEN_GT_EQ); }
{dot_dot}       { RETURN_TOKEN(CYPHER_TOKEN_DOT_DOT); }
{typecast}      { RETURN_TOKEN(CYPHER_TOKEN_TYPECAST); }
{plus_eq}       { RETURN_TOKEN(CYPHER_TOKEN_PLUS_EQ); }
{regex_match}   { RETURN_TOKEN(CYPHER_TOKEN_REGEX_MATCH); }

{hexint}        { prepare_integer_token(yyextra, yytext); return CYPHER_TOKEN_INTEGER; }
{integer}       { prepare_integer_token(yyextra, yytext); return CYPHER_TOKEN_INTEGER; }
{decimal}       { prepare_decimal_token(yyextra, yytext); return CYPHER_TOKEN_DECIMAL; }

{string}        {
                    /* Remove quotes and handle escapes */
                    int len = yyleng - 2; /* Remove quotes */
                    char *unescaped = unescape_string(yytext + 1, len);
                    if (!unescaped) {
                        scanner_error(yyextra, "Out of memory processing string literal");
                        return CYPHER_TOKEN_EOF;
                    }
                    
                    prepare_string_token(yyextra, CYPHER_TOKEN_STRING, yytext, unescaped);
                    free(unescaped);
                    return CYPHER_TOKEN_STRING;
                }

{sstring}       {
                    /* Same as double-quoted strings with escape processing */
                    int len = yyleng - 2;
                    char *unescaped = unescape_string(yytext + 1, len);
                    if (!unescaped) {
                        scanner_error(yyextra, "Out of memory processing string literal");
                        return CYPHER_TOKEN_EOF;
                    }
                    
                    prepare_string_token(yyextra, CYPHER_TOKEN_STRING, yytext, unescaped);
                    free(unescaped);
                    return CYPHER_TOKEN_STRING;
                }

\$[A-Za-z_][A-Za-z0-9_]* {
                    /* Named parameters: $name, $param1 */
                    prepare_string_token(yyextra, CYPHER_TOKEN_PARAMETER, yytext, yytext + 1);
                    return CYPHER_TOKEN_PARAMETER;
                }

\$[0-9]+        {
                    /* Numeric parameters: $0, $1, $2, ... */
                    prepare_string_token(yyextra, CYPHER_TOKEN_PARAMETER, yytext, yytext + 1);
                    return CYPHER_TOKEN_PARAMETER;
                }

\$\{[A-Za-z_][A-Za-z0-9_]*\} {
                    /* Braced parameters: ${name}, ${param1} */
                    /* Extract name without ${ and } */
                    int len = yyleng - 3; /* Remove ${ and } */
                    char *value = malloc(len + 1);
                    if (value) {
                        strncpy(value, yytext + 2, len);
                        value[len] = '\0';
                        prepare_string_token(yyextra, CYPHER_TOKEN_PARAMETER, yytext, value);
                        free(value);
                    }
                    return CYPHER_TOKEN_PARAMETER;
                }

`[^`]*`         {
                    /* Backtick-quoted identifiers */
                    int len = yyleng - 2;
                    char *value = malloc(len + 1);
                    if (!value) {
                        scanner_error(yyextra, "Out of memory");
                        return CYPHER_TOKEN_EOF;
                    }
                    
                    strncpy(value, yytext + 1, len);
                    value[len] = '\0';
                    
                    prepare_string_token(yyextra, CYPHER_TOKEN_BQIDENT, yytext, value);
                    free(value);
                    return CYPHER_TOKEN_BQIDENT;
                }

{identifier}    {
                    /* Check if this is a reserved keyword */
                    const CypherKeywordToken *kw = cypher_keyword_lookup_full(yytext);
                    if (kw && kw->category == RESERVED_KEYWORD) {
                        /* Only reserved keywords are treated as keywords */
                        prepare_string_token(yyextra, CYPHER_TOKEN_KEYWORD, yytext, yytext);
                        yyextra->current_token.token_id = kw->token;
                        return CYPHER_TOKEN_KEYWORD;
                    } else {
                        /* Unreserved keywords and non-keywords are identifiers */
                        prepare_string_token(yyextra, CYPHER_TOKEN_IDENTIFIER, yytext, yytext);
                        return CYPHER_TOKEN_IDENTIFIER;
                    }
                }

{operator}      { prepare_string_token(yyextra, CYPHER_TOKEN_OPERATOR, yytext, yytext); return CYPHER_TOKEN_OPERATOR; }

:               { prepare_token(yyextra, CYPHER_TOKEN_CHAR, yytext); return CYPHER_TOKEN_CHAR; }
[(){}[\],;.<>\-]  { prepare_token(yyextra, CYPHER_TOKEN_CHAR, yytext); return CYPHER_TOKEN_CHAR; }

<<EOF>>         { prepare_token(yyextra, CYPHER_TOKEN_EOF, ""); return CYPHER_TOKEN_EOF; }

.               {
                    char msg[100];
                    snprintf(msg, sizeof(msg), "Unexpected character: '%c' (0x%02x)", 
                            yytext[0], (unsigned char)yytext[0]);
                    scanner_error(yyextra, msg);
                    prepare_token(yyextra, CYPHER_TOKEN_EOF, "");
                    return CYPHER_TOKEN_EOF;
                }

%%

/* Helper functions - all take state for reentrant operation */

static void prepare_token(CypherScannerState *state, CypherTokenType type, const char *text)
{
    state->current_token.type = type;
    state->current_token.token_id = 0;
    state->current_token.line = state->current_line;
    state->current_token.column = state->current_column - (int)strlen(text);
    state->current_token.text = strdup(text ? text : "");
    
    /* Initialize value based on type */
    switch (type) {
        case CYPHER_TOKEN_CHAR:
            state->current_token.value.character = text[0];
            break;
        default:
            state->current_token.value.string = NULL;
            break;
    }
}

static void prepare_string_token(CypherScannerState *state, CypherTokenType type, const char *text, const char *value)
{
    prepare_token(state, type, text);
    state->current_token.value.string = strdup(value ? value : "");
}

static void prepare_integer_token(CypherScannerState *state, const char *text)
{
    prepare_token(state, CYPHER_TOKEN_INTEGER, text);
    
    /* Parse integer with proper base detection */
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        /* Hexadecimal */
        state->current_token.value.integer = strtoll(text, NULL, 16);
    } else if (text[0] == '0' && strlen(text) > 1) {
        /* Octal */
        state->current_token.value.integer = strtoll(text, NULL, 8);
    } else {
        /* Decimal */
        state->current_token.value.integer = strtoll(text, NULL, 10);
    }
}

static void prepare_decimal_token(CypherScannerState *state, const char *text)
{
    prepare_token(state, CYPHER_TOKEN_DECIMAL, text);
    state->current_token.value.decimal = strtod(text, NULL);
}

static void scanner_error(CypherScannerState *state, const char *message)
{
    if (state) {
        state->has_error = true;
        state->last_error.line = state->current_line;
        state->last_error.column = state->current_column;
        
        /* Free previous error message if any */
        if (state->last_error.message) {
            free(state->last_error.message);
        }
        
        state->last_error.message = strdup(message);
    }
}

/* Function to get the current prepared token */
CypherToken cypher_scanner_get_current_token(const CypherScannerState *state)
{
    return state ? state->current_token : (CypherToken){0};
}

/* Process escape sequences in string literals */
static char* unescape_string(const char *escaped_str, int length)
{
    char *result = malloc(length + 1); /* Worst case - no escapes */
    if (!result) {
        return NULL;
    }
    
    int i = 0, j = 0;
    while (i < length) {
        if (escaped_str[i] == '\\' && i + 1 < length) {
            char next = escaped_str[i + 1];
            switch (next) {
                case 't':
                    result[j++] = '\t';
                    i += 2;
                    break;
                case 'b':
                    result[j++] = '\b';
                    i += 2;
                    break;
                case 'n':
                    result[j++] = '\n';
                    i += 2;
                    break;
                case 'r':
                    result[j++] = '\r';
                    i += 2;
                    break;
                case 'f':
                    result[j++] = '\f';
                    i += 2;
                    break;
                case '\'':
                    result[j++] = '\'';
                    i += 2;
                    break;
                case '"':
                    result[j++] = '"';
                    i += 2;
                    break;
                case '\\':
                    result[j++] = '\\';
                    i += 2;
                    break;
                case 'u':
                    /* Unicode escape sequence \uxxxx */
                    if (i + 5 < length) {
                        char hex_chars[5];
                        bool valid_hex = true;
                        
                        /* Extract 4 hex digits */
                        for (int k = 0; k < 4; k++) {
                            char c = escaped_str[i + 2 + k];
                            if (!((c >= '0' && c <= '9') || 
                                  (c >= 'A' && c <= 'F') || 
                                  (c >= 'a' && c <= 'f'))) {
                                valid_hex = false;
                                break;
                            }
                            hex_chars[k] = c;
                        }
                        hex_chars[4] = '\0';
                        
                        if (valid_hex) {
                            /* Convert hex to unicode code point */
                            unsigned int codepoint = strtoul(hex_chars, NULL, 16);
                            
                            /* Convert to UTF-8 (simplified for basic ASCII range) */
                            if (codepoint <= 0x7F) {
                                /* ASCII range */
                                result[j++] = (char)codepoint;
                            } else if (codepoint <= 0x7FF) {
                                /* 2-byte UTF-8 */
                                result[j++] = (char)(0xC0 | (codepoint >> 6));
                                result[j++] = (char)(0x80 | (codepoint & 0x3F));
                            } else {
                                /* 3-byte UTF-8 for Basic Multilingual Plane */
                                result[j++] = (char)(0xE0 | (codepoint >> 12));
                                result[j++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                                result[j++] = (char)(0x80 | (codepoint & 0x3F));
                            }
                            i += 6; /* Skip \uxxxx */
                        } else {
                            /* Invalid unicode escape - treat as literal */
                            result[j++] = escaped_str[i++];
                        }
                    } else {
                        /* Incomplete unicode escape - treat as literal */
                        result[j++] = escaped_str[i++];
                    }
                    break;
                default:
                    /* Unknown escape sequence - treat as literal */
                    result[j++] = escaped_str[i++];
                    break;
            }
        } else {
            /* Regular character */
            result[j++] = escaped_str[i++];
        }
    }
    
    result[j] = '\0';
    return result;
}