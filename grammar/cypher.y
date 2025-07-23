%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ast.h"

// Forward declarations
int yylex(void);
void yyerror(const char *s);

// Global result storage for parsed AST
cypher_ast_node_t *parse_result = NULL;
%}

%define api.value.type union
%define parse.error detailed

// Token definitions
%token <char*> IDENTIFIER STRING_LITERAL INTEGER_LITERAL FLOAT_LITERAL
%token CREATE MATCH RETURN TRUE FALSE
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET
%token DOT COMMA COLON SEMICOLON
%token ARROW_RIGHT ARROW_LEFT DASH

// AST node types
%type <cypher_ast_node_t*> statement
%type <cypher_ast_node_t*> create_statement
%type <cypher_ast_node_t*> match_statement  
%type <cypher_ast_node_t*> return_statement
%type <cypher_ast_node_t*> node_pattern
%type <cypher_ast_node_t*> relationship_pattern
%type <cypher_ast_node_t*> edge_pattern
%type <cypher_ast_node_t*> variable
%type <cypher_ast_node_t*> label
%type <cypher_ast_node_t*> property
%type <cypher_ast_node_t*> property_list
%type <cypher_ast_node_t*> literal
%type <cypher_ast_node_t*> string_literal
%type <cypher_ast_node_t*> integer_literal
%type <cypher_ast_node_t*> float_literal
%type <cypher_ast_node_t*> boolean_literal

%start program

%%

program:
    statement { parse_result = $1; }
    ;

statement:
    create_statement { $$ = $1; }
    | match_statement return_statement { 
        // Create a compound statement with MATCH and RETURN
        $$ = ast_create_compound_statement($1, $2);
    }
    ;

// CREATE (n:Person {name: "John"}) or CREATE (a)-[:KNOWS]->(b)
create_statement:
    CREATE node_pattern {
        $$ = ast_create_create_statement($2);
    }
    | CREATE relationship_pattern {
        $$ = ast_create_create_statement($2);
    }
    ;

// MATCH (n:Person) or MATCH (n:Person {name: "John"}) or MATCH (a)-[:KNOWS]->(b)
match_statement:
    MATCH node_pattern {
        $$ = ast_create_match_statement($2);
    }
    | MATCH relationship_pattern {
        $$ = ast_create_match_statement($2);
    }
    ;

// RETURN n
return_statement:
    RETURN variable {
        $$ = ast_create_return_statement($2);
    }
    ;

// (n:Person) or (n:Person {name: "John"})
node_pattern:
    LPAREN variable COLON label RPAREN {
        $$ = ast_create_node_pattern($2, $4, NULL);
    }
    | LPAREN variable COLON label LBRACE property_list RBRACE RPAREN {
        $$ = ast_create_node_pattern($2, $4, $6);
    }
    ;

// (a)-[:TYPE]->(b) or (a)-[r:TYPE {prop: value}]->(b)
relationship_pattern:
    node_pattern DASH LBRACKET COLON label RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, $5, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $8, 1);  // 1 = right direction
    }
    | node_pattern DASH LBRACKET variable COLON label RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $9, 1);  // 1 = right direction
    }
    | node_pattern DASH LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, $8);
        $$ = ast_create_relationship_pattern($1, edge, $12, 1);  // 1 = right direction
    }
    | node_pattern ARROW_LEFT LBRACKET COLON label RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, $5, NULL);
        $$ = ast_create_relationship_pattern($8, edge, $1, -1);  // -1 = left direction
    }
    | node_pattern ARROW_LEFT LBRACKET variable COLON label RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, NULL);
        $$ = ast_create_relationship_pattern($9, edge, $1, -1);  // -1 = left direction  
    }
    | node_pattern ARROW_LEFT LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, $8);
        $$ = ast_create_relationship_pattern($12, edge, $1, -1);  // -1 = left direction
    }
    ;

// Edge pattern: [r:TYPE] or [r:TYPE {prop: value}]  
edge_pattern:
    LBRACKET COLON label RBRACKET {
        $$ = ast_create_edge_pattern(NULL, $3, NULL);
    }
    | LBRACKET variable COLON label RBRACKET {
        $$ = ast_create_edge_pattern($2, $4, NULL);
    }
    | LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET {
        $$ = ast_create_edge_pattern($2, $4, $6);
    }
    ;

// Property list: single property or comma-separated properties
property_list:
    property {
        cypher_ast_node_t *list = ast_create_property_list();
        $$ = ast_add_property_to_list(list, $1);
    }
    | property_list COMMA property {
        $$ = ast_add_property_to_list($1, $3);
    }
    ;

// Variable name (e.g., "n")
variable:
    IDENTIFIER {
        $$ = ast_create_variable($1);
        free($1);  // Free the token string
    }
    ;

// Label name (e.g., "Person")
label:
    IDENTIFIER {
        $$ = ast_create_label($1);
        free($1);  // Free the token string
    }
    ;

// Property: key: value (supports different literal types)
property:
    IDENTIFIER COLON literal {
        $$ = ast_create_property($1, $3);
        free($1);  // Free the key string
    }
    ;

// Literal values (string, integer, float, boolean)
literal:
    string_literal { $$ = $1; }
    | integer_literal { $$ = $1; }
    | float_literal { $$ = $1; }
    | boolean_literal { $$ = $1; }
    ;

// Literal value types
string_literal:
    STRING_LITERAL {
        $$ = ast_create_string_literal($1);
        free($1);  // Free the token string
    }
    ;

integer_literal:
    INTEGER_LITERAL {
        $$ = ast_create_integer_literal($1);
        free($1);  // Free the token string
    }
    ;

float_literal:
    FLOAT_LITERAL {
        $$ = ast_create_float_literal($1);
        free($1);  // Free the token string
    }
    ;

boolean_literal:
    TRUE {
        $$ = ast_create_boolean_literal(1);
    }
    | FALSE {
        $$ = ast_create_boolean_literal(0);
    }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

// Forward declarations for lexer functions
extern void init_lexer(const char *input);
extern void cleanup_lexer(void);

// Main parse function
cypher_ast_node_t* parse_cypher_query(const char *query) {
    parse_result = NULL;
    
    // Initialize lexer with input string
    init_lexer(query);
    
    // Parse the input
    int result = yyparse();
    
    // Clean up lexer
    cleanup_lexer();
    
    if (result != 0) {
        // Parse failed - clean up any partial result
        if (parse_result) {
            ast_free(parse_result);
            parse_result = NULL;
        }
        return NULL;
    }
    
    return parse_result;
}