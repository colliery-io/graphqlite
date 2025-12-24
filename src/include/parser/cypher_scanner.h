#ifndef CYPHER_SCANNER_H
#define CYPHER_SCANNER_H

#include <stdio.h>
#include <stdbool.h>
#include "parser/cypher_tokens.h"

/* Forward declaration for scanner handle */
typedef void* cypher_scanner_t;

/* Token types that the scanner can produce */
typedef enum CypherTokenType {
    CYPHER_TOKEN_EOF = 0,          /* End of input */
    CYPHER_TOKEN_INTEGER,          /* 123, 0x1F, 077 */
    CYPHER_TOKEN_DECIMAL,          /* 123.45, 1.23E-4 */
    CYPHER_TOKEN_STRING,           /* "hello" or 'world' */
    CYPHER_TOKEN_IDENTIFIER,       /* variable_name */
    CYPHER_TOKEN_PARAMETER,        /* $param */
    CYPHER_TOKEN_BQIDENT,          /* `quoted identifier` */
    CYPHER_TOKEN_OPERATOR,         /* +, -, *, etc. */
    CYPHER_TOKEN_CHAR,             /* Single character tokens like (, ), etc. */
    
    /* Multi-character operators */
    CYPHER_TOKEN_NOT_EQ,           /* != or <> */
    CYPHER_TOKEN_LT_EQ,            /* <= */
    CYPHER_TOKEN_GT_EQ,            /* >= */
    CYPHER_TOKEN_DOT_DOT,          /* .. */
    CYPHER_TOKEN_TYPECAST,         /* :: */
    CYPHER_TOKEN_PLUS_EQ,          /* += */
    CYPHER_TOKEN_REGEX_MATCH,      /* =~ */

    /* Keywords - will be mapped from our keyword system */
    CYPHER_TOKEN_KEYWORD           /* Any keyword from our keyword table */
} CypherTokenType;

/* Token value union */
typedef union CypherTokenValue {
    int integer;                   /* For integer tokens */
    double decimal;                /* For decimal tokens */
    char *string;                  /* For strings, identifiers, operators */
    char character;                /* For single character tokens */
} CypherTokenValue;

/* Complete token structure */
typedef struct CypherToken {
    CypherTokenType type;          /* Token type */
    CypherTokenValue value;        /* Token value */
    int token_id;                  /* Bison token ID (for keywords) */
    int line;                      /* Line number */
    int column;                    /* Column number */
    const char *text;              /* Original text (for debugging) */
} CypherToken;

/* Scanner error information */
typedef struct CypherScannerError {
    int line;
    int column;
    char *message;
} CypherScannerError;

/* Scanner state */
typedef struct CypherScannerState {
    cypher_scanner_t scanner;      /* Flex scanner handle */
    const char *input_string;      /* Input string */
    CypherScannerError last_error; /* Last error encountered */
    bool has_error;                /* Error flag */
} CypherScannerState;

/* Function prototypes */

/* Scanner lifecycle */
CypherScannerState* cypher_scanner_create(void);
void cypher_scanner_destroy(CypherScannerState *state);

/* Input setup */
int cypher_scanner_set_input_string(CypherScannerState *state, const char *input);

/* Token retrieval */
CypherToken cypher_scanner_next_token(CypherScannerState *state);

/* Error handling */
bool cypher_scanner_has_error(const CypherScannerState *state);
const CypherScannerError* cypher_scanner_get_error(const CypherScannerState *state);
void cypher_scanner_clear_error(CypherScannerState *state);

/* Utility functions */
const char* cypher_token_type_name(CypherTokenType type);
void cypher_token_free(CypherToken *token);

/* Internal function to get current token (used by API) */
CypherToken cypher_scanner_get_current_token(void);

#endif /* CYPHER_SCANNER_H */