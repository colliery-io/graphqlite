#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// AST Node Creation Functions
// ============================================================================

static cypher_ast_node_t* ast_create_node(ast_node_type_t type) {
    cypher_ast_node_t *node = malloc(sizeof(cypher_ast_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(cypher_ast_node_t));
    node->type = type;
    return node;
}

cypher_ast_node_t* ast_create_create_statement(cypher_ast_node_t *node_pattern) {
    cypher_ast_node_t *node = ast_create_node(AST_CREATE_STATEMENT);
    if (!node) return NULL;
    
    node->data.create_stmt.node_pattern = node_pattern;
    return node;
}

cypher_ast_node_t* ast_create_match_statement(cypher_ast_node_t *node_pattern) {
    cypher_ast_node_t *node = ast_create_node(AST_MATCH_STATEMENT);
    if (!node) return NULL;
    
    node->data.match_stmt.node_pattern = node_pattern;
    return node;
}

cypher_ast_node_t* ast_create_return_statement(cypher_ast_node_t *variable) {
    cypher_ast_node_t *node = ast_create_node(AST_RETURN_STATEMENT);
    if (!node) return NULL;
    
    node->data.return_stmt.variable = variable;
    return node;
}

cypher_ast_node_t* ast_create_compound_statement(cypher_ast_node_t *match_stmt, cypher_ast_node_t *return_stmt) {
    cypher_ast_node_t *node = ast_create_node(AST_COMPOUND_STATEMENT);
    if (!node) return NULL;
    
    node->data.compound_stmt.match_stmt = match_stmt;
    node->data.compound_stmt.return_stmt = return_stmt;
    return node;
}

cypher_ast_node_t* ast_create_node_pattern(cypher_ast_node_t *variable, cypher_ast_node_t *label, cypher_ast_node_t *properties) {
    cypher_ast_node_t *node = ast_create_node(AST_NODE_PATTERN);
    if (!node) return NULL;
    
    node->data.node_pattern.variable = variable;
    node->data.node_pattern.label = label;
    node->data.node_pattern.properties = properties;
    return node;
}

cypher_ast_node_t* ast_create_variable(const char *name) {
    cypher_ast_node_t *node = ast_create_node(AST_VARIABLE);
    if (!node) return NULL;
    
    node->data.variable.name = strdup(name);
    return node;
}

cypher_ast_node_t* ast_create_label(const char *name) {
    cypher_ast_node_t *node = ast_create_node(AST_LABEL);
    if (!node) return NULL;
    
    node->data.label.name = strdup(name);
    return node;
}

cypher_ast_node_t* ast_create_property(const char *key, cypher_ast_node_t *value) {
    cypher_ast_node_t *node = ast_create_node(AST_PROPERTY);
    if (!node) return NULL;
    
    node->data.property.key = strdup(key);
    node->data.property.value = value;
    return node;
}

cypher_ast_node_t* ast_create_property_list(void) {
    cypher_ast_node_t *node = ast_create_node(AST_PROPERTY_LIST);
    if (!node) return NULL;
    
    node->data.property_list.properties = NULL;
    node->data.property_list.count = 0;
    return node;
}

cypher_ast_node_t* ast_add_property_to_list(cypher_ast_node_t *list, cypher_ast_node_t *property) {
    if (!list || list->type != AST_PROPERTY_LIST || !property) return list;
    
    int new_count = list->data.property_list.count + 1;
    cypher_ast_node_t **new_props = realloc(list->data.property_list.properties, 
                                           new_count * sizeof(cypher_ast_node_t*));
    if (!new_props) return list;
    
    new_props[list->data.property_list.count] = property;
    list->data.property_list.properties = new_props;
    list->data.property_list.count = new_count;
    
    return list;
}

cypher_ast_node_t* ast_create_string_literal(const char *value) {
    cypher_ast_node_t *node = ast_create_node(AST_STRING_LITERAL);
    if (!node) return NULL;
    
    node->data.string_literal.value = strdup(value);
    return node;
}

cypher_ast_node_t* ast_create_integer_literal(const char *value) {
    cypher_ast_node_t *node = ast_create_node(AST_INTEGER_LITERAL);
    if (!node) return NULL;
    
    node->data.integer_literal.value = atoi(value);
    return node;
}

cypher_ast_node_t* ast_create_float_literal(const char *value) {
    cypher_ast_node_t *node = ast_create_node(AST_FLOAT_LITERAL);
    if (!node) return NULL;
    
    node->data.float_literal.value = atof(value);
    return node;
}

cypher_ast_node_t* ast_create_boolean_literal(int value) {
    cypher_ast_node_t *node = ast_create_node(AST_BOOLEAN_LITERAL);
    if (!node) return NULL;
    
    node->data.boolean_literal.value = value ? 1 : 0;
    return node;
}

// ============================================================================
// AST Memory Management
// ============================================================================

void ast_free(cypher_ast_node_t *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_CREATE_STATEMENT:
            ast_free(node->data.create_stmt.node_pattern);
            break;
            
        case AST_MATCH_STATEMENT:
            ast_free(node->data.match_stmt.node_pattern);
            break;
            
        case AST_RETURN_STATEMENT:
            ast_free(node->data.return_stmt.variable);
            break;
            
        case AST_COMPOUND_STATEMENT:
            ast_free(node->data.compound_stmt.match_stmt);
            ast_free(node->data.compound_stmt.return_stmt);
            break;
            
        case AST_NODE_PATTERN:
            ast_free(node->data.node_pattern.variable);
            ast_free(node->data.node_pattern.label);
            ast_free(node->data.node_pattern.properties);
            break;
            
        case AST_VARIABLE:
            free(node->data.variable.name);
            break;
            
        case AST_LABEL:
            free(node->data.label.name);
            break;
            
        case AST_PROPERTY:
            free(node->data.property.key);
            ast_free(node->data.property.value);
            break;
            
        case AST_PROPERTY_LIST:
            for (int i = 0; i < node->data.property_list.count; i++) {
                ast_free(node->data.property_list.properties[i]);
            }
            free(node->data.property_list.properties);
            break;
            
        case AST_STRING_LITERAL:
            free(node->data.string_literal.value);
            break;
            
        case AST_INTEGER_LITERAL:
        case AST_FLOAT_LITERAL:
        case AST_BOOLEAN_LITERAL:
            // No dynamic memory to free for numeric/boolean literals
            break;
    }
    
    free(node);
}

// ============================================================================
// AST Utility Functions
// ============================================================================

const char* ast_node_type_name(ast_node_type_t type) {
    switch (type) {
        case AST_CREATE_STATEMENT: return "CREATE_STATEMENT";
        case AST_MATCH_STATEMENT: return "MATCH_STATEMENT";
        case AST_RETURN_STATEMENT: return "RETURN_STATEMENT";
        case AST_COMPOUND_STATEMENT: return "COMPOUND_STATEMENT";
        case AST_NODE_PATTERN: return "NODE_PATTERN";
        case AST_VARIABLE: return "VARIABLE";
        case AST_LABEL: return "LABEL";
        case AST_PROPERTY: return "PROPERTY";
        case AST_PROPERTY_LIST: return "PROPERTY_LIST";
        case AST_STRING_LITERAL: return "STRING_LITERAL";
        case AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case AST_FLOAT_LITERAL: return "FLOAT_LITERAL";
        case AST_BOOLEAN_LITERAL: return "BOOLEAN_LITERAL";
        default: return "UNKNOWN";
    }
}

void ast_print(cypher_ast_node_t *node, int indent) {
    if (!node) {
        printf("%*sNULL\n", indent, "");
        return;
    }
    
    printf("%*s%s", indent, "", ast_node_type_name(node->type));
    
    switch (node->type) {
        case AST_VARIABLE:
            printf(": %s", node->data.variable.name);
            break;
        case AST_LABEL:
            printf(": %s", node->data.label.name);
            break;
        case AST_STRING_LITERAL:
            printf(": \"%s\"", node->data.string_literal.value);
            break;
        case AST_INTEGER_LITERAL:
            printf(": %d", node->data.integer_literal.value);
            break;
        case AST_FLOAT_LITERAL:
            printf(": %g", node->data.float_literal.value);
            break;
        case AST_BOOLEAN_LITERAL:
            printf(": %s", node->data.boolean_literal.value ? "true" : "false");
            break;
        case AST_PROPERTY:
            printf(": %s", node->data.property.key);
            break;
        default:
            // For compound nodes, print type only
            break;
    }
    
    printf("\n");
    
    // Print children
    switch (node->type) {
        case AST_CREATE_STATEMENT:
            ast_print(node->data.create_stmt.node_pattern, indent + 2);
            break;
            
        case AST_MATCH_STATEMENT:
            ast_print(node->data.match_stmt.node_pattern, indent + 2);
            break;
            
        case AST_RETURN_STATEMENT:
            ast_print(node->data.return_stmt.variable, indent + 2);
            break;
            
        case AST_COMPOUND_STATEMENT:
            ast_print(node->data.compound_stmt.match_stmt, indent + 2);
            ast_print(node->data.compound_stmt.return_stmt, indent + 2);
            break;
            
        case AST_NODE_PATTERN:
            ast_print(node->data.node_pattern.variable, indent + 2);
            ast_print(node->data.node_pattern.label, indent + 2);
            ast_print(node->data.node_pattern.properties, indent + 2);
            break;
            
        case AST_PROPERTY:
            ast_print(node->data.property.value, indent + 2);
            break;
            
        case AST_PROPERTY_LIST:
            for (int i = 0; i < node->data.property_list.count; i++) {
                ast_print(node->data.property_list.properties[i], indent + 2);
            }
            break;
            
        default:
            // Leaf nodes have no children
            break;
    }
}