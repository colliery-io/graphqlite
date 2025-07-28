#ifndef CYPHER_AST_H
#define CYPHER_AST_H

#include <stdbool.h>

/* AST node types */
typedef enum ast_node_type {
    AST_NODE_UNKNOWN = 0,
    
    /* Query structure */
    AST_NODE_QUERY,
    AST_NODE_SINGLE_QUERY,
    
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
    AST_NODE_MERGE,
    AST_NODE_UNWIND,
    
    /* Patterns */
    AST_NODE_PATTERN,
    AST_NODE_PATH,
    AST_NODE_NODE_PATTERN,
    AST_NODE_REL_PATTERN,
    
    /* Expressions */
    AST_NODE_EXPR,
    AST_NODE_LITERAL,
    AST_NODE_IDENTIFIER,
    AST_NODE_PARAMETER,
    AST_NODE_PROPERTY,
    AST_NODE_LABEL_EXPR,
    AST_NODE_NOT_EXPR,
    AST_NODE_BINARY_OP,
    AST_NODE_FUNCTION_CALL,
    AST_NODE_LIST,
    AST_NODE_MAP,
    AST_NODE_MAP_PAIR,
    
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
    BINARY_OP_EQ,
    BINARY_OP_NEQ,
    BINARY_OP_LT,
    BINARY_OP_GT,
    BINARY_OP_LTE,
    BINARY_OP_GTE,
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_MUL,
    BINARY_OP_DIV
} binary_op_type;

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
} cypher_query;

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

/* CREATE clause */
typedef struct cypher_create {
    ast_node base;
    ast_list *pattern;    /* List of patterns to create */
} cypher_create;

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

/* Node pattern: (var:Label {props}) */
typedef struct cypher_node_pattern {
    ast_node base;
    char *variable;       /* Variable name (optional) */
    char *label;          /* Node label (optional) */
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

/* Path pattern */
typedef struct cypher_path {
    ast_node base;
    ast_list *elements;   /* Alternating nodes and relationships */
    char *var_name;       /* Variable name for path assignment (optional) */
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

/* List literal: [item1, item2, ...] */
typedef struct cypher_list {
    ast_node base;
    ast_list *items;      /* List of expressions */
} cypher_list;

/* Function prototypes */

/* Memory management */
ast_node* ast_node_create(ast_node_type type, int location, size_t size);
void ast_node_free(ast_node *node);
ast_list* ast_list_create(void);
void ast_list_free(ast_list *list);
void ast_list_append(ast_list *list, ast_node *node);

/* Node creation functions */
cypher_query* make_cypher_query(ast_list *clauses);
cypher_match* make_cypher_match(ast_list *pattern, ast_node *where, bool optional);
cypher_return* make_cypher_return(bool distinct, ast_list *items, ast_list *order_by, ast_node *skip, ast_node *limit);
cypher_create* make_cypher_create(ast_list *pattern);
cypher_set* make_cypher_set(ast_list *items);
cypher_set_item* make_cypher_set_item(ast_node *property, ast_node *expr);
cypher_delete* make_cypher_delete(ast_list *items, bool detach);
cypher_delete_item* make_delete_item(char *variable);
cypher_return_item* make_return_item(ast_node *expr, char *alias);
cypher_order_by_item* make_order_by_item(ast_node *expr, bool descending);
cypher_node_pattern* make_node_pattern(char *variable, char *label, ast_node *properties);
cypher_rel_pattern* make_rel_pattern(char *variable, char *type, ast_node *properties, bool left_arrow, bool right_arrow);
cypher_rel_pattern* make_rel_pattern_multi_type(char *variable, ast_list *types, ast_node *properties, bool left_arrow, bool right_arrow);
cypher_path* make_path(ast_list *elements);
cypher_path* make_path_with_var(char *var_name, ast_list *elements);
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
cypher_binary_op* make_binary_op(binary_op_type op_type, ast_node *left, ast_node *right, int location);
cypher_function_call* make_function_call(char *function_name, ast_list *args, bool distinct, int location);
cypher_map* make_map(ast_list *pairs, int location);
cypher_map_pair* make_map_pair(char *key, ast_node *value, int location);
cypher_list* make_list(ast_list *items, int location);

/* Utility functions */
const char* ast_node_type_name(ast_node_type type);
void ast_node_print(ast_node *node, int indent);

#endif /* CYPHER_AST_H */