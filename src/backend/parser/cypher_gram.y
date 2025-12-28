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
%define api.pure
%define api.prefix {cypher_yy}
%define api.token.prefix {CYPHER_}
%define parse.error detailed
%glr-parser
%parse-param {cypher_parser_context *context}
%lex-param {cypher_parser_context *context}

/*
 * Expected grammar conflicts - handled correctly by GLR parsing.
 * These arise from pattern comprehension syntax [(...)-[r]->(...) | expr]
 * where the parser can't immediately distinguish a node pattern from
 * a parenthesized expression until it sees more context.
 */
%expect 4
%expect-rr 2  /* One for IDENTIFIER, one for BQIDENT in variable_opt */

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
    cypher_merge *merge;
    cypher_set *set;
    cypher_set_item *set_item;
    cypher_delete *delete;
    cypher_delete_item *delete_item;
    cypher_remove *remove;
    cypher_remove_item *remove_item;
    cypher_with *with_clause;
    cypher_union *union_query;
    cypher_return_item *return_item;
    cypher_order_by_item *order_by_item;
    cypher_literal *literal;
    cypher_identifier *identifier;
    cypher_parameter *parameter;
    cypher_label_expr *label_expr;
    cypher_not_expr *not_expr;
    cypher_binary_op *binary_op;
    cypher_exists_expr *exists_expr;
    cypher_node_pattern *node_pattern;
    cypher_rel_pattern *rel_pattern;
    cypher_varlen_range *varlen_range;
    cypher_path *path;
    cypher_map *map;
    cypher_map_pair *map_pair;
    cypher_list_comprehension *list_comprehension;
}

/* Terminal tokens */
%token <integer> INTEGER
%token <decimal> DECIMAL
%token <string> STRING IDENTIFIER PARAMETER BQIDENT

/* Multi-character operators */
%token NOT_EQ LT_EQ GT_EQ DOT_DOT TYPECAST PLUS_EQ REGEX_MATCH

/* Keywords */
%token MATCH RETURN CREATE WHERE WITH SET DELETE REMOVE MERGE UNWIND DETACH FOREACH
%token OPTIONAL DISTINCT ORDER BY SKIP LIMIT AS ASC DESC
%token AND OR XOR NOT IN IS NULL_P TRUE_P FALSE_P EXISTS
%token ANY NONE SINGLE REDUCE
%token UNION ALL CASE WHEN THEN ELSE END_P ON
%token SHORTESTPATH ALLSHORTESTPATHS PATTERN EXPLAIN
%token LOAD CSV FROM HEADERS FIELDTERMINATOR
%token STARTS ENDS CONTAINS

/* Non-terminal types */
%type <node> stmt union_query single_query
%type <list> clause_list
%type <node> clause

%type <match> match_clause
%type <return_clause> return_clause
%type <create> create_clause
%type <merge> merge_clause
%type <set> set_clause
%type <delete> delete_clause
%type <remove> remove_clause
%type <with_clause> with_clause
%type <node> unwind_clause foreach_clause load_csv_clause list_literal list_comprehension pattern_comprehension map_literal map_projection map_projection_item case_expression when_clause
%type <list> map_projection_list foreach_update_list
%type <list> when_clause_list on_create_clause on_match_clause

%type <list> pattern_list return_item_list set_item_list delete_item_list remove_item_list
%type <set_item> set_item
%type <delete_item> delete_item
%type <remove_item> remove_item
%type <boolean> detach_opt
%type <path> path simple_path
%type <node_pattern> node_pattern
%type <rel_pattern> rel_pattern
%type <varlen_range> varlen_range_opt
%type <return_item> return_item

%type <node> expr primary_expr literal_expr function_call list_predicate reduce_expr
%type <list> argument_list
%type <literal> literal
%type <identifier> identifier
%type <parameter> parameter
%type <map> properties_opt
%type <map_pair> map_pair
%type <list> map_pair_list

%type <list> label_opt label_list
%type <string> variable_opt
%type <boolean> optional_opt distinct_opt
%type <list> order_by_opt order_by_list rel_type_list
%type <node> skip_opt limit_opt where_opt
%type <order_by_item> order_by_item

/* Operator precedence (lowest to highest) */
%left OR
%left XOR
%left AND
%right NOT
%left '=' NOT_EQ '<' LT_EQ '>' GT_EQ REGEX_MATCH STARTS ENDS CONTAINS
%left '+' '-'
%left '*' '/' '%'
%left '^'
%left IN IS
%left '.'
%right UNARY_MINUS UNARY_PLUS

%%

/* Main grammar rules */

stmt:
    union_query
        {
            $$ = $1;
            context->result = $1;
        }
    | EXPLAIN union_query
        {
            /* For EXPLAIN with UNION, wrap if needed */
            if ($2->type == AST_NODE_QUERY) {
                ((cypher_query*)$2)->explain = true;
            }
            $$ = $2;
            context->result = $2;
        }
    ;

union_query:
    single_query
        {
            $$ = $1;
        }
    | union_query UNION single_query
        {
            $$ = (ast_node*)make_cypher_union($1, $3, false, @2.first_line);
        }
    | union_query UNION ALL single_query
        {
            $$ = (ast_node*)make_cypher_union($1, $4, true, @2.first_line);
        }
    ;

single_query:
    clause_list
        {
            $$ = (ast_node*)make_cypher_query($1, false);
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
    | with_clause       { $$ = (ast_node*)$1; }
    | unwind_clause     { $$ = $1; }
    | foreach_clause    { $$ = $1; }
    | load_csv_clause   { $$ = $1; }
    | create_clause     { $$ = (ast_node*)$1; }
    | merge_clause      { $$ = (ast_node*)$1; }
    | set_clause        { $$ = (ast_node*)$1; }
    | delete_clause     { $$ = (ast_node*)$1; }
    | remove_clause     { $$ = (ast_node*)$1; }
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

/* WITH clause - projection with optional WHERE for filtering */
with_clause:
    WITH distinct_opt return_item_list order_by_opt skip_opt limit_opt where_opt
        {
            $$ = make_cypher_with($2, $3, $4, $5, $6, $7);
        }
    ;

/* UNWIND clause - expands list into rows */
unwind_clause:
    UNWIND expr AS IDENTIFIER
        {
            $$ = (ast_node*)make_cypher_unwind($2, $4, @1.first_line);
            free($4);
        }
    ;

/* FOREACH clause - iterate over list and apply update clauses */
foreach_clause:
    FOREACH '(' IDENTIFIER IN expr '|' foreach_update_list ')'
        {
            $$ = (ast_node*)make_cypher_foreach($3, $5, $7, @1.first_line);
            free($3);
        }
    ;

/* LOAD CSV clause - import data from CSV files */
load_csv_clause:
    LOAD CSV FROM STRING AS IDENTIFIER
        {
            $$ = (ast_node*)make_cypher_load_csv($4, $6, false, NULL, @1.first_line);
            free($4);
            free($6);
        }
    | LOAD CSV WITH HEADERS FROM STRING AS IDENTIFIER
        {
            $$ = (ast_node*)make_cypher_load_csv($6, $8, true, NULL, @1.first_line);
            free($6);
            free($8);
        }
    | LOAD CSV FROM STRING AS IDENTIFIER FIELDTERMINATOR STRING
        {
            $$ = (ast_node*)make_cypher_load_csv($4, $6, false, $8, @1.first_line);
            free($4);
            free($6);
            free($8);
        }
    | LOAD CSV WITH HEADERS FROM STRING AS IDENTIFIER FIELDTERMINATOR STRING
        {
            $$ = (ast_node*)make_cypher_load_csv($6, $8, true, $10, @1.first_line);
            free($6);
            free($8);
            free($10);
        }
    ;

/* Update clauses allowed inside FOREACH: CREATE, SET, DELETE, MERGE, REMOVE, FOREACH */
foreach_update_list:
    create_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | set_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | delete_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | merge_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | remove_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | foreach_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, $1);
        }
    | foreach_update_list create_clause
        {
            ast_list_append($1, (ast_node*)$2);
            $$ = $1;
        }
    | foreach_update_list set_clause
        {
            ast_list_append($1, (ast_node*)$2);
            $$ = $1;
        }
    | foreach_update_list delete_clause
        {
            ast_list_append($1, (ast_node*)$2);
            $$ = $1;
        }
    | foreach_update_list merge_clause
        {
            ast_list_append($1, (ast_node*)$2);
            $$ = $1;
        }
    | foreach_update_list remove_clause
        {
            ast_list_append($1, (ast_node*)$2);
            $$ = $1;
        }
    | foreach_update_list foreach_clause
        {
            ast_list_append($1, $2);
            $$ = $1;
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

/* MERGE clause */
merge_clause:
    MERGE pattern_list
        {
            $$ = make_cypher_merge($2, NULL, NULL);
        }
    | MERGE pattern_list on_create_clause
        {
            $$ = make_cypher_merge($2, $3, NULL);
        }
    | MERGE pattern_list on_match_clause
        {
            $$ = make_cypher_merge($2, NULL, $3);
        }
    | MERGE pattern_list on_create_clause on_match_clause
        {
            $$ = make_cypher_merge($2, $3, $4);
        }
    | MERGE pattern_list on_match_clause on_create_clause
        {
            $$ = make_cypher_merge($2, $4, $3);
        }
    ;

on_create_clause:
    ON CREATE SET set_item_list
        {
            $$ = $4;
        }
    ;

on_match_clause:
    ON MATCH SET set_item_list
        {
            $$ = $4;
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

/* REMOVE clause */
remove_clause:
    REMOVE remove_item_list
        {
            $$ = make_cypher_remove($2);
        }
    ;

remove_item_list:
    remove_item
        {
            $$ = ast_list_create();
            ast_list_append($$, (ast_node*)$1);
        }
    | remove_item_list ',' remove_item
        {
            ast_list_append($1, (ast_node*)$3);
            $$ = $1;
        }
    ;

remove_item:
    IDENTIFIER '.' IDENTIFIER
        {
            /* REMOVE n.property - property access */
            cypher_identifier *base = make_identifier($1, @1.first_line);
            ast_node *prop = (ast_node*)make_property((ast_node*)base, $3, @3.first_line);
            $$ = make_remove_item(prop);
            free($1);
            free($3);
        }
    | IDENTIFIER ':' IDENTIFIER
        {
            /* REMOVE n:Label syntax */
            cypher_identifier *var = make_identifier($1, @1.first_line);
            cypher_label_expr *label = make_label_expr((ast_node*)var, $3, @3.first_line);
            $$ = make_remove_item((ast_node*)label);
            free($1);
            free($3);
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

/* Simple path - base path without shortestPath wrappers */
simple_path:
    node_pattern
        {
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)$1);
            $$ = make_path(elements);
        }
    | simple_path rel_pattern node_pattern
        {
            /* Create a new list copying elements from the existing path */
            ast_list *new_elements = ast_list_create();
            for (int i = 0; i < $1->elements->count; i++) {
                ast_list_append(new_elements, $1->elements->items[i]);
            }
            ast_list_append(new_elements, (ast_node*)$2);
            ast_list_append(new_elements, (ast_node*)$3);

            /* Note: Let Bison handle memory cleanup of $1 automatically */
            /* Manual freeing during parsing can cause parser state corruption */

            $$ = make_path(new_elements);
        }
    ;

/* Full path - includes variable assignment and shortestPath wrappers */
path:
    simple_path
        {
            $$ = $1;
        }
    | IDENTIFIER '=' simple_path
        {
            $$ = make_path_with_var($1, $3->elements);
            /* Free the anonymous path structure, but keep its elements */
            free($3);
        }
    | IDENTIFIER '=' SHORTESTPATH '(' simple_path ')'
        {
            cypher_path *sp = make_shortest_path($5->elements, PATH_TYPE_SHORTEST);
            sp->var_name = $1;
            $$ = sp;
            free($5);
        }
    | SHORTESTPATH '(' simple_path ')'
        {
            $$ = make_shortest_path($3->elements, PATH_TYPE_SHORTEST);
            free($3);
        }
    | IDENTIFIER '=' ALLSHORTESTPATHS '(' simple_path ')'
        {
            cypher_path *sp = make_shortest_path($5->elements, PATH_TYPE_ALL_SHORTEST);
            sp->var_name = $1;
            $$ = sp;
            free($5);
        }
    | ALLSHORTESTPATHS '(' simple_path ')'
        {
            $$ = make_shortest_path($3->elements, PATH_TYPE_ALL_SHORTEST);
            free($3);
        }
    ;

node_pattern:
    '(' variable_opt label_opt properties_opt ')'
        {
            $$ = make_node_pattern($2, $3, (ast_node*)$4);
        }
    ;

/* Relationship patterns - supports fixed and variable-length relationships
 * Each direction (outgoing ->, incoming <-, undirected -) has variants for:
 * - No type: -[var *1..5]->, -[*]->
 * - Single type: -[:TYPE*1..5]->
 * - Multiple types: -[:TYPE1|TYPE2]->
 * Variable-length syntax: *, *N, *N..M, *N.., *..M
 */
rel_pattern:
    /* Outgoing relationships: -[...]-> */
    '-' '[' variable_opt varlen_range_opt properties_opt ']' '-' '>'
        {
            $$ = make_rel_pattern_varlen($3, NULL, (ast_node*)$5, false, true, (ast_node*)$4);
        }
    | '-' '[' variable_opt ':' IDENTIFIER varlen_range_opt properties_opt ']' '-' '>'
        {
            $$ = make_rel_pattern_varlen($3, $5, (ast_node*)$7, false, true, (ast_node*)$6);
            free($5);
        }
    | '-' '[' variable_opt ':' BQIDENT varlen_range_opt properties_opt ']' '-' '>'
        {
            $$ = make_rel_pattern_varlen($3, $5, (ast_node*)$7, false, true, (ast_node*)$6);
            free($5);
        }
    | '-' '[' variable_opt ':' rel_type_list varlen_range_opt properties_opt ']' '-' '>'
        {
            cypher_rel_pattern *p = make_rel_pattern_multi_type($3, $5, (ast_node*)$7, false, true);
            if (p) p->varlen = (ast_node*)$6;
            $$ = p;
        }
    /* Incoming relationships: <-[...]- */
    | '<' '-' '[' variable_opt varlen_range_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern_varlen($4, NULL, (ast_node*)$6, true, false, (ast_node*)$5);
        }
    | '<' '-' '[' variable_opt ':' IDENTIFIER varlen_range_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern_varlen($4, $6, (ast_node*)$8, true, false, (ast_node*)$7);
            free($6);
        }
    | '<' '-' '[' variable_opt ':' BQIDENT varlen_range_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern_varlen($4, $6, (ast_node*)$8, true, false, (ast_node*)$7);
            free($6);
        }
    | '<' '-' '[' variable_opt ':' rel_type_list varlen_range_opt properties_opt ']' '-'
        {
            cypher_rel_pattern *p = make_rel_pattern_multi_type($4, $6, (ast_node*)$8, true, false);
            if (p) p->varlen = (ast_node*)$7;
            $$ = p;
        }
    /* Undirected relationships: -[...]- */
    | '-' '[' variable_opt varlen_range_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern_varlen($3, NULL, (ast_node*)$5, false, false, (ast_node*)$4);
        }
    | '-' '[' variable_opt ':' IDENTIFIER varlen_range_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern_varlen($3, $5, (ast_node*)$7, false, false, (ast_node*)$6);
            free($5);
        }
    | '-' '[' variable_opt ':' BQIDENT varlen_range_opt properties_opt ']' '-'
        {
            $$ = make_rel_pattern_varlen($3, $5, (ast_node*)$7, false, false, (ast_node*)$6);
            free($5);
        }
    | '-' '[' variable_opt ':' rel_type_list varlen_range_opt properties_opt ']' '-'
        {
            cypher_rel_pattern *p = make_rel_pattern_multi_type($3, $5, (ast_node*)$7, false, false);
            if (p) p->varlen = (ast_node*)$6;
            $$ = p;
        }
    ;

variable_opt:
    /* empty */     { $$ = NULL; }
    | IDENTIFIER    { $$ = $1; }
    | BQIDENT       { $$ = $1; }
    ;

/* Variable-length range for relationships: *, *1..5, *2.., *..3 */
varlen_range_opt:
    /* empty - not variable length */
        { $$ = NULL; }
    | '*'
        { $$ = make_varlen_range(1, -1); }  /* unbounded: 1 to infinity */
    | '*' INTEGER
        { $$ = make_varlen_range($2, $2); }  /* exact: *3 means exactly 3 hops */
    | '*' INTEGER DOT_DOT INTEGER
        { $$ = make_varlen_range($2, $4); }  /* bounded: *1..5 */
    | '*' INTEGER DOT_DOT
        { $$ = make_varlen_range($2, -1); }  /* min bounded: *2.. */
    | '*' DOT_DOT INTEGER
        { $$ = make_varlen_range(1, $3); }   /* max bounded: *..3 */
    ;

label_opt:
    /* empty */         { $$ = NULL; }
    | label_list        { $$ = $1; }
    ;

/* Support for multiple labels: :Label1:Label2:Label3 or :`Quoted Label` */
label_list:
    ':' IDENTIFIER
        {
            $$ = ast_list_create();
            cypher_literal *label = make_string_literal($2, @2.first_line);
            ast_list_append($$, (ast_node*)label);
            free($2);
        }
    | ':' BQIDENT
        {
            $$ = ast_list_create();
            cypher_literal *label = make_string_literal($2, @2.first_line);
            ast_list_append($$, (ast_node*)label);
            free($2);
        }
    | label_list ':' IDENTIFIER
        {
            $$ = $1;
            cypher_literal *label = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)label);
            free($3);
        }
    | label_list ':' BQIDENT
        {
            $$ = $1;
            cypher_literal *label = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)label);
            free($3);
        }
    ;

rel_type_list:
    IDENTIFIER '|' IDENTIFIER
        {
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit3 = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit3);
            free($1);
            free($3);
        }
    | IDENTIFIER '|' ':' IDENTIFIER
        {
            /* Support [:TYPE1|:TYPE2] syntax with colon before second type */
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit4 = make_string_literal($4, @4.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit4);
            free($1);
            free($4);
        }
    | IDENTIFIER '|' BQIDENT
        {
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit3 = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit3);
            free($1);
            free($3);
        }
    | IDENTIFIER '|' ':' BQIDENT
        {
            /* Support [:TYPE1|:`backtick-type`] syntax */
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit4 = make_string_literal($4, @4.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit4);
            free($1);
            free($4);
        }
    | BQIDENT '|' IDENTIFIER
        {
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit3 = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit3);
            free($1);
            free($3);
        }
    | BQIDENT '|' ':' IDENTIFIER
        {
            /* Support [:`backtick-type`|:TYPE2] syntax */
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit4 = make_string_literal($4, @4.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit4);
            free($1);
            free($4);
        }
    | BQIDENT '|' BQIDENT
        {
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit3 = make_string_literal($3, @3.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit3);
            free($1);
            free($3);
        }
    | BQIDENT '|' ':' BQIDENT
        {
            /* Support [:`backtick-type`|:`backtick-type2`] syntax */
            $$ = ast_list_create();
            cypher_literal *type_lit1 = make_string_literal($1, @1.first_line);
            cypher_literal *type_lit4 = make_string_literal($4, @4.first_line);
            ast_list_append($$, (ast_node*)type_lit1);
            ast_list_append($$, (ast_node*)type_lit4);
            free($1);
            free($4);
        }
    | rel_type_list '|' IDENTIFIER
        {
            cypher_literal *type_lit = make_string_literal($3, @3.first_line);
            ast_list_append($1, (ast_node*)type_lit);
            $$ = $1;
            free($3);
        }
    | rel_type_list '|' BQIDENT
        {
            cypher_literal *type_lit = make_string_literal($3, @3.first_line);
            ast_list_append($1, (ast_node*)type_lit);
            $$ = $1;
            free($3);
        }
    | rel_type_list '|' ':' IDENTIFIER
        {
            cypher_literal *type_lit = make_string_literal($4, @4.first_line);
            ast_list_append($1, (ast_node*)type_lit);
            $$ = $1;
            free($4);
        }
    | rel_type_list '|' ':' BQIDENT
        {
            cypher_literal *type_lit = make_string_literal($4, @4.first_line);
            ast_list_append($1, (ast_node*)type_lit);
            $$ = $1;
            free($4);
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
    | expr '%' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_MOD, $1, $3, @2.first_line); }
    | expr '=' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_EQ, $1, $3, @2.first_line); }
    | expr NOT_EQ expr  { $$ = (ast_node*)make_binary_op(BINARY_OP_NEQ, $1, $3, @2.first_line); }
    | expr '<' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_LT, $1, $3, @2.first_line); }
    | expr '>' expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_GT, $1, $3, @2.first_line); }
    | expr LT_EQ expr   { $$ = (ast_node*)make_binary_op(BINARY_OP_LTE, $1, $3, @2.first_line); }
    | expr GT_EQ expr   { $$ = (ast_node*)make_binary_op(BINARY_OP_GTE, $1, $3, @2.first_line); }
    | expr REGEX_MATCH expr { $$ = (ast_node*)make_binary_op(BINARY_OP_REGEX_MATCH, $1, $3, @2.first_line); }
    | expr AND expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_AND, $1, $3, @2.first_line); }
    | expr OR expr      { $$ = (ast_node*)make_binary_op(BINARY_OP_OR, $1, $3, @2.first_line); }
    | expr XOR expr     { $$ = (ast_node*)make_binary_op(BINARY_OP_XOR, $1, $3, @2.first_line); }
    | expr IN expr      { $$ = (ast_node*)make_binary_op(BINARY_OP_IN, $1, $3, @2.first_line); }
    | expr STARTS WITH expr %prec STARTS { $$ = (ast_node*)make_binary_op(BINARY_OP_STARTS_WITH, $1, $4, @2.first_line); }
    | expr ENDS WITH expr %prec ENDS    { $$ = (ast_node*)make_binary_op(BINARY_OP_ENDS_WITH, $1, $4, @2.first_line); }
    | expr CONTAINS expr %prec CONTAINS { $$ = (ast_node*)make_binary_op(BINARY_OP_CONTAINS, $1, $3, @2.first_line); }
    | NOT expr          { $$ = (ast_node*)make_not_expr($2, @1.first_line); }
    | expr IS NULL_P      { $$ = (ast_node*)make_null_check($1, false, @2.first_line); }
    | expr IS NOT NULL_P  { $$ = (ast_node*)make_null_check($1, true, @2.first_line); }
    | '(' expr ')'      { $$ = $2; }
    ;

primary_expr:
    literal_expr        { $$ = $1; }
    | identifier        { $$ = (ast_node*)$1; }
    | parameter         { $$ = (ast_node*)$1; }
    | function_call     { $$ = $1; }
    | list_predicate    { $$ = $1; }
    | reduce_expr       { $$ = $1; }
    | list_literal      { $$ = $1; } %dprec 1
    | list_comprehension { $$ = $1; } %dprec 2
    | pattern_comprehension { $$ = $1; }
    | map_literal       { $$ = $1; }
    | map_projection    { $$ = $1; }
    | case_expression   { $$ = $1; }
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
    | IDENTIFIER '[' expr ']'
        {
            /* Array subscript on identifier: items[idx] */
            cypher_identifier *base = make_identifier($1, @1.first_line);
            $$ = (ast_node*)make_subscript((ast_node*)base, $3, @2.first_line);
            free($1);
        }
    | '(' expr ')' '[' expr ']'
        {
            /* Array subscript on parenthesized expression: (expr)[idx] */
            $$ = (ast_node*)make_subscript($2, $5, @4.first_line);
        }
    | list_literal '[' expr ']'
        {
            /* Array subscript on list literal: [1,2,3][0] */
            $$ = (ast_node*)make_subscript($1, $3, @2.first_line);
        }
    ;

literal_expr:
    literal             { $$ = (ast_node*)$1; }
    ;

function_call:
    IDENTIFIER '(' ')'
        {
            /* Check if this is EXISTS function call */
            if (strcasecmp($1, "exists") == 0) {
                /* Empty EXISTS() - invalid */
                free($1);
                cypher_yyerror(&@1, context, "EXISTS requires an argument");
                YYERROR;
            } else {
                ast_list *args = ast_list_create();
                $$ = (ast_node*)make_function_call($1, args, false, @1.first_line);
                free($1);
            }
        }
    /* Single and multiple argument function calls using argument_list */
    | IDENTIFIER '(' argument_list ')'
        {
            /* Check if this is EXISTS function call */
            if (strcasecmp($1, "exists") == 0) {
                /* EXISTS with argument list - check first arg */
                if ($3->count == 1 && $3->items[0] != NULL) {
                    ast_node *arg = $3->items[0];
                    if (arg->type == AST_NODE_PROPERTY) {
                        $$ = (ast_node*)make_exists_property_expr(arg, @1.first_line);
                        $3->items[0] = NULL;  /* Transfer ownership */
                        ast_list_free($3);
                    } else {
                        $$ = (ast_node*)make_exists_pattern_expr($3, @1.first_line);
                    }
                } else {
                    cypher_yyerror(&@1, context, "EXISTS requires exactly one argument");
                    ast_list_free($3);
                    free($1);
                    YYERROR;
                }
                free($1);
            } else {
                $$ = (ast_node*)make_function_call($1, $3, false, @1.first_line);
                free($1);
            }
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
    | EXISTS '(' pattern_list ')'
        {
            /* EXISTS((pattern)) - check for relationship/path existence */
            $$ = (ast_node*)make_exists_pattern_expr($3, @1.first_line);
        }
    | EXISTS '(' IDENTIFIER '.' IDENTIFIER ')'
        {
            /* EXISTS(n.property) - unambiguous property existence check */
            ast_node *var = (ast_node*)make_identifier($3, @3.first_line);
            ast_node *prop = (ast_node*)make_property(var, $5, @1.first_line);
            $$ = (ast_node*)make_exists_property_expr(prop, @1.first_line);
            free($3);
            free($5);
        }
    /* Allow keyword-named functions: contains(), startsWith(), endsWith() */
    | CONTAINS '(' argument_list ')'
        {
            $$ = (ast_node*)make_function_call(strdup("contains"), $3, false, @1.first_line);
        }
    | STARTS '(' argument_list ')'
        {
            /* startsWith function uses STARTS keyword */
            $$ = (ast_node*)make_function_call(strdup("startsWith"), $3, false, @1.first_line);
        }
    | ENDS '(' argument_list ')'
        {
            /* endsWith function uses ENDS keyword */
            $$ = (ast_node*)make_function_call(strdup("endsWith"), $3, false, @1.first_line);
        }
    ;

/* List predicates: all(), any(), none(), single() */
list_predicate:
    ALL '(' IDENTIFIER IN expr WHERE expr ')'
        {
            $$ = (ast_node*)make_list_predicate(LIST_PRED_ALL, $3, $5, $7, @1.first_line);
            free($3);
        }
    | ANY '(' IDENTIFIER IN expr WHERE expr ')'
        {
            $$ = (ast_node*)make_list_predicate(LIST_PRED_ANY, $3, $5, $7, @1.first_line);
            free($3);
        }
    | NONE '(' IDENTIFIER IN expr WHERE expr ')'
        {
            $$ = (ast_node*)make_list_predicate(LIST_PRED_NONE, $3, $5, $7, @1.first_line);
            free($3);
        }
    | SINGLE '(' IDENTIFIER IN expr WHERE expr ')'
        {
            $$ = (ast_node*)make_list_predicate(LIST_PRED_SINGLE, $3, $5, $7, @1.first_line);
            free($3);
        }
    ;

/* Reduce expression: reduce(acc = initial, x IN list | expression) */
reduce_expr:
    REDUCE '(' IDENTIFIER '=' expr ',' IDENTIFIER IN expr '|' expr ')'
        {
            /* reduce(acc = initial, x IN list | expression) */
            $$ = (ast_node*)make_reduce_expr($3, $5, $7, $9, $11, @1.first_line);
            free($3);
            free($7);
        }
    ;

/* Argument list for function calls: expr, expr, ... */
argument_list:
    expr
        {
            $$ = ast_list_create();
            ast_list_append($$, $1);
        }
    | argument_list ',' expr
        {
            ast_list_append($1, $3);
            $$ = $1;
        }
    ;

/* List literal: [item1, item2, ...] */
list_literal:
    '[' ']'
        {
            $$ = (ast_node*)make_list(ast_list_create(), @1.first_line);
        }
    | '[' return_item_list ']'
        {
            /* Reuse return_item_list for comma-separated expressions */
            /* But we need to extract the expressions from return_items */
            ast_list *exprs = ast_list_create();
            for (int i = 0; i < $2->count; i++) {
                cypher_return_item *item = (cypher_return_item*)$2->items[i];
                ast_list_append(exprs, item->expr);
                item->expr = NULL;  /* Prevent double-free */
            }
            ast_list_free($2);
            $$ = (ast_node*)make_list(exprs, @1.first_line);
        }
    ;

/* List comprehension: [x IN list], [x IN list WHERE cond], [x IN list | expr], [x IN list WHERE cond | expr] */
list_comprehension:
    '[' IDENTIFIER IN expr ']'
        {
            $$ = (ast_node*)make_list_comprehension($2, $4, NULL, NULL, @1.first_line);
            free($2);
        }
    | '[' IDENTIFIER IN expr WHERE expr ']'
        {
            $$ = (ast_node*)make_list_comprehension($2, $4, $6, NULL, @1.first_line);
            free($2);
        }
    | '[' IDENTIFIER IN expr '|' expr ']'
        {
            $$ = (ast_node*)make_list_comprehension($2, $4, NULL, $6, @1.first_line);
            free($2);
        }
    | '[' IDENTIFIER IN expr WHERE expr '|' expr ']'
        {
            $$ = (ast_node*)make_list_comprehension($2, $4, $6, $8, @1.first_line);
            free($2);
        }
    ;

/* Pattern comprehension: [(n)-[r]->(m) | expr]
 *
 * Standard OpenCypher syntax. GLR parser handles the ambiguity with list literals
 * by exploring both parse paths - the wrong one fails and is discarded.
 *
 * Examples:
 *   [(a)-[r]->(b) | b.name]
 *   [(a:Person)-[:KNOWS]->(b) WHERE b.age > 21 | b.name]
 */
pattern_comprehension:
    '[' '(' variable_opt label_opt properties_opt ')' rel_pattern node_pattern '|' expr ']'
        {
            cypher_node_pattern *first = make_node_pattern($3, $4, (ast_node*)$5);
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)first);
            ast_list_append(elements, (ast_node*)$7);
            ast_list_append(elements, (ast_node*)$8);
            cypher_path *path = make_path(elements);

            ast_list *pattern = ast_list_create();
            ast_list_append(pattern, (ast_node*)path);
            $$ = (ast_node*)make_pattern_comprehension(pattern, NULL, $10, @1.first_line);
        }
    | '[' '(' variable_opt label_opt properties_opt ')' rel_pattern node_pattern WHERE expr '|' expr ']'
        {
            cypher_node_pattern *first = make_node_pattern($3, $4, (ast_node*)$5);
            ast_list *elements = ast_list_create();
            ast_list_append(elements, (ast_node*)first);
            ast_list_append(elements, (ast_node*)$7);
            ast_list_append(elements, (ast_node*)$8);
            cypher_path *path = make_path(elements);

            ast_list *pattern = ast_list_create();
            ast_list_append(pattern, (ast_node*)path);
            $$ = (ast_node*)make_pattern_comprehension(pattern, $10, $12, @1.first_line);
        }
    ;

/* Map literal: {key: value, ...} */
map_literal:
    '{' '}'
        {
            $$ = (ast_node*)make_map(ast_list_create(), @1.first_line);
        }
    | '{' map_pair_list '}'
        {
            $$ = (ast_node*)make_map($2, @1.first_line);
        }
    ;

/* Map projection: n{.prop1, .prop2} or n{alias: .prop, ...} */
map_projection:
    IDENTIFIER '{' map_projection_list '}'
        {
            cypher_identifier *base = make_identifier($1, @1.first_line);
            $$ = (ast_node*)make_map_projection((ast_node*)base, $3, @1.first_line);
            free($1);
        }
    ;

map_projection_list:
    map_projection_item
        {
            $$ = ast_list_create();
            ast_list_append($$, $1);
        }
    | map_projection_list ',' map_projection_item
        {
            ast_list_append($1, $3);
            $$ = $1;
        }
    ;

map_projection_item:
    '.' IDENTIFIER
        {
            /* Shorthand: .prop -> key=prop, property=prop, expr=NULL */
            $$ = (ast_node*)make_map_projection_item($2, $2, NULL, @1.first_line);
            free($2);
        }
    | '.' '*'
        {
            /* All properties: .* -> key=NULL, property="*", expr=NULL */
            $$ = (ast_node*)make_map_projection_item(NULL, strdup("*"), NULL, @1.first_line);
        }
    | IDENTIFIER ':' '.' IDENTIFIER
        {
            /* Aliased property: alias: .prop */
            $$ = (ast_node*)make_map_projection_item($1, $4, NULL, @1.first_line);
            free($1);
            free($4);
        }
    | IDENTIFIER ':' expr
        {
            /* Computed value: alias: expr */
            $$ = (ast_node*)make_map_projection_item($1, NULL, $3, @1.first_line);
            free($1);
        }
    ;

/* CASE expression: CASE WHEN cond THEN val [WHEN cond THEN val ...] [ELSE val] END */
case_expression:
    CASE when_clause_list END_P
        {
            $$ = (ast_node*)make_case_expr($2, NULL, @1.first_line);
        }
    | CASE when_clause_list ELSE expr END_P
        {
            $$ = (ast_node*)make_case_expr($2, $4, @1.first_line);
        }
    ;

when_clause_list:
    when_clause
        {
            $$ = ast_list_create();
            ast_list_append($$, $1);
        }
    | when_clause_list when_clause
        {
            ast_list_append($1, $2);
            $$ = $1;
        }
    ;

when_clause:
    WHEN expr THEN expr
        {
            $$ = (ast_node*)make_when_clause($2, $4, @1.first_line);
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
    | TRUE_P
        {
            $$ = make_boolean_literal(true, @1.first_line);
        }
    | FALSE_P
        {
            $$ = make_boolean_literal(false, @1.first_line);
        }
    | NULL_P
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
    | BQIDENT ':' expr
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

    /* Create error message with line number - Bison's detailed error mode
     * provides good context about what was expected */
    char error_buffer[512];
    if (yylloc && yylloc->first_line > 0) {
        snprintf(error_buffer, sizeof(error_buffer),
                 "Line %d: %s", yylloc->first_line, msg);
    } else {
        snprintf(error_buffer, sizeof(error_buffer), "%s", msg);
    }

    free(context->error_message);
    context->error_message = strdup(error_buffer);
}

/* cypher_yylex function is implemented in cypher_parser.c */