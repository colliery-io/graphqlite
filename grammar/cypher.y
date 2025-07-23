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
%token CREATE MATCH RETURN WHERE TRUE FALSE
%token AND OR NOT IS NULL_TOKEN
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET
%token DOT COMMA COLON SEMICOLON
%token ARROW_RIGHT ARROW_LEFT DASH
%token EQ NEQ LT GT LE GE

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
%type <cypher_ast_node_t*> where_clause
%type <cypher_ast_node_t*> expression
%type <cypher_ast_node_t*> comparison_expression
%type <cypher_ast_node_t*> property_expression

%start program

%%

program:
    statement { parse_result = $1; }
    ;

statement:
    create_statement { $$ = $1; }
    | create_statement return_statement { 
        // Create a compound statement with CREATE and RETURN
        $$ = ast_create_compound_statement($1, $2);
    }
    | match_statement return_statement { 
        // Create a compound statement with MATCH and RETURN
        $$ = ast_create_compound_statement($1, $2);
    }
    | match_statement where_clause return_statement {
        // Create a compound statement with MATCH, WHERE and RETURN
        cypher_ast_node_t *match_with_where = ast_attach_where_clause($1, $2);
        $$ = ast_create_compound_statement(match_with_where, $3);
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

// (n:Person) or (n:Person {name: "John"}) or (n) or (n {name: "John"})
node_pattern:
    LPAREN variable COLON label RPAREN {
        $$ = ast_create_node_pattern($2, $4, NULL);
    }
    | LPAREN variable COLON label LBRACE property_list RBRACE RPAREN {
        $$ = ast_create_node_pattern($2, $4, $6);
    }
    | LPAREN variable RPAREN {
        $$ = ast_create_node_pattern($2, NULL, NULL);
    }
    | LPAREN variable LBRACE property_list RBRACE RPAREN {
        $$ = ast_create_node_pattern($2, NULL, $4);
    }
    ;

// (a)-[:TYPE]->(b) or (a)-[r:TYPE {prop: value}]->(b) or (a)-[r]->(b) or (a)-->(b)
relationship_pattern:
    node_pattern DASH LBRACKET COLON label RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, $5, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $8, 1);  // 1 = right direction
    }
    | node_pattern DASH LBRACKET COLON label LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, $5, $7);
        $$ = ast_create_relationship_pattern($1, edge, $11, 1);  // 1 = right direction, type with properties
    }
    | node_pattern DASH LBRACKET variable COLON label RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $9, 1);  // 1 = right direction
    }
    | node_pattern DASH LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, $8);
        $$ = ast_create_relationship_pattern($1, edge, $12, 1);  // 1 = right direction
    }
    | node_pattern DASH LBRACKET variable RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, NULL, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $7, 1);  // 1 = right direction, no type
    }
    | node_pattern DASH LBRACKET variable LBRACE property_list RBRACE RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, NULL, $6);
        $$ = ast_create_relationship_pattern($1, edge, $10, 1);  // 1 = right direction, no type but with properties
    }
    | node_pattern DASH LBRACKET RBRACKET ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $6, 1);  // 1 = right direction, no variable or type
    }
    | node_pattern DASH ARROW_RIGHT node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        $$ = ast_create_relationship_pattern($1, edge, $4, 1);  // 1 = right direction, no brackets
    }
    | node_pattern ARROW_LEFT LBRACKET COLON label RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, $5, NULL);
        $$ = ast_create_relationship_pattern($8, edge, $1, -1);  // -1 = left direction
    }
    | node_pattern ARROW_LEFT LBRACKET COLON label LBRACE property_list RBRACE RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, $5, $7);
        $$ = ast_create_relationship_pattern($11, edge, $1, -1);  // -1 = left direction, type with properties
    }
    | node_pattern ARROW_LEFT LBRACKET variable COLON label RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, NULL);
        $$ = ast_create_relationship_pattern($9, edge, $1, -1);  // -1 = left direction  
    }
    | node_pattern ARROW_LEFT LBRACKET variable COLON label LBRACE property_list RBRACE RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, $6, $8);
        $$ = ast_create_relationship_pattern($12, edge, $1, -1);  // -1 = left direction
    }
    | node_pattern ARROW_LEFT LBRACKET variable RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, NULL, NULL);
        $$ = ast_create_relationship_pattern($7, edge, $1, -1);  // -1 = left direction, no type
    }
    | node_pattern ARROW_LEFT LBRACKET variable LBRACE property_list RBRACE RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern($4, NULL, $6);
        $$ = ast_create_relationship_pattern($10, edge, $1, -1);  // -1 = left direction, no type but with properties
    }
    | node_pattern ARROW_LEFT LBRACKET RBRACKET DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        $$ = ast_create_relationship_pattern($6, edge, $1, -1);  // -1 = left direction, no variable or type
    }
    | node_pattern ARROW_LEFT DASH node_pattern {
        cypher_ast_node_t *edge = ast_create_edge_pattern(NULL, NULL, NULL);
        $$ = ast_create_relationship_pattern($4, edge, $1, -1);  // -1 = left direction, no brackets
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

// WHERE clause
where_clause:
    WHERE expression {
        $$ = ast_create_where_clause($2);
    }
    ;

// Expressions
expression:
    comparison_expression { $$ = $1; }
    | expression AND expression {
        $$ = ast_create_binary_expr($1, AST_OP_AND, $3);
    }
    | expression OR expression {
        $$ = ast_create_binary_expr($1, AST_OP_OR, $3);
    }
    | NOT expression {
        $$ = ast_create_unary_expr(AST_OP_NOT, $2);
    }
    | LPAREN expression RPAREN {
        $$ = $2;  // Parentheses just return the inner expression
    }
    ;

// Comparison expressions
comparison_expression:
    property_expression EQ property_expression {
        $$ = ast_create_binary_expr($1, AST_OP_EQ, $3);
    }
    | property_expression NEQ property_expression {
        $$ = ast_create_binary_expr($1, AST_OP_NEQ, $3);
    }
    | property_expression LT property_expression {
        $$ = ast_create_binary_expr($1, AST_OP_LT, $3);
    }
    | property_expression GT property_expression {
        $$ = ast_create_binary_expr($1, AST_OP_GT, $3);
    }
    | property_expression LE property_expression {
        $$ = ast_create_binary_expr($1, AST_OP_LE, $3);
    }
    | property_expression GE property_expression {
        $$ = ast_create_binary_expr($1, AST_OP_GE, $3);
    }
    | property_expression IS NULL_TOKEN {
        $$ = ast_create_is_null_expr($1, 1);  // IS NULL
    }
    | property_expression IS NOT NULL_TOKEN {
        $$ = ast_create_is_null_expr($1, 0);  // IS NOT NULL
    }
    ;

// Property expressions
property_expression:
    IDENTIFIER {
        $$ = ast_create_identifier($1);
        free($1);
    }
    | IDENTIFIER DOT IDENTIFIER {
        $$ = ast_create_property_access($1, $3);
        free($1);
        free($3);
    }
    | literal { $$ = $1; }
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