%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cypher/cypher_ast.h"

// Forward declarations
int yylex(void);
void yyerror(const char *s);

// Global result storage
cypher_ast_node_t *cypher_parse_result = NULL;
%}

%define api.value.type union
%define parse.error detailed

%token <char*> IDENTIFIER STRING_LITERAL NUMBER_LITERAL
%token CREATE MATCH RETURN
%token LPAREN RPAREN LBRACE RBRACE 
%token DOT COMMA COLON
%token SEMICOLON

%type <cypher_ast_node_t*> statement
%type <cypher_ast_node_t*> create_statement  
%type <cypher_ast_node_t*> match_statement
%type <cypher_ast_node_t*> return_statement
%type <cypher_ast_node_t*> node_pattern
%type <cypher_ast_node_t*> variable
%type <cypher_ast_node_t*> label_list
%type <cypher_ast_node_t*> label
%type <cypher_ast_node_t*> return_items
%type <cypher_ast_node_t*> return_item

%start program

%%

program:
    statement { cypher_parse_result = $1; }
    ;

statement:
    create_statement { $$ = $1; }
    | match_statement { $$ = $1; }
    | match_statement return_statement { 
        $$ = cypher_ast_create_linear_statement($1);
        // TODO: Chain MATCH and RETURN properly
    }
    ;

/* CREATE (n:Person) */
create_statement:
    CREATE node_pattern {
        $$ = cypher_ast_create_create_clause($2);
    }
    ;

/* MATCH (n:Person) */
match_statement:
    MATCH node_pattern {
        $$ = cypher_ast_create_match_clause($2, NULL);
    }
    ;

/* RETURN n */
return_statement:
    RETURN return_items {
        $$ = cypher_ast_create_return_clause($2, false, NULL, NULL, NULL);
    }
    ;

/* (n) or (n:Person) or (:Person) */
node_pattern:
    LPAREN variable RPAREN {
        $$ = cypher_ast_create_node_pattern($2, NULL, NULL);
    }
    | LPAREN variable COLON label_list RPAREN {
        $$ = cypher_ast_create_node_pattern($2, $4, NULL);
    }
    | LPAREN COLON label_list RPAREN {
        $$ = cypher_ast_create_node_pattern(NULL, $3, NULL);
    }
    | LPAREN RPAREN {
        $$ = cypher_ast_create_node_pattern(NULL, NULL, NULL);
    }
    ;

variable:
    IDENTIFIER {
        $$ = cypher_ast_create_variable($1);
        free($1);  // Free the string token
    }
    ;

label_list:
    label { $$ = $1; }
    ;

label:
    IDENTIFIER {
        $$ = cypher_ast_create_label_name($1);
        free($1);  // Free the string token
    }
    ;

return_items:
    return_item { $$ = $1; }
    ;

return_item:
    variable { $$ = $1; }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

// Interface function
cypher_ast_node_t* cypher_bison_parse_string(const char *input) {
    cypher_parse_result = NULL;
    
    // TODO: Set up lexer with input string and call yyparse()
    (void)input;  // Suppress unused parameter warning
    
    return cypher_parse_result;
}