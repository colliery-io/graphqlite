#ifndef GQL_AST_H
#define GQL_AST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// AST Node Types
// ============================================================================

typedef enum {
    // Queries
    GQL_AST_MATCH_QUERY,
    GQL_AST_CREATE_QUERY,
    GQL_AST_SET_QUERY,
    GQL_AST_DELETE_QUERY,
    
    // Patterns
    GQL_AST_PATTERN,
    GQL_AST_NODE_PATTERN,
    GQL_AST_EDGE_PATTERN,
    
    // Expressions
    GQL_AST_BINARY_EXPR,
    GQL_AST_UNARY_EXPR,
    GQL_AST_PROPERTY_ACCESS,
    GQL_AST_LITERAL,
    GQL_AST_IDENTIFIER,
    
    // Clauses
    GQL_AST_WHERE_CLAUSE,
    GQL_AST_RETURN_CLAUSE,
    GQL_AST_SET_CLAUSE,
    
    // Lists
    GQL_AST_PATTERN_LIST,
    GQL_AST_RETURN_LIST,
    GQL_AST_RETURN_ITEM,
    GQL_AST_ASSIGNMENT_LIST,
    GQL_AST_PROPERTY_MAP,
    
    // Literals
    GQL_AST_STRING_LITERAL,
    GQL_AST_INTEGER_LITERAL,
    GQL_AST_BOOLEAN_LITERAL,
    GQL_AST_NULL_LITERAL
} gql_ast_node_type_t;

typedef enum {
    GQL_OP_AND,
    GQL_OP_OR,
    GQL_OP_NOT,
    GQL_OP_EQUALS,
    GQL_OP_NOT_EQUALS,
    GQL_OP_LESS_THAN,
    GQL_OP_LESS_EQUAL,
    GQL_OP_GREATER_THAN,
    GQL_OP_GREATER_EQUAL,
    GQL_OP_IS_NULL,
    GQL_OP_IS_NOT_NULL,
    GQL_OP_STARTS_WITH,
    GQL_OP_ENDS_WITH,
    GQL_OP_CONTAINS
} gql_operator_t;

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct gql_ast_node gql_ast_node_t;

// ============================================================================
// AST Node Structures
// ============================================================================

// Generic AST node
struct gql_ast_node {
    gql_ast_node_type_t type;
    union {
        struct {
            gql_ast_node_t *patterns;
            gql_ast_node_t *where_clause;
            gql_ast_node_t *return_clause;
        } match_query;
        
        struct {
            gql_ast_node_t *patterns;
        } create_query;
        
        struct {
            gql_ast_node_t *patterns;
            gql_ast_node_t *where_clause;
            gql_ast_node_t *assignments;
        } set_query;
        
        struct {
            gql_ast_node_t *patterns;
            gql_ast_node_t *where_clause;
            gql_ast_node_t *identifiers;
        } delete_query;
        
        struct {
            gql_ast_node_t *node;
            gql_ast_node_t *edge;
            gql_ast_node_t *target_node;
        } pattern;
        
        struct {
            char *variable;           // Optional variable name
            gql_ast_node_t *labels;  // Optional list of labels (string literals)
            gql_ast_node_t *properties; // Optional property map
        } node_pattern;
        
        struct {
            char *variable;           // Optional variable name
            char *type;              // Optional edge type
            gql_ast_node_t *properties; // Optional property map
            bool directed;           // true for ->, false for <-
        } edge_pattern;
        
        struct {
            gql_operator_t operator;
            gql_ast_node_t *left;
            gql_ast_node_t *right;
        } binary_expr;
        
        struct {
            gql_operator_t operator;
            gql_ast_node_t *operand;
        } unary_expr;
        
        struct {
            char *object;            // Left side of dot
            char *property;          // Right side of dot
        } property_access;
        
        struct {
            char *name;
        } identifier;
        
        struct {
            gql_ast_node_t *expression;
        } where_clause;
        
        struct {
            gql_ast_node_t *items;   // List of return items
            bool distinct;
        } return_clause;
        
        struct {
            gql_ast_node_t *expression;  // The expression to return
            char *alias;                 // Optional alias (AS clause)
        } return_item;
        
        struct {
            gql_ast_node_t *assignments; // List of assignments
        } set_clause;
        
        struct {
            char *value;
        } string_literal;
        
        struct {
            int64_t value;
        } integer_literal;
        
        struct {
            bool value;
        } boolean_literal;
        
        struct {
            // No value for null
        } null_literal;
    } data;
    
    // List management (for list nodes)
    gql_ast_node_t *next;        // Next item in list
    
    // Memory management
    bool owns_strings;           // Whether this node owns its string data
};

// ============================================================================
// AST Construction Functions
// ============================================================================

// Node creation
gql_ast_node_t* gql_ast_create_match_query(gql_ast_node_t *patterns, 
                                           gql_ast_node_t *where_clause,
                                           gql_ast_node_t *return_clause);

gql_ast_node_t* gql_ast_create_create_query(gql_ast_node_t *patterns);

gql_ast_node_t* gql_ast_create_set_query(gql_ast_node_t *patterns,
                                         gql_ast_node_t *where_clause,
                                         gql_ast_node_t *assignments);

gql_ast_node_t* gql_ast_create_delete_query(gql_ast_node_t *patterns,
                                            gql_ast_node_t *where_clause,
                                            gql_ast_node_t *identifiers);

gql_ast_node_t* gql_ast_create_pattern(gql_ast_node_t *node,
                                      gql_ast_node_t *edge,
                                      gql_ast_node_t *target_node);

gql_ast_node_t* gql_ast_create_node_pattern(const char *variable,
                                           gql_ast_node_t *labels,
                                           gql_ast_node_t *properties);

gql_ast_node_t* gql_ast_create_edge_pattern(const char *variable,
                                           const char *type,
                                           gql_ast_node_t *properties,
                                           bool directed);

gql_ast_node_t* gql_ast_create_binary_expr(gql_operator_t operator,
                                          gql_ast_node_t *left,
                                          gql_ast_node_t *right);

gql_ast_node_t* gql_ast_create_unary_expr(gql_operator_t operator,
                                         gql_ast_node_t *operand);

gql_ast_node_t* gql_ast_create_property_access(const char *object,
                                              const char *property);

gql_ast_node_t* gql_ast_create_identifier(const char *name);

gql_ast_node_t* gql_ast_create_return_item(gql_ast_node_t *expression, const char *alias);

gql_ast_node_t* gql_ast_create_string_literal(const char *value);
gql_ast_node_t* gql_ast_create_integer_literal(int64_t value);
gql_ast_node_t* gql_ast_create_boolean_literal(bool value);
gql_ast_node_t* gql_ast_create_null_literal(void);

// List operations
gql_ast_node_t* gql_ast_create_list(gql_ast_node_type_t type);
void gql_ast_list_append(gql_ast_node_t *list, gql_ast_node_t *item);
size_t gql_ast_list_length(gql_ast_node_t *list);

// Memory management
void gql_ast_free(gql_ast_node_t *node);
void gql_ast_free_recursive(gql_ast_node_t *node);

// Utility functions
const char* gql_ast_node_type_name(gql_ast_node_type_t type);
const char* gql_operator_name(gql_operator_t op);
void gql_ast_print(gql_ast_node_t *node, int indent);

#ifdef __cplusplus
}
#endif

#endif // GQL_AST_H