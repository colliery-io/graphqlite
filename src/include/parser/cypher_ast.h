#ifndef CYPHER_AST_H
#define CYPHER_AST_H

#include <stdbool.h>

/* AST node types */
typedef enum ast_node_type {
    AST_NODE_UNKNOWN = 0,
    
    /* Query structure */
    AST_NODE_QUERY,
    AST_NODE_SINGLE_QUERY,
    AST_NODE_UNION,

    /* Clauses */
    AST_NODE_MATCH,
    AST_NODE_RETURN,
    AST_NODE_CREATE,
    AST_NODE_WHERE,
    AST_NODE_WITH,
    AST_NODE_SET,
    AST_NODE_SET_ITEM,
    AST_NODE_DELETE,
    AST_NODE_DELETE_ITEM,
    AST_NODE_REMOVE,
    AST_NODE_REMOVE_ITEM,
    AST_NODE_MERGE,
    AST_NODE_UNWIND,
    AST_NODE_FOREACH,
    AST_NODE_LOAD_CSV,

    /* Patterns */
    AST_NODE_PATTERN,
    AST_NODE_PATH,
    AST_NODE_NODE_PATTERN,
    AST_NODE_REL_PATTERN,
    AST_NODE_VARLEN_RANGE,

    /* Expressions */
    AST_NODE_EXPR,
    AST_NODE_LITERAL,
    AST_NODE_IDENTIFIER,
    AST_NODE_PARAMETER,
    AST_NODE_PROPERTY,
    AST_NODE_LABEL_EXPR,
    AST_NODE_NOT_EXPR,
    AST_NODE_NULL_CHECK,
    AST_NODE_BINARY_OP,
    AST_NODE_FUNCTION_CALL,
    AST_NODE_EXISTS_EXPR,
    AST_NODE_LIST,
    AST_NODE_LIST_COMPREHENSION,
    AST_NODE_PATTERN_COMPREHENSION,
    AST_NODE_MAP,
    AST_NODE_MAP_PAIR,
    AST_NODE_MAP_PROJECTION,
    AST_NODE_MAP_PROJECTION_ITEM,
    AST_NODE_CASE_EXPR,
    AST_NODE_WHEN_CLAUSE,
    AST_NODE_LIST_PREDICATE,
    AST_NODE_REDUCE_EXPR,

    /* Return items */
    AST_NODE_RETURN_ITEM,
    AST_NODE_ORDER_BY,
    AST_NODE_SKIP,
    AST_NODE_LIMIT
} ast_node_type;

/* Binary operator types */
typedef enum {
    BINARY_OP_AND,
    BINARY_OP_OR,
    BINARY_OP_XOR,
    BINARY_OP_EQ,
    BINARY_OP_NEQ,
    BINARY_OP_LT,
    BINARY_OP_GT,
    BINARY_OP_LTE,
    BINARY_OP_GTE,
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_MUL,
    BINARY_OP_DIV,
    BINARY_OP_MOD,
    BINARY_OP_REGEX_MATCH,
    BINARY_OP_IN
} binary_op_type;

/* EXISTS expression types */
typedef enum {
    EXISTS_TYPE_PATTERN,   /* EXISTS((pattern)) */
    EXISTS_TYPE_PROPERTY   /* EXISTS(property) */
} exists_expr_type;

/* List predicate types: all(), any(), none(), single() */
typedef enum {
    LIST_PRED_ALL,     /* all(x IN list WHERE predicate) */
    LIST_PRED_ANY,     /* any(x IN list WHERE predicate) */
    LIST_PRED_NONE,    /* none(x IN list WHERE predicate) */
    LIST_PRED_SINGLE   /* single(x IN list WHERE predicate) */
} list_predicate_type;

/* Base AST node structure */
typedef struct ast_node {
    ast_node_type type;
    void *data;
    int location;  /* Character location in original query for error reporting */
} ast_node;

/* Generic list structure for AST nodes */
typedef struct ast_list {
    ast_node **items;
    int count;
    int capacity;
} ast_list;

/* Cypher query structure */
typedef struct cypher_query {
    ast_node base;
    ast_list *clauses;  /* List of clauses */
    bool explain;       /* EXPLAIN prefix - return SQL instead of executing */
} cypher_query;

/* UNION query - combines multiple queries */
typedef struct cypher_union {
    ast_node base;
    ast_node *left;     /* Left query (cypher_query or cypher_union) */
    ast_node *right;    /* Right query (cypher_query) */
    bool all;           /* UNION ALL (keeps duplicates) vs UNION (removes duplicates) */
} cypher_union;

/* MATCH clause */
typedef struct cypher_match {
    ast_node base;
    ast_list *pattern;    /* List of path patterns */
    ast_node *where;      /* Optional WHERE expression */
    bool optional;        /* OPTIONAL MATCH */
} cypher_match;

/* RETURN clause */
typedef struct cypher_return {
    ast_node base;
    bool distinct;        /* RETURN DISTINCT */
    ast_list *items;      /* List of return items */
    ast_list *order_by;   /* Optional ORDER BY */
    ast_node *skip;       /* Optional SKIP */
    ast_node *limit;      /* Optional LIMIT */
} cypher_return;

/* WITH clause - similar to RETURN but creates new variable scope */
typedef struct cypher_with {
    ast_node base;
    bool distinct;        /* WITH DISTINCT */
    ast_list *items;      /* List of projection items */
    ast_list *order_by;   /* Optional ORDER BY */
    ast_node *skip;       /* Optional SKIP */
    ast_node *limit;      /* Optional LIMIT */
    ast_node *where;      /* Optional WHERE (applied after projection) */
} cypher_with;

/* UNWIND clause - expands list into rows */
typedef struct cypher_unwind {
    ast_node base;
    ast_node *expr;       /* Expression producing the list (list literal, property, etc.) */
    char *alias;          /* Variable name for each element (AS alias) */
} cypher_unwind;

/* FOREACH clause - iterate and update */
typedef struct cypher_foreach {
    ast_node base;
    char *variable;       /* Iteration variable name */
    ast_node *list_expr;  /* List expression to iterate over */
    ast_list *body;       /* List of update clauses (CREATE, SET, DELETE, MERGE, REMOVE, FOREACH) */
} cypher_foreach;

/* LOAD CSV clause - import data from CSV files */
typedef struct cypher_load_csv {
    ast_node base;
    char *file_path;      /* Path to CSV file */
    char *variable;       /* Row variable name (AS variable) */
    bool with_headers;    /* WITH HEADERS flag */
    char *fieldterminator; /* Optional field terminator (default comma) */
} cypher_load_csv;

/* CREATE clause */
typedef struct cypher_create {
    ast_node base;
    ast_list *pattern;    /* List of patterns to create */
} cypher_create;

/* MERGE clause */
typedef struct cypher_merge {
    ast_node base;
    ast_list *pattern;        /* Pattern to merge (single path) */
    ast_list *on_create;      /* ON CREATE SET items (can be NULL) */
    ast_list *on_match;       /* ON MATCH SET items (can be NULL) */
} cypher_merge;

/* SET clause */
typedef struct cypher_set {
    ast_node base;
    ast_list *items;      /* List of set items */
} cypher_set;

/* SET item (e.g., n.prop = value) */
typedef struct cypher_set_item {
    ast_node base;
    ast_node *property;   /* Property to set (n.prop) */
    ast_node *expr;       /* Expression to set it to */
} cypher_set_item;

/* DELETE clause */
typedef struct cypher_delete {
    ast_node base;
    ast_list *items;      /* List of delete items (variables to delete) */
    bool detach;          /* TRUE for DETACH DELETE, FALSE for regular DELETE */
} cypher_delete;

/* DELETE item (e.g., n or r) */
typedef struct cypher_delete_item {
    ast_node base;
    char *variable;       /* Variable name to delete */
} cypher_delete_item;

/* REMOVE clause */
typedef struct cypher_remove {
    ast_node base;
    ast_list *items;      /* List of remove items (properties or labels) */
} cypher_remove;

/* REMOVE item - can be property (n.prop) or label (n:Label) */
typedef struct cypher_remove_item {
    ast_node base;
    ast_node *target;     /* Property access (n.prop) or label expr (n:Label) */
} cypher_remove_item;

/* WHERE clause */
typedef struct cypher_where {
    ast_node base;
    ast_node *expr;       /* Boolean expression */
} cypher_where;

/* Return item */
typedef struct cypher_return_item {
    ast_node base;
    ast_node *expr;       /* Expression to return */
    char *alias;          /* Optional alias (AS alias) */
} cypher_return_item;

/* Order by item: expression with optional ASC/DESC */
typedef struct cypher_order_by_item {
    ast_node base;
    ast_node *expr;       /* Expression to sort by */
    bool descending;      /* true for DESC, false for ASC (default) */
} cypher_order_by_item;

/* Node pattern: (var:Label {props}) or (var:Label1:Label2 {props}) */
typedef struct cypher_node_pattern {
    ast_node base;
    char *variable;       /* Variable name (optional) */
    ast_list *labels;     /* List of node labels (optional) - supports :A:B:C syntax */
    ast_node *properties; /* Property map (optional) */
} cypher_node_pattern;

/* Relationship pattern: -[var:TYPE {props}]-> */
typedef struct cypher_rel_pattern {
    ast_node base;
    char *variable;       /* Variable name (optional) */
    char *type;           /* Single relationship type (optional) - deprecated, use types */
    ast_list *types;      /* List of relationship types (optional) for [:TYPE1|TYPE2] syntax */
    ast_node *properties; /* Property map (optional) */
    bool left_arrow;      /* <- direction */
    bool right_arrow;     /* -> direction */
    ast_node *varlen;     /* Variable length range (optional) */
} cypher_rel_pattern;

/* Variable-length range: [*1..5], [*], [*2..], [*..3] */
typedef struct cypher_varlen_range {
    ast_node base;
    int min_hops;         /* Minimum hops (-1 = unbounded/default to 1) */
    int max_hops;         /* Maximum hops (-1 = unbounded) */
} cypher_varlen_range;

/* Path type for shortest path queries */
typedef enum {
    PATH_TYPE_NORMAL,       /* Regular path matching */
    PATH_TYPE_SHORTEST,     /* shortestPath() - single shortest path */
    PATH_TYPE_ALL_SHORTEST  /* allShortestPaths() - all paths of minimum length */
} path_type;

/* Path pattern */
typedef struct cypher_path {
    ast_node base;
    ast_list *elements;   /* Alternating nodes and relationships */
    char *var_name;       /* Variable name for path assignment (optional) */
    path_type type;       /* Type of path matching (normal, shortest, all_shortest) */
} cypher_path;

/* Literal expression */
typedef struct cypher_literal {
    ast_node base;
    enum {
        LITERAL_INTEGER,
        LITERAL_DECIMAL,
        LITERAL_STRING,
        LITERAL_BOOLEAN,
        LITERAL_NULL
    } literal_type;
    union {
        int integer;
        double decimal;
        char *string;
        bool boolean;
    } value;
} cypher_literal;

/* Identifier expression */
typedef struct cypher_identifier {
    ast_node base;
    char *name;
} cypher_identifier;

/* Parameter expression */
typedef struct cypher_parameter {
    ast_node base;
    char *name;
} cypher_parameter;

/* Property access: expr.property */
typedef struct cypher_property {
    ast_node base;
    ast_node *expr;       /* Expression being accessed */
    char *property_name;  /* Property name */
} cypher_property;

/* Label expression: expr:Label */
typedef struct cypher_label_expr {
    ast_node base;
    ast_node *expr;       /* Expression to check (usually identifier) */
    char *label_name;     /* Label name to check for */
} cypher_label_expr;

/* NOT expression: NOT expr */
typedef struct cypher_not_expr {
    ast_node base;
    ast_node *expr;       /* Expression to negate */
} cypher_not_expr;

/* NULL check expression: expr IS NULL / expr IS NOT NULL */
typedef struct cypher_null_check {
    ast_node base;
    ast_node *expr;       /* Expression to check for NULL */
    bool is_not_null;     /* true for IS NOT NULL, false for IS NULL */
} cypher_null_check;

/* Binary operation: expr OP expr */
typedef struct cypher_binary_op {
    ast_node base;
    binary_op_type op_type;  /* Operation type (AND, OR, EQ, etc.) */
    ast_node *left;          /* Left expression */
    ast_node *right;         /* Right expression */
} cypher_binary_op;

/* Function call */
typedef struct cypher_function_call {
    ast_node base;
    char *function_name;
    ast_list *args;       /* List of argument expressions */
    bool distinct;        /* Function(DISTINCT ...) */
} cypher_function_call;

/* EXISTS expression: EXISTS((pattern)) or EXISTS(property) */
typedef struct cypher_exists_expr {
    ast_node base;
    exists_expr_type expr_type;  /* Pattern or property */
    union {
        ast_list *pattern;       /* For EXISTS((pattern)) - list of path elements */
        ast_node *property;      /* For EXISTS(property) - property access expression */
    } expr;
} cypher_exists_expr;

/* List predicate: all/any/none/single(x IN list WHERE predicate) */
typedef struct cypher_list_predicate {
    ast_node base;
    list_predicate_type pred_type;   /* ALL, ANY, NONE, or SINGLE */
    char *variable;                   /* Iteration variable (e.g., 'x') */
    ast_node *list_expr;             /* List expression to iterate */
    ast_node *predicate;             /* WHERE predicate */
} cypher_list_predicate;

/* Reduce expression: reduce(acc = initial, x IN list | expr) */
typedef struct cypher_reduce_expr {
    ast_node base;
    char *accumulator;               /* Accumulator variable name */
    ast_node *initial_value;         /* Initial accumulator value */
    char *variable;                  /* Iteration variable (e.g., 'x') */
    ast_node *list_expr;             /* List expression to iterate */
    ast_node *expression;            /* Expression using acc and x */
} cypher_reduce_expr;

/* Map literal: {key: value, ...} */
typedef struct cypher_map {
    ast_node base;
    ast_list *pairs;      /* List of key-value pairs */
} cypher_map;

/* Map key-value pair */
typedef struct cypher_map_pair {
    ast_node base;
    char *key;
    ast_node *value;
} cypher_map_pair;

/* Map projection: n{.prop1, .prop2} or n{alias: .prop, ...} */
typedef struct cypher_map_projection {
    ast_node base;
    ast_node *base_expr;      /* Base expression (usually identifier like 'n') */
    ast_list *items;          /* List of projection items (property names or key:value pairs) */
} cypher_map_projection;

/* Map projection item: .prop or alias: .prop or alias: expr */
typedef struct cypher_map_projection_item {
    ast_node base;
    char *key;                /* Output key name (NULL for shorthand .prop) */
    char *property;           /* Property name for .prop syntax (NULL if using expr) */
    ast_node *expr;           /* Expression value (NULL for simple .prop) */
} cypher_map_projection_item;

/* List literal: [item1, item2, ...] */
typedef struct cypher_list {
    ast_node base;
    ast_list *items;      /* List of expressions */
} cypher_list;

/* List comprehension: [x IN list WHERE condition | transform] */
typedef struct cypher_list_comprehension {
    ast_node base;
    char *variable;           /* Loop variable name */
    ast_node *list_expr;      /* Source list expression */
    ast_node *where_expr;     /* Optional filter condition (NULL if not present) */
    ast_node *transform_expr; /* Optional transform expression (NULL if not present) */
} cypher_list_comprehension;

/* Pattern comprehension: [(n)-[r]->(m) WHERE condition | expression] */
typedef struct cypher_pattern_comprehension {
    ast_node base;
    ast_list *pattern;        /* The pattern to match (list of path elements) */
    ast_node *where_expr;     /* Optional filter condition (NULL if not present) */
    ast_node *collect_expr;   /* Expression to collect (NULL returns matched nodes/rels) */
} cypher_pattern_comprehension;

/* CASE expression: CASE WHEN cond THEN val [...] [ELSE val] END */
typedef struct cypher_case_expr {
    ast_node base;
    ast_list *when_clauses;  /* List of when_clause nodes */
    ast_node *else_expr;     /* Optional ELSE expression */
} cypher_case_expr;

/* WHEN clause: WHEN condition THEN result */
typedef struct cypher_when_clause {
    ast_node base;
    ast_node *condition;     /* WHEN expression */
    ast_node *result;        /* THEN expression */
} cypher_when_clause;

/* Function prototypes */

/* Memory management */
ast_node* ast_node_create(ast_node_type type, int location, size_t size);
void ast_node_free(ast_node *node);
ast_list* ast_list_create(void);
void ast_list_free(ast_list *list);
void ast_list_append(ast_list *list, ast_node *node);

/* Node creation functions */
cypher_query* make_cypher_query(ast_list *clauses, bool explain);
cypher_union* make_cypher_union(ast_node *left, ast_node *right, bool all, int location);
cypher_match* make_cypher_match(ast_list *pattern, ast_node *where, bool optional);
cypher_return* make_cypher_return(bool distinct, ast_list *items, ast_list *order_by, ast_node *skip, ast_node *limit);
cypher_with* make_cypher_with(bool distinct, ast_list *items, ast_list *order_by, ast_node *skip, ast_node *limit, ast_node *where);
cypher_unwind* make_cypher_unwind(ast_node *expr, char *alias, int location);
cypher_foreach* make_cypher_foreach(char *variable, ast_node *list_expr, ast_list *body, int location);
cypher_load_csv* make_cypher_load_csv(char *file_path, char *variable, bool with_headers, char *fieldterminator, int location);
cypher_create* make_cypher_create(ast_list *pattern);
cypher_merge* make_cypher_merge(ast_list *pattern, ast_list *on_create, ast_list *on_match);
cypher_set* make_cypher_set(ast_list *items);
cypher_set_item* make_cypher_set_item(ast_node *property, ast_node *expr);
cypher_delete* make_cypher_delete(ast_list *items, bool detach);
cypher_delete_item* make_delete_item(char *variable);
cypher_remove* make_cypher_remove(ast_list *items);
cypher_remove_item* make_remove_item(ast_node *target);
cypher_return_item* make_return_item(ast_node *expr, char *alias);
cypher_order_by_item* make_order_by_item(ast_node *expr, bool descending);
cypher_node_pattern* make_node_pattern(char *variable, ast_list *labels, ast_node *properties);
cypher_rel_pattern* make_rel_pattern(char *variable, char *type, ast_node *properties, bool left_arrow, bool right_arrow);
cypher_rel_pattern* make_rel_pattern_multi_type(char *variable, ast_list *types, ast_node *properties, bool left_arrow, bool right_arrow);
cypher_rel_pattern* make_rel_pattern_varlen(char *variable, char *type, ast_node *properties, bool left_arrow, bool right_arrow, ast_node *varlen);
cypher_varlen_range* make_varlen_range(int min_hops, int max_hops);
cypher_path* make_path(ast_list *elements);
cypher_path* make_path_with_var(char *var_name, ast_list *elements);
cypher_path* make_shortest_path(ast_list *elements, path_type type);
cypher_literal* make_integer_literal(int value, int location);
cypher_literal* make_decimal_literal(double value, int location);
cypher_literal* make_string_literal(char *value, int location);
cypher_literal* make_boolean_literal(bool value, int location);
cypher_literal* make_null_literal(int location);
cypher_identifier* make_identifier(char *name, int location);
cypher_parameter* make_parameter(char *name, int location);
cypher_property* make_property(ast_node *expr, char *property_name, int location);
cypher_label_expr* make_label_expr(ast_node *expr, char *label_name, int location);
cypher_not_expr* make_not_expr(ast_node *expr, int location);
cypher_null_check* make_null_check(ast_node *expr, bool is_not_null, int location);
cypher_binary_op* make_binary_op(binary_op_type op_type, ast_node *left, ast_node *right, int location);
cypher_function_call* make_function_call(char *function_name, ast_list *args, bool distinct, int location);
cypher_exists_expr* make_exists_pattern_expr(ast_list *pattern, int location);
cypher_exists_expr* make_exists_property_expr(ast_node *property, int location);
cypher_list_predicate* make_list_predicate(list_predicate_type pred_type, const char *variable, ast_node *list_expr, ast_node *predicate, int location);
cypher_reduce_expr* make_reduce_expr(const char *accumulator, ast_node *initial_value, const char *variable, ast_node *list_expr, ast_node *expression, int location);
cypher_map* make_map(ast_list *pairs, int location);
cypher_map_pair* make_map_pair(char *key, ast_node *value, int location);
cypher_map_projection* make_map_projection(ast_node *base_expr, ast_list *items, int location);
cypher_map_projection_item* make_map_projection_item(char *key, char *property, ast_node *expr, int location);
cypher_list* make_list(ast_list *items, int location);
cypher_list_comprehension* make_list_comprehension(const char *variable, ast_node *list_expr, ast_node *where_expr, ast_node *transform_expr, int location);
cypher_pattern_comprehension* make_pattern_comprehension(ast_list *pattern, ast_node *where_expr, ast_node *collect_expr, int location);
cypher_case_expr* make_case_expr(ast_list *when_clauses, ast_node *else_expr, int location);
cypher_when_clause* make_when_clause(ast_node *condition, ast_node *result, int location);

/* Utility functions */
const char* ast_node_type_name(ast_node_type type);
void ast_node_print(ast_node *node, int indent);

#endif /* CYPHER_AST_H */