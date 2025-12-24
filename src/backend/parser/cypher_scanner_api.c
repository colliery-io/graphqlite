/*
 * Cypher Scanner API Implementation
 * High-level interface for the Flex-generated scanner
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/cypher_scanner.h"

/* External Flex functions (will be generated) */
extern int yylex(void);
extern void *yy_scan_string(const char *str);
extern void yy_delete_buffer(void *buffer);

/* External variables from scanner */
extern CypherScannerState *current_scanner;

/* Scanner lifecycle functions */

CypherScannerState* cypher_scanner_create(void)
{
    CypherScannerState *state = calloc(1, sizeof(CypherScannerState));
    if (!state) {
        return NULL;
    }
    
    /* Initialize error state */
    state->has_error = false;
    state->last_error.line = 0;
    state->last_error.column = 0;
    state->last_error.message = NULL;
    state->scanner = NULL;
    state->input_string = NULL;
    
    /* Set global current_scanner for Flex callbacks */
    current_scanner = state;
    
    return state;
}

void cypher_scanner_destroy(CypherScannerState *state)
{
    if (!state) {
        return;
    }
    
    /* Clean up buffer if any */
    if (state->scanner) {
        yy_delete_buffer(state->scanner);
    }
    
    /* Free error message if any */
    if (state->last_error.message) {
        free(state->last_error.message);
    }
    
    /* Clear global reference */
    if (current_scanner == state) {
        current_scanner = NULL;
    }
    
    free(state);
}

/* Input setup functions */

int cypher_scanner_set_input_string(CypherScannerState *state, const char *input)
{
    if (!state || !input) {
        return -1;
    }
    
    state->input_string = input;
    
    /* Create Flex buffer for string input */
    state->scanner = yy_scan_string(input);
    
    return 0;
}

/* Token retrieval */
CypherToken cypher_scanner_next_token(CypherScannerState *state)
{
    CypherToken token = {0};
    
    if (!state) {
        token.type = CYPHER_TOKEN_EOF;
        token.text = strdup("");
        return token;
    }
    
    /* Set current context for Flex callbacks */
    current_scanner = state;
    
    /* Call Flex to get the next token */
    yylex();
    
    /* Get the prepared token from the scanner */
    token = cypher_scanner_get_current_token();
    
    return token;
}

/* Error handling functions */

bool cypher_scanner_has_error(const CypherScannerState *state)
{
    return state ? state->has_error : true;
}

const CypherScannerError* cypher_scanner_get_error(const CypherScannerState *state)
{
    return state ? &state->last_error : NULL;
}

void cypher_scanner_clear_error(CypherScannerState *state)
{
    if (!state) {
        return;
    }
    
    state->has_error = false;
    
    if (state->last_error.message) {
        free(state->last_error.message);
        state->last_error.message = NULL;
    }
    
    state->last_error.line = 0;
    state->last_error.column = 0;
}

/* Utility functions */

const char* cypher_token_type_name(CypherTokenType type)
{
    switch (type) {
        case CYPHER_TOKEN_EOF:         return "EOF";
        case CYPHER_TOKEN_INTEGER:     return "INTEGER";
        case CYPHER_TOKEN_DECIMAL:     return "DECIMAL";
        case CYPHER_TOKEN_STRING:      return "STRING";
        case CYPHER_TOKEN_IDENTIFIER:  return "IDENTIFIER";
        case CYPHER_TOKEN_PARAMETER:   return "PARAMETER";
        case CYPHER_TOKEN_BQIDENT:     return "BQIDENT";
        case CYPHER_TOKEN_OPERATOR:    return "OPERATOR";
        case CYPHER_TOKEN_CHAR:        return "CHAR";
        case CYPHER_TOKEN_NOT_EQ:      return "NOT_EQ";
        case CYPHER_TOKEN_LT_EQ:       return "LT_EQ";
        case CYPHER_TOKEN_GT_EQ:       return "GT_EQ";
        case CYPHER_TOKEN_DOT_DOT:     return "DOT_DOT";
        case CYPHER_TOKEN_TYPECAST:    return "TYPECAST";
        case CYPHER_TOKEN_PLUS_EQ:     return "PLUS_EQ";
        case CYPHER_TOKEN_REGEX_MATCH: return "REGEX_MATCH";
        case CYPHER_TOKEN_KEYWORD:     return "KEYWORD";
        default:                       return "UNKNOWN";
    }
}

void cypher_token_free(CypherToken *token)
{
    if (!token) {
        return;
    }
    
    /* Free text field */
    if (token->text) {
        free((void*)token->text);
        token->text = NULL;
    }
    
    /* Free string value if applicable */
    if (token->type == CYPHER_TOKEN_STRING ||
        token->type == CYPHER_TOKEN_IDENTIFIER ||
        token->type == CYPHER_TOKEN_PARAMETER ||
        token->type == CYPHER_TOKEN_BQIDENT ||
        token->type == CYPHER_TOKEN_OPERATOR ||
        token->type == CYPHER_TOKEN_KEYWORD) {
        if (token->value.string) {
            free(token->value.string);
            token->value.string = NULL;
        }
    }
}