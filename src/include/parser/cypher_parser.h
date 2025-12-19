#ifndef CYPHER_PARSER_H
#define CYPHER_PARSER_H

#include "parser/cypher_ast.h"
#include "parser/cypher_scanner.h"

/* Parser context structure */
typedef struct cypher_parser_context {
    CypherScannerState *scanner;
    ast_node *result;
    char *error_message;
    int error_location;
    bool has_error;
    char *last_token_text;  /* For better error messages */
} cypher_parser_context;

/* Forward declaration of Bison-generated types */
#ifndef CYPHER_GRAM_TAB_H
typedef union CYPHER_YYSTYPE CYPHER_YYSTYPE;
typedef struct CYPHER_YYLTYPE CYPHER_YYLTYPE;
#endif

/* Function prototypes */

/* Main parser interface */
ast_node* parse_cypher_query(const char *query);
void cypher_parser_free_result(ast_node *result);

/* Extended parser interface that returns error details */
typedef struct {
    ast_node *ast;
    char *error_message;
} cypher_parse_result;

cypher_parse_result* parse_cypher_query_ext(const char *query);
void cypher_parse_result_free(cypher_parse_result *result);

/* Parser context management */
cypher_parser_context* cypher_parser_context_create(void);
void cypher_parser_context_destroy(cypher_parser_context *context);

/* Token bridge functions */
int cypher_yylex(CYPHER_YYSTYPE *yylval, CYPHER_YYLTYPE *yylloc, cypher_parser_context *context);
int cypher_token_to_bison(CypherToken *token);
const char* cypher_keyword_to_token_name(int keyword_id);

/* Error handling */
void cypher_yyerror(CYPHER_YYLTYPE *yylloc, cypher_parser_context *context, const char *msg);
const char* cypher_parser_get_error(ast_node *result);

/* Bison parser function (will be generated) */
int cypher_yyparse(cypher_parser_context *context);

/* Token name utilities */
const char* cypher_token_name(int token);

#endif /* CYPHER_PARSER_H */