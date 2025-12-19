/*
 * Cypher Parser Implementation
 * Bridge between scanner and Bison-generated parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/cypher_parser.h"
#include "parser/cypher_keywords.h"
#include "parser/cypher_debug.h"
#include "cypher_gram.tab.h"

/* Keyword to token mapping table */
static struct {
    const char *keyword;
    int token;
} keyword_token_map[] = {
    {"all", CYPHER_ALL},
    {"and", CYPHER_AND},
    {"as", CYPHER_AS},
    {"asc", CYPHER_ASC},
    {"by", CYPHER_BY},
    {"case", CYPHER_CASE},
    {"create", CYPHER_CREATE},
    {"delete", CYPHER_DELETE},
    {"desc", CYPHER_DESC},
    {"detach", CYPHER_DETACH},
    {"distinct", CYPHER_DISTINCT},
    {"else", CYPHER_ELSE},
    {"end", CYPHER_END},
    {"false", CYPHER_FALSE},
    {"in", CYPHER_IN},
    {"is", CYPHER_IS},
    {"limit", CYPHER_LIMIT},
    {"match", CYPHER_MATCH},
    {"merge", CYPHER_MERGE},
    {"not", CYPHER_NOT},
    {"null", CYPHER_NULL},
    {"optional", CYPHER_OPTIONAL},
    {"or", CYPHER_OR},
    {"order", CYPHER_ORDER},
    {"remove", CYPHER_REMOVE},
    {"return", CYPHER_RETURN},
    {"set", CYPHER_SET},
    {"skip", CYPHER_SKIP},
    {"then", CYPHER_THEN},
    {"true", CYPHER_TRUE},
    {"union", CYPHER_UNION},
    {"unwind", CYPHER_UNWIND},
    {"when", CYPHER_WHEN},
    {"where", CYPHER_WHERE},
    {"with", CYPHER_WITH},
    {NULL, 0}
};

/* Main parser interface */

/* Public extended parser interface */
cypher_parse_result* parse_cypher_query_ext(const char *query)
{
    cypher_parse_result *result = calloc(1, sizeof(cypher_parse_result));
    if (!result) {
        return NULL;
    }
    
    if (!query) {
        result->error_message = strdup("Query string is NULL");
        return result;
    }
    
    cypher_parser_context *context = cypher_parser_context_create();
    if (!context) {
        result->error_message = strdup("Failed to create parser context");
        return result;
    }
    
    /* Set up scanner with input query */
    context->scanner = cypher_scanner_create();
    if (!context->scanner) {
        result->error_message = strdup("Failed to create scanner");
        cypher_parser_context_destroy(context);
        return result;
    }
    
    if (cypher_scanner_set_input_string(context->scanner, query) != 0) {
        result->error_message = strdup("Failed to set scanner input");
        cypher_parser_context_destroy(context);
        return result;
    }
    
    /* Parse the query */
    int parse_result = cypher_yyparse(context);
    
    if (parse_result == 0 && !context->has_error) {
        /* Parsing succeeded */
        result->ast = context->result;
        context->result = NULL; /* Transfer ownership */
    } else {
        /* Parsing failed - copy error message */
        if (context->error_message) {
            result->error_message = strdup(context->error_message);
        } else {
            result->error_message = strdup("Parse failed with unknown error");
        }
    }
    
    cypher_parser_context_destroy(context);
    return result;
}

ast_node* parse_cypher_query(const char *query)
{
    cypher_parse_result *ext_result = parse_cypher_query_ext(query);
    if (!ext_result) {
        return NULL;
    }
    
    ast_node *ast = ext_result->ast;
    
    /* For backward compatibility, just return the AST and discard error */
    free(ext_result->error_message);
    free(ext_result);
    
    return ast;
}

/* Free extended parse result */
void cypher_parse_result_free(cypher_parse_result *result)
{
    if (!result) {
        return;
    }
    
    if (result->ast) {
        cypher_parser_free_result(result->ast);
    }
    
    free(result->error_message);
    free(result);
}

void cypher_parser_free_result(ast_node *result)
{
    CYPHER_DEBUG("cypher_parser_free_result called with %p", (void*)result);
    fflush(stdout);
    
    if (!result) {
        CYPHER_DEBUG("result is NULL, returning");
        return;
    }
    
    CYPHER_DEBUG("About to call ast_node_free");
    fflush(stdout);
    
    ast_node_free(result);
    
    CYPHER_DEBUG("ast_node_free completed");
}

/* Parser context management */

cypher_parser_context* cypher_parser_context_create(void)
{
    cypher_parser_context *context = calloc(1, sizeof(cypher_parser_context));
    if (!context) {
        return NULL;
    }

    context->scanner = NULL;
    context->result = NULL;
    context->error_message = NULL;
    context->error_location = -1;
    context->has_error = false;
    context->last_token_text = NULL;

    return context;
}

void cypher_parser_context_destroy(cypher_parser_context *context)
{
    if (!context) {
        return;
    }

    if (context->scanner) {
        cypher_scanner_destroy(context->scanner);
    }

    if (context->result) {
        ast_node_free(context->result);
    }

    free(context->error_message);
    free(context->last_token_text);
    free(context);
}

/* Token bridge functions */

int cypher_yylex(CYPHER_YYSTYPE *yylval, CYPHER_YYLTYPE *yylloc, cypher_parser_context *context)
{
    if (!context || !context->scanner || !yylval || !yylloc) {
        return 0;
    }
    
    CypherToken token = cypher_scanner_next_token(context->scanner);
    
    /* Check for scanner errors */
    if (cypher_scanner_has_error(context->scanner)) {
        const CypherScannerError *error = cypher_scanner_get_error(context->scanner);
        if (error && error->message) {
            context->has_error = true;
            context->error_message = strdup(error->message);
            context->error_location = error->line;
        }
        cypher_token_free(&token);
        return 0;
    }
    
    /* Set location information */
    yylloc->first_line = yylloc->last_line = token.line;
    yylloc->first_column = yylloc->last_column = token.column;
    
    /* Convert token to Bison format */
    int bison_token = cypher_token_to_bison(&token);
    
    /* Set token value based on type */
    switch (token.type) {
        case CYPHER_TOKEN_INTEGER:
            yylval->integer = token.value.integer;
            break;
            
        case CYPHER_TOKEN_DECIMAL:
            yylval->decimal = token.value.decimal;
            break;
            
        case CYPHER_TOKEN_STRING:
        case CYPHER_TOKEN_IDENTIFIER:
        case CYPHER_TOKEN_PARAMETER:
        case CYPHER_TOKEN_BQIDENT:
            yylval->string = token.value.string ? strdup(token.value.string) : NULL;
            break;
            
        case CYPHER_TOKEN_KEYWORD:
            yylval->string = token.value.string ? strdup(token.value.string) : NULL;
            break;
            
        case CYPHER_TOKEN_CHAR:
            /* Single character tokens just use their ASCII value */
            bison_token = token.value.character;
            break;
            
        default:
            /* Other tokens don't need special value handling */
            break;
    }
    
    cypher_token_free(&token);
    return bison_token;
}

int cypher_token_to_bison(CypherToken *token)
{
    if (!token) {
        return 0;
    }
    
    switch (token->type) {
        case CYPHER_TOKEN_EOF:
            return 0;
            
        case CYPHER_TOKEN_INTEGER:
            return CYPHER_INTEGER;
            
        case CYPHER_TOKEN_DECIMAL:
            return CYPHER_DECIMAL;
            
        case CYPHER_TOKEN_STRING:
            return CYPHER_STRING;
            
        case CYPHER_TOKEN_IDENTIFIER:
            return CYPHER_IDENTIFIER;
            
        case CYPHER_TOKEN_PARAMETER:
            return CYPHER_PARAMETER;
            
        case CYPHER_TOKEN_BQIDENT:
            return CYPHER_BQIDENT;
            
        case CYPHER_TOKEN_OPERATOR:
            /* Simple operators just use their first character */
            if (token->value.string && token->value.string[0]) {
                return token->value.string[0];
            }
            return '?'; /* Unknown operator */
            
        case CYPHER_TOKEN_CHAR:
            return token->value.character;
            
        case CYPHER_TOKEN_NOT_EQ:
            return CYPHER_NOT_EQ;
            
        case CYPHER_TOKEN_LT_EQ:
            return CYPHER_LT_EQ;
            
        case CYPHER_TOKEN_GT_EQ:
            return CYPHER_GT_EQ;
            
        case CYPHER_TOKEN_DOT_DOT:
            return CYPHER_DOT_DOT;
            
        case CYPHER_TOKEN_TYPECAST:
            return CYPHER_TYPECAST;
            
        case CYPHER_TOKEN_PLUS_EQ:
            return CYPHER_PLUS_EQ;
            
        case CYPHER_TOKEN_KEYWORD:
            /* Look up keyword in mapping table */
            if (token->value.string) {
                for (int i = 0; keyword_token_map[i].keyword; i++) {
                    if (strcasecmp(token->value.string, keyword_token_map[i].keyword) == 0) {
                        return keyword_token_map[i].token;
                    }
                }
            }
            /* Unknown keyword - treat as identifier */
            return CYPHER_IDENTIFIER;
            
        default:
            return 0;
    }
}

const char* cypher_keyword_to_token_name(int keyword_id)
{
    /* Map keyword IDs back to names for error messages */
    for (int i = 0; keyword_token_map[i].keyword; i++) {
        if (keyword_token_map[i].token == keyword_id) {
            return keyword_token_map[i].keyword;
        }
    }
    return "unknown";
}

/* Error handling - cypher_yyerror is implemented in the grammar file */

const char* cypher_parser_get_error(ast_node *result)
{
    UNUSED_PARAMETER(result);
    /* This function is used to check for parse errors */
    /* If we have a valid AST result, there was no error */
    /* Real error handling is done through the parser context */
    return NULL;
}

/* Token name utilities */

const char* cypher_token_name(int token)
{
    switch (token) {
        case 0:                  return "EOF";
        case CYPHER_INTEGER:     return "INTEGER";
        case CYPHER_DECIMAL:     return "DECIMAL";
        case CYPHER_STRING:      return "STRING";
        case CYPHER_IDENTIFIER:  return "IDENTIFIER";
        case CYPHER_PARAMETER:   return "PARAMETER";
        case CYPHER_BQIDENT:     return "BQIDENT";
        case CYPHER_NOT_EQ:      return "NOT_EQ";
        case CYPHER_LT_EQ:       return "LT_EQ";
        case CYPHER_GT_EQ:       return "GT_EQ";
        case CYPHER_DOT_DOT:     return "DOT_DOT";
        case CYPHER_TYPECAST:    return "TYPECAST";
        case CYPHER_PLUS_EQ:     return "PLUS_EQ";
        case CYPHER_MATCH:       return "MATCH";
        case CYPHER_RETURN:      return "RETURN";
        case CYPHER_CREATE:      return "CREATE";
        case CYPHER_WHERE:       return "WHERE";
        case CYPHER_WITH:        return "WITH";
        case CYPHER_SET:         return "SET";
        case CYPHER_DELETE:      return "DELETE";
        case CYPHER_REMOVE:      return "REMOVE";
        case CYPHER_MERGE:       return "MERGE";
        case CYPHER_UNWIND:      return "UNWIND";
        case CYPHER_OPTIONAL:    return "OPTIONAL";
        case CYPHER_DISTINCT:    return "DISTINCT";
        case CYPHER_ORDER:       return "ORDER";
        case CYPHER_BY:          return "BY";
        case CYPHER_SKIP:        return "SKIP";
        case CYPHER_LIMIT:       return "LIMIT";
        case CYPHER_AS:          return "AS";
        case CYPHER_AND:         return "AND";
        case CYPHER_OR:          return "OR";
        case CYPHER_NOT:         return "NOT";
        case CYPHER_IN:          return "IN";
        case CYPHER_IS:          return "IS";
        case CYPHER_NULL:        return "NULL";
        case CYPHER_TRUE:        return "TRUE";
        case CYPHER_FALSE:       return "FALSE";
        case CYPHER_UNION:       return "UNION";
        case CYPHER_ALL:         return "ALL";
        case CYPHER_CASE:        return "CASE";
        case CYPHER_WHEN:        return "WHEN";
        case CYPHER_THEN:        return "THEN";
        case CYPHER_ELSE:        return "ELSE";
        case CYPHER_END:         return "END";
        default:
            if (token >= 32 && token <= 126) {
                static char char_name[4];
                snprintf(char_name, sizeof(char_name), "'%c'", token);
                return char_name;
            }
            return "UNKNOWN";
    }
}

/* The cypher_yyparse function is generated by Bison */