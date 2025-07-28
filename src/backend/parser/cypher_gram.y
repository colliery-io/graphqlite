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
    cypher_set *set;
    cypher_set_item *set_item;
    cypher_delete *delete;
    cypher_delete_item *delete_item;
    cypher_return_item *return_item;
    cypher_order_by_item *order_by_item;
    cypher_literal *literal;
    cypher_identifier *identifier;
    cypher_parameter *parameter;
    cypher_label_expr *label_expr;
    cypher_not_expr *not_expr;
    cypher_binary_op *binary_op;
    cypher_node_pattern *node_pattern;
    cypher_rel_pattern *rel_pattern;
    cypher_path *path;
    cypher_map *map;
    cypher_map_pair *map_pair;
}

/* Terminal tokens */
%token <integer> INTEGER
%token <decimal> DECIMAL
%token <string> STRING IDENTIFIER PARAMETER BQIDENT

/* Multi-character operators */
%token NOT_EQ LT_EQ GT_EQ DOT_DOT TYPECAST PLUS_EQ

/* Keywords */
%token MATCH RETURN CREATE WHERE WITH SET DELETE REMOVE MERGE UNWIND DETACH
%token OPTIONAL DISTINCT ORDER BY SKIP LIMIT AS ASC DESC
%token AND OR NOT IN IS NULL TRUE FALSE
%token UNION ALL CASE WHEN THEN ELSE END

/* Non-terminal types */
%type <query> stmt
%type <list> clause_list
%type <node> clause

%type <match> match_clause
%type <return_clause> return_clause
%type <create> create_clause
%type <set> set_clause
%type <delete> delete_clause

%type <list> pattern_list return_item_list set_item_list delete_item_list
%type <set_item> set_item
%type <delete_item> delete_item
%type <boolean> detach_opt
%type <path> path
%type <node_pattern> node_pattern
%type <rel_pattern> rel_pattern
%type <return_item> return_item

%type <node> expr primary_expr literal_expr function_call
%type <literal> literal
%type <identifier> identifier
%type <parameter> parameter
%type <map> map_literal properties_opt
%type <map_pair> map_pair
%type <list> map_pair_list

%type <string> label_opt variable_opt
%type <boolean> optional_opt distinct_opt
%type <list> order_by_opt order_by_list rel_type_list
%type <node> skip_opt limit_opt where_opt
%type <order_by_item> order_by_item

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
%right UNARY_MINUS UNARY_PLUS

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
    | set_clause        { $$ = (ast_node*)$1; }
    | delete_clause     { $$ = (ast_node*)$1; }
    ;

/* MATCH clause */
match_clause:
    optional_opt MATCH pattern_list where_opt
        {
            $$ = make_cypher_match($3, $4, $1);
        }
    ;

optional_opt:
    /* empty */     { $$ = false; }
    | OPTIONAL      { $$ = true; }
    ;

/* RETURN clause */
return_clause:
    RETURN distinct_opt return_item_list order_by_opt skip_opt limit_opt
        {
            $$ = make_cypher_return($2, $3, $4, $5, $6);
        }
    ;

distinct_opt:
    /* empty */     { $$ = false; }
    | DISTINCT      { $$ = true; }
    ;

order_by_opt:
    /* empty */     { $$ = NULL; }
    | ORDER BY order_by_list { $$ = $3; }
    ;

skip_opt:
    /* empty */     { $$ = NULL; }
    | SKIP expr     { $$ = $2; }
    ;

limit_opt:
    /* empty */     { $$ = NULL; }
    | LIMIT expr    { $$ = $2; }
    ;

where_opt:
    /* empty */     { $$ = NULL; }
    | WHERE expr    { $$ = $2; }
    ;

order_by_list:
    order_by_item
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | order_by_list ',' order_by_item
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

order_by_item:
    expr            { $$ = make_order_by_item($1, false); /* Default is ASC */ }
    | expr ASC      { $$ = make_order_by_item($1, false); }
    | expr DESC     { $$ = make_order_by_item($1, true); }
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

set_item_list:
    set_item
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | set_item_list ',' set_item
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

set_item:
    expr '=' expr
        {
            $$ = make_cypher_set_item($1, $3);
        }
    | IDENTIFIER ':' IDENTIFIER
        {
            /* SET n:Label syntax */
            cypher_identifier *var = make_identifier($1, @1.first_line);
            cypher_label_expr *label = make_label_expr((ast_node*)var, $3, @3.first_line);
            $$ = make_cypher_set_item((ast_node*)label, NULL);
            free($1);
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

/* SET clause */
set_clause:
    SET set_item_list
        {
            $$ = make_cypher_set($2);
        }
    ;

/* DELETE clause */
delete_clause:
    detach_opt DELETE delete_item_list
        {
            $$ = make_cypher_delete($3, $1);
        }
    ;

delete_item_list:
    delete_item
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | delete_item_list ',' delete_item
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

delete_item:
    IDENTIFIER
        {
            $$ = make_delete_item($1);
            free($1);
        }
    ;

detach_opt:
    DETACH
        {
            $$ = true;
        }
    | /* EMPTY */
        {
            $$ = false;
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
    | IDENTIFIER '=' path
        {
            $$ = make_path_with_var($1, $3->elements);
            /* Free the anonymous path structure, but keep its elements */
            free($3);
        }
    ;

node_pattern:
    '(' variable_opt label_opt properties_opt ')'
        {
            $$ = make_node_pattern($2, $3, (ast_node*)$4);
        }
    ;

rel_pattern:
    '-' '[' variable_opt ':' IDENTIFIER ']' '-' '>'
        {
            $$ = make_rel_pattern($3, $5, NULL, false, true);
            free($5);
        }
    | '-' '[' variable_opt ':' IDENTIFIER properties_opt ']' '-' '>'
        {
            $$ = make_rel_pattern($3, $5, (ast_node*)$6, false, true);
            free($5);
        }
    | '-' '[' variable_opt ':' rel_type_list ']' '-' '>'
        {
            $$ = make_rel_pattern_multi_type($3, $5, NULL, false, true);
        }
    | '-' '[' variable_opt ':' rel_type_list properties_opt ']' '-' '>'
        {
            $$ = make_rel_pattern_multi_type($3, $5, (ast_node*)$6, false, true);
        }
    | '<' '-' '[' variable_opt ':' IDENTIFIER ']' '-'
        {
            $$ = make_rel_pattern($4, $6, NULL, true, false);
            free($6);
        }
    | '<' '-' '[' variable_opt ':' IDENTIFIER properties_opt ']' '-'
        {
            $$ = make_rel_pattern($4, $6, (ast_node*)$7, true, false);
            free($6);
        }
    | '<' '-' '[' variable_opt ':' rel_type_list ']' '-'
        {
            $$ = make_rel_pattern_multi_type($4, $6, NULL, true, false);
        }
    | '<' '-' '[' variable_opt ':' rel_type_list properties_opt ']' '-'
        {
            $$ = make_rel_pattern_multi_type($4, $6, (ast_node*)$7, true, false);
        }
    | '-' '[' variable_opt ']' '-' '>'
        {
            $$ = make_rel_pattern($3, NULL, NULL, false, true);
        }
    | '-' '[' variable_opt properties_opt ']' '-' '>'
        {
            $$ = make_rel_pattern($3, NULL, (ast_node*)$4, false, true);
        }
    | '<' '-' '[' variable_opt ']' '-'
        {
            $$ = make_rel_pattern($4, NULL, NULL, true, false);
        }
    | '<' '-' '[' variable_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern($4, NULL, (ast_node*)$5, true, false);
        }
    | '-' '[' variable_opt ']' '-'
        {
            $$ = make_rel_pattern($3, NULL, NULL, false, false);
        }
    | '-' '[' variable_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern($3, NULL, (ast_node*)$4, false, false);
        }
    | '-' '[' variable_opt ':' IDENTIFIER ']' '-'
        {
            $$ = make_rel_pattern($3, $5, NULL, false, false);
            free($5);
        }
    | '-' '[' variable_opt ':' IDENTIFIER properties_opt ']' '-'
        {
            $$ = make_rel_pattern($3, $5, (ast_node*)$6, false, false);
            free($5);
        }
    | '-' '[' variable_opt ':' rel_type_list ']' '-'
        {
            $$ = make_rel_pattern_multi_type($3, $5, NULL, false, false);
        }
    | '-' '[' variable_opt ':' rel_type_list properties_opt ']' '-'
        {
            $$ = make_rel_pattern_multi_type($3, $5, (ast_node*)$6, false, false);
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

rel_type_list:
    IDENTIFIER '|' IDENTIFIER
        {
            $$ = ast_list_create();
            /* Create string literal nodes for both types */
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit3 = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit3);
            free($1);
            free($3);
        }
    | rel_type_list '|' IDENTIFIER
        {
            /* Add another type to the list */
            cypher_literal *type_lit = make_string_literal($3, @3.first_line);
            ast_list_append($1, (ast_node*)type_lit);
            $$ = $1;
            free($3);
        }
    ;

/* Expressions */
expr:
    primary_expr        { $$ = $1; }
    | '+' expr %prec UNARY_PLUS  { $$ = $2; }  /* unary plus - just return the expression */
    | '-' expr %prec UNARY_MINUS  { 
        /* Handle unary minus */
        if ($2->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)$2;
            if (lit->literal_type == LITERAL_INTEGER) {
                lit->value.integer = -lit->value.integer;
                $$ = $2;
            } else if (lit->literal_type == LITERAL_DECIMAL) {
                lit->value.decimal = -lit->value.decimal;
                $$ = $2;
            } else {
                /* For other types, we'd need a unary minus node */
                $$ = $2;
            }
        } else {
            /* For non-literals, we'd need a unary minus expression node */
            $$ = $2;
        }
    }
    | expr '+' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_ADD, $1, $3, @2.first_line); }
    | expr '-' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_SUB, $1, $3, @2.first_line); }
    | expr '*' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_MUL, $1, $3, @2.first_line); }
    | expr '/' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_DIV, $1, $3, @2.first_line); }
    | expr '=' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_EQ, $1, $3, @2.first_line); }
    | expr NOT_EQ expr  { $$ = (ast_node*)make_binary_op(BINARY_OP_NEQ, $1, $3, @2.first_line); }
    | expr '<' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_LT, $1, $3, @2.first_line); }
    | expr '>' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_GT, $1, $3, @2.first_line); }
    | expr LT_EQ expr   { $$ = (ast_node*)make_binary_op(BINARY_OP_LTE, $1, $3, @2.first_line); }
    | expr GT_EQ expr   { $$ = (ast_node*)make_binary_op(BINARY_OP_GTE, $1, $3, @2.first_line); }
    | expr AND expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_AND, $1, $3, @2.first_line); }
    | expr OR expr      { $$ = (ast_node*)make_binary_op(BINARY_OP_OR, $1, $3, @2.first_line); }
    | NOT expr          { $$ = (ast_node*)make_not_expr($2, @1.first_line); }
    | expr IS NULL      { /* TODO: implement IS NULL */ $$ = $1; }
    | expr IS NOT NULL  { /* TODO: implement IS NOT NULL */ $$ = $1; }
    | '(' expr ')'      { $$ = $2; }
    ;

primary_expr:
    literal_expr        { $$ = $1; }
    | identifier        { $$ = (ast_node*)$1; }
    | parameter         { $$ = (ast_node*)$1; }
    | function_call     { $$ = $1; }
    | IDENTIFIER '.' IDENTIFIER
        {
            cypher_identifier *base = make_identifier($1, @1.first_line);
            $$ = (ast_node*)make_property((ast_node*)base, $3, @3.first_line);
            free($1);
            free($3);
        }
    | IDENTIFIER ':' IDENTIFIER
        {
            cypher_identifier *base = make_identifier($1, @1.first_line);
            $$ = (ast_node*)make_label_expr((ast_node*)base, $3, @3.first_line);
            free($1);
            free($3);
        }
    ;

literal_expr:
    literal             { $$ = (ast_node*)$1; }
    ;

function_call:
    IDENTIFIER '(' ')'
        {
            ast_list *args = ast_list_create();
            $$ = (ast_node*)make_function_call($1, args, false, @1.first_line);
            free($1);
        }
    | IDENTIFIER '(' expr ')'
        {
            ast_list *args = ast_list_create();
            ast_list_append(args, $3);
            $$ = (ast_node*)make_function_call($1, args, false, @1.first_line);
            free($1);
        }
    | IDENTIFIER '(' DISTINCT expr ')'
        {
            ast_list *args = ast_list_create();
            ast_list_append(args, $4);
            $$ = (ast_node*)make_function_call($1, args, true, @1.first_line);
            free($1);
        }
    | IDENTIFIER '(' '*' ')'
        {
            ast_list *args = ast_list_create();
            /* For count(*), we'll use a special NULL argument to indicate * */
            ast_list_append(args, NULL);
            $$ = (ast_node*)make_function_call($1, args, false, @1.first_line);
            free($1);
        }
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

/* Property map support */
properties_opt:
    /* empty */         { $$ = NULL; }
    | '{' '}'           { $$ = NULL; }
    | '{' map_pair_list '}'
        {
            $$ = make_map($2, @1.first_line);
        }
    ;

map_literal:
    '{' '}'
        {
            $$ = make_map(ast_list_create(), @1.first_line);
        }
    | '{' map_pair_list '}'
        {
            $$ = make_map($2, @1.first_line);
        }
    ;

map_pair_list:
    map_pair
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | map_pair_list ',' map_pair
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

map_pair:
    IDENTIFIER ':' expr
        {
            $$ = make_map_pair($1, $3, @1.first_line);
        }
    | STRING ':' expr
        {
            $$ = make_map_pair($1, $3, @1.first_line);
        }
    | ASC ':' expr
        {
            $$ = make_map_pair("asc", $3, @1.first_line);
        }
    | DESC ':' expr
        {
            $$ = make_map_pair("desc", $3, @1.first_line);
        }
    | ORDER ':' expr
        {
            $$ = make_map_pair("order", $3, @1.first_line);
        }
    | BY ':' expr
        {
            $$ = make_map_pair("by", $3, @1.first_line);
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