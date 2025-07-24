%{
/*
 * Cypher Grammar for GraphQLite
 * Simplified version based on Apache AGE grammar for SQLite compatibility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser/cypher_ast.h"
#include "parser/cypher_parser.h"

/* Forward declarations */
void cypher_yyerror(CYPHER_YYLTYPE *yylloc, cypher_parser_context *context, const char *msg);
int cypher_yylex(CYPHER_YYSTYPE *yylval, CYPHER_YYLTYPE *yylloc, cypher_parser_context *context);

%}

%locations
%define api.pure full
%define api.prefix {cypher_yy}
%define api.token.prefix {CYPHER_}
%parse-param {cypher_parser_context *context}
%lex-param {cypher_parser_context *context}

%union {
    int integer;
    double decimal;
    char *string;
    bool boolean;
    
    /* AST node types */
    ast_node *node;
    ast_list *list;
    
    /* Specific node types for type safety */
    cypher_query *query;
    cypher_match *match;
    cypher_return *return_clause;
    cypher_create *create;
    cypher_return_item *return_item;
    cypher_literal *literal;
    cypher_identifier *identifier;
    cypher_parameter *parameter;
    cypher_node_pattern *node_pattern;
    cypher_rel_pattern *rel_pattern;
    cypher_path *path;
}

/* Terminal tokens */
%token <integer> INTEGER
%token <decimal> DECIMAL
%token <string> STRING IDENTIFIER PARAMETER BQIDENT

/* Multi-character operators */
%token NOT_EQ LT_EQ GT_EQ DOT_DOT TYPECAST PLUS_EQ

/* Keywords */
%token MATCH RETURN CREATE WHERE WITH SET DELETE REMOVE MERGE UNWIND
%token OPTIONAL DISTINCT ORDER BY SKIP LIMIT AS
%token AND OR NOT IN IS NULL TRUE FALSE
%token UNION ALL CASE WHEN THEN ELSE END

/* Non-terminal types */
%type <query> stmt
%type <list> clause_list
%type <node> clause

%type <match> match_clause
%type <return_clause> return_clause
%type <create> create_clause

%type <list> pattern_list return_item_list
%type <path> path
%type <node_pattern> node_pattern
%type <rel_pattern> rel_pattern
%type <return_item> return_item

%type <node> expr primary_expr literal_expr
%type <literal> literal
%type <identifier> identifier
%type <parameter> parameter

%type <string> label_opt variable_opt
%type <boolean> optional_opt distinct_opt

/* Operator precedence (lowest to highest) */
%left OR
%left AND
%right NOT
%left '=' NOT_EQ '<' LT_EQ '>' GT_EQ
%left '+' '-'
%left '*' '/' '%'
%left '^'
%left IN IS
%left '.'
%nonassoc UMINUS

%%

/* Main grammar rules */

stmt:
    clause_list
        {
            $$ = make_cypher_query($1);
            context->result = (ast_node*)$$;
        }
    ;

clause_list:
    clause
        {
            $$ = ast_list_create();
            ast_list_append($$, $1);
        }
    | clause_list clause
        {
            ast_list_append($1, $2);
            $$ = $1;
        }
    ;

clause:
    match_clause        { $$ = (ast_node*)$1; }
    | return_clause     { $$ = (ast_node*)$1; }
    | create_clause     { $$ = (ast_node*)$1; }
    ;

/* MATCH clause */
match_clause:
    optional_opt MATCH pattern_list
        {
            $$ = make_cypher_match($3, NULL, $1);
        }
    | optional_opt MATCH pattern_list WHERE expr
        {
            $$ = make_cypher_match($3, $5, $1);
        }
    ;

optional_opt:
    /* empty */     { $$ = false; }
    | OPTIONAL      { $$ = true; }
    ;

/* RETURN clause */
return_clause:
    RETURN distinct_opt return_item_list
        {
            $$ = make_cypher_return($2, $3, NULL, NULL, NULL);
        }
    ;

distinct_opt:
    /* empty */     { $$ = false; }
    | DISTINCT      { $$ = true; }
    ;

return_item_list:
    return_item
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | return_item_list ',' return_item
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

return_item:
    expr
        {
            $$ = make_return_item($1, NULL);
        }
    | expr AS IDENTIFIER
        {
            $$ = make_return_item($1, $3);
            free($3);
        }
    ;

/* CREATE clause */
create_clause:
    CREATE pattern_list
        {
            $$ = make_cypher_create($2);
        }
    ;

/* Pattern matching */
pattern_list:
    path
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | pattern_list ',' path
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

path:
    node_pattern
        {
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)$1);
            $$ = make_path(elements);
        }
    | path rel_pattern node_pattern
        {
            /* Create a new list copying elements from the existing path */
            ast_list *new_elements = ast_list_create();
            for (int i = 0; i < $1->elements->count; i++) {
                ast_list_append(new_elements, $1->elements->items[i]);
            }
            ast_list_append(new_elements, (ast_node*)$2);
            ast_list_append(new_elements, (ast_node*)$3);
            
            /* Free only the old path structure, not its elements (which are reused) */
            free($1->elements->items);  /* Free the items array */
            free($1->elements);         /* Free the list structure */
            free($1);                   /* Free the path structure */
            
            $$ = make_path(new_elements);
        }
    ;

node_pattern:
    '(' variable_opt label_opt ')'
        {
            $$ = make_node_pattern($2, $3, NULL);
        }
    | '(' variable_opt label_opt '{' '}' ')'
        {
            /* Empty properties for now */
            $$ = make_node_pattern($2, $3, NULL);
        }
    ;

rel_pattern:
    '-' '[' variable_opt ':' IDENTIFIER ']' '-' '>'
        {
            $$ = make_rel_pattern($3, $5, NULL, false, true);
            free($5);
        }
    | '<' '-' '[' variable_opt ':' IDENTIFIER ']' '-'
        {
            $$ = make_rel_pattern($4, $6, NULL, true, false);
            free($6);
        }
    | '-' '[' variable_opt ']' '-' '>'
        {
            $$ = make_rel_pattern($3, NULL, NULL, false, true);
        }
    | '<' '-' '[' variable_opt ']' '-'
        {
            $$ = make_rel_pattern($4, NULL, NULL, true, false);
        }
    | '-' '[' variable_opt ']' '-'
        {
            $$ = make_rel_pattern($3, NULL, NULL, false, false);
        }
    ;

variable_opt:
    /* empty */     { $$ = NULL; }
    | IDENTIFIER    { $$ = $1; }
    ;

label_opt:
    /* empty */         { $$ = NULL; }
    | ':' IDENTIFIER    { $$ = $2; }
    ;

/* Expressions */
expr:
    primary_expr        { $$ = $1; }
    | expr '+' expr     { /* TODO: implement binary operations */ $$ = $1; }
    | expr '-' expr     { /* TODO: implement binary operations */ $$ = $1; }
    | expr '*' expr     { /* TODO: implement binary operations */ $$ = $1; }
    | expr '/' expr     { /* TODO: implement binary operations */ $$ = $1; }
    | expr '=' expr     { /* TODO: implement comparisons */ $$ = $1; }
    | expr NOT_EQ expr  { /* TODO: implement comparisons */ $$ = $1; }
    | expr '<' expr     { /* TODO: implement comparisons */ $$ = $1; }
    | expr '>' expr     { /* TODO: implement comparisons */ $$ = $1; }
    | expr LT_EQ expr   { /* TODO: implement comparisons */ $$ = $1; }
    | expr GT_EQ expr   { /* TODO: implement comparisons */ $$ = $1; }
    | expr AND expr     { /* TODO: implement logical operations */ $$ = $1; }
    | expr OR expr      { /* TODO: implement logical operations */ $$ = $1; }
    | NOT expr          { /* TODO: implement NOT */ $$ = $2; }
    | '(' expr ')'      { $$ = $2; }
    ;

primary_expr:
    literal_expr        { $$ = $1; }
    | identifier        { $$ = (ast_node*)$1; }
    | parameter         { $$ = (ast_node*)$1; }
    | IDENTIFIER '.' IDENTIFIER
        {
            cypher_identifier *base = make_identifier($1, @1.first_line);
            $$ = (ast_node*)make_property((ast_node*)base, $3, @3.first_line);
            free($1);
            free($3);
        }
    ;

literal_expr:
    literal             { $$ = (ast_node*)$1; }
    ;

literal:
    INTEGER
        {
            $$ = make_integer_literal($1, @1.first_line);
        }
    | DECIMAL
        {
            $$ = make_decimal_literal($1, @1.first_line);
        }
    | STRING
        {
            $$ = make_string_literal($1, @1.first_line);
            free($1);
        }
    | TRUE
        {
            $$ = make_boolean_literal(true, @1.first_line);
        }
    | FALSE
        {
            $$ = make_boolean_literal(false, @1.first_line);
        }
    | NULL
        {
            $$ = make_null_literal(@1.first_line);
        }
    ;

identifier:
    IDENTIFIER
        {
            $$ = make_identifier($1, @1.first_line);
            free($1);
        }
    | BQIDENT
        {
            $$ = make_identifier($1, @1.first_line);
            free($1);
        }
    ;

parameter:
    PARAMETER
        {
            $$ = make_parameter($1, @1.first_line);
            free($1);
        }
    ;

%%

/* Error handling function */
void cypher_yyerror(CYPHER_YYLTYPE *yylloc, cypher_parser_context *context, const char *msg)
{
    if (!context || !msg) {
        return;
    }
    
    context->has_error = true;
    context->error_location = yylloc ? yylloc->first_line : -1;
    
    /* Create detailed error message */
    char error_buffer[512];
    if (yylloc && yylloc->first_line > 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                 "Parse error at line %d, column %d: %s",
                 yylloc->first_line, yylloc->first_column, msg);
    } else {
        snprintf(error_buffer, sizeof(error_buffer), "Parse error: %s", msg);
    }
    
    free(context->error_message);
    context->error_message = strdup(error_buffer);
}

/* cypher_yylex function is implemented in cypher_parser.c */