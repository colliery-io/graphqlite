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
    AST_VARIABLE,
    AST_LABEL,
    AST_PROPERTY,
    AST_PROPERTY_LIST,
    AST_STRING_LITERAL,
    AST_INTEGER_LITERAL,
    AST_FLOAT_LITERAL,
    AST_BOOLEAN_LITERAL
} ast_node_type_t;

// AST node structure
struct cypher_ast_node {
    ast_node_type_t type;
    union {
        struct {
            cypher_ast_node_t *node_pattern;
        } create_stmt;
        
        struct {
            cypher_ast_node_t *node_pattern;
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
    } data;
};

// AST creation functions
cypher_ast_node_t* ast_create_create_statement(cypher_ast_node_t *node_pattern);
cypher_ast_node_t* ast_create_match_statement(cypher_ast_node_t *node_pattern);
cypher_ast_node_t* ast_create_return_statement(cypher_ast_node_t *variable);
cypher_ast_node_t* ast_create_compound_statement(cypher_ast_node_t *match_stmt, cypher_ast_node_t *return_stmt);
cypher_ast_node_t* ast_create_node_pattern(cypher_ast_node_t *variable, cypher_ast_node_t *label, cypher_ast_node_t *properties);
cypher_ast_node_t* ast_create_variable(const char *name);
cypher_ast_node_t* ast_create_label(const char *name);
cypher_ast_node_t* ast_create_property(const char *key, cypher_ast_node_t *value);
cypher_ast_node_t* ast_create_property_list(void);
cypher_ast_node_t* ast_add_property_to_list(cypher_ast_node_t *list, cypher_ast_node_t *property);
cypher_ast_node_t* ast_create_string_literal(const char *value);
cypher_ast_node_t* ast_create_integer_literal(const char *value);
cypher_ast_node_t* ast_create_float_literal(const char *value);
cypher_ast_node_t* ast_create_boolean_literal(int value);

// AST utility functions
void ast_free(cypher_ast_node_t *node);
void ast_print(cypher_ast_node_t *node, int indent);
const char* ast_node_type_name(ast_node_type_t type);

#ifdef __cplusplus
}
#endif

#endif // AST_H