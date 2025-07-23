#ifndef AST_H
#define AST_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct cypher_ast_node cypher_ast_node_t;

// AST node types for our minimal implementation
typedef enum {
    AST_CREATE_STATEMENT,
    AST_MATCH_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_COMPOUND_STATEMENT,  // MATCH + RETURN combination
    AST_NODE_PATTERN,
    AST_RELATIONSHIP_PATTERN,
    AST_PATH_PATTERN,
    AST_EDGE_PATTERN,
    AST_VARIABLE,
    AST_LABEL,
    AST_PROPERTY,
    AST_PROPERTY_LIST,
    AST_STRING_LITERAL,
    AST_INTEGER_LITERAL,
    AST_FLOAT_LITERAL,
    AST_BOOLEAN_LITERAL,
    AST_WHERE_CLAUSE,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_PROPERTY_ACCESS,
    AST_IS_NULL_EXPR,
    AST_IDENTIFIER
} ast_node_type_t;

// Operator types for expressions
typedef enum {
    AST_OP_EQ,    // =
    AST_OP_NEQ,   // <>
    AST_OP_LT,    // <
    AST_OP_GT,    // >
    AST_OP_LE,    // <=
    AST_OP_GE,    // >=
    AST_OP_AND,
    AST_OP_OR,
    AST_OP_NOT
} ast_operator_t;

// AST node structure
struct cypher_ast_node {
    ast_node_type_t type;
    union {
        struct {
            cypher_ast_node_t *node_pattern;
        } create_stmt;
        
        struct {
            cypher_ast_node_t *node_pattern;
            cypher_ast_node_t *where_clause;  // Optional WHERE clause
        } match_stmt;
        
        struct {
            cypher_ast_node_t *variable;
        } return_stmt;
        
        struct {
            cypher_ast_node_t *match_stmt;
            cypher_ast_node_t *return_stmt;
        } compound_stmt;
        
        struct {
            cypher_ast_node_t *variable;     // Variable name (e.g., "n")
            cypher_ast_node_t *label;        // Label (e.g., "Person")
            cypher_ast_node_t *properties;   // Property list (optional)
        } node_pattern;
        
        struct {
            cypher_ast_node_t *left_node;    // Left node pattern
            cypher_ast_node_t *edge;         // Edge pattern (optional)
            cypher_ast_node_t *right_node;   // Right node pattern
            int direction;                   // 0=undirected, 1=right, -1=left
        } relationship_pattern;
        
        struct {
            cypher_ast_node_t **patterns;    // Array of relationship patterns
            int count;                       // Number of patterns
        } path_pattern;
        
        struct {
            cypher_ast_node_t *variable;     // Variable name (optional)
            cypher_ast_node_t *label;        // Edge type/label (optional)
            cypher_ast_node_t *properties;   // Property list (optional)
        } edge_pattern;
        
        struct {
            char *name;
        } variable;
        
        struct {
            char *name;
        } label;
        
        struct {
            char *key;
            cypher_ast_node_t *value;
        } property;
        
        struct {
            cypher_ast_node_t **properties;  // Array of property nodes
            int count;                       // Number of properties
        } property_list;
        
        struct {
            char *value;
        } string_literal;
        
        struct {
            int value;
        } integer_literal;
        
        struct {
            double value;
        } float_literal;
        
        struct {
            int value;  // 0 = false, 1 = true
        } boolean_literal;
        
        struct {
            cypher_ast_node_t *expression;
        } where_clause;
        
        struct {
            cypher_ast_node_t *left;
            cypher_ast_node_t *right;
            ast_operator_t op;
        } binary_expr;
        
        struct {
            cypher_ast_node_t *operand;
            ast_operator_t op;
        } unary_expr;
        
        struct {
            char *variable;  // e.g., "n"
            char *property;  // e.g., "age"
        } property_access;
        
        struct {
            cypher_ast_node_t *expression;
            int is_null;  // 1 for IS NULL, 0 for IS NOT NULL
        } is_null_expr;
        
        struct {
            char *name;
        } identifier;
    } data;
};

// AST creation functions
cypher_ast_node_t* ast_create_create_statement(cypher_ast_node_t *node_pattern);
cypher_ast_node_t* ast_create_match_statement(cypher_ast_node_t *node_pattern);
cypher_ast_node_t* ast_create_return_statement(cypher_ast_node_t *variable);
cypher_ast_node_t* ast_create_compound_statement(cypher_ast_node_t *match_stmt, cypher_ast_node_t *return_stmt);
cypher_ast_node_t* ast_create_node_pattern(cypher_ast_node_t *variable, cypher_ast_node_t *label, cypher_ast_node_t *properties);
cypher_ast_node_t* ast_create_relationship_pattern(cypher_ast_node_t *left_node, cypher_ast_node_t *edge, cypher_ast_node_t *right_node, int direction);
cypher_ast_node_t* ast_create_path_pattern(void);
cypher_ast_node_t* ast_add_relationship_to_path(cypher_ast_node_t *path, cypher_ast_node_t *relationship);
cypher_ast_node_t* ast_create_edge_pattern(cypher_ast_node_t *variable, cypher_ast_node_t *label, cypher_ast_node_t *properties);
cypher_ast_node_t* ast_create_variable(const char *name);
cypher_ast_node_t* ast_create_label(const char *name);
cypher_ast_node_t* ast_create_property(const char *key, cypher_ast_node_t *value);
cypher_ast_node_t* ast_create_property_list(void);
cypher_ast_node_t* ast_add_property_to_list(cypher_ast_node_t *list, cypher_ast_node_t *property);
cypher_ast_node_t* ast_create_string_literal(const char *value);
cypher_ast_node_t* ast_create_integer_literal(const char *value);
cypher_ast_node_t* ast_create_float_literal(const char *value);
cypher_ast_node_t* ast_create_boolean_literal(int value);

// WHERE and expression creation functions
cypher_ast_node_t* ast_create_where_clause(cypher_ast_node_t *expression);
cypher_ast_node_t* ast_create_binary_expr(cypher_ast_node_t *left, ast_operator_t op, cypher_ast_node_t *right);
cypher_ast_node_t* ast_create_unary_expr(ast_operator_t op, cypher_ast_node_t *operand);
cypher_ast_node_t* ast_create_property_access(const char *variable, const char *property);
cypher_ast_node_t* ast_create_is_null_expr(cypher_ast_node_t *expression, int is_null);
cypher_ast_node_t* ast_create_identifier(const char *name);
cypher_ast_node_t* ast_attach_where_clause(cypher_ast_node_t *match_stmt, cypher_ast_node_t *where_clause);

// AST utility functions
void ast_free(cypher_ast_node_t *node);
void ast_print(cypher_ast_node_t *node, int indent);
const char* ast_node_type_name(ast_node_type_t type);

#ifdef __cplusplus
}
#endif

#endif // AST_H