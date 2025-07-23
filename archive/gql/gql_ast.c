#include "gql_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Helper Functions
// ============================================================================

static gql_ast_node_t* create_node(gql_ast_node_type_t type) {
    gql_ast_node_t *node = calloc(1, sizeof(gql_ast_node_t));
    if (!node) return NULL;
    
    node->type = type;
    node->owns_strings = true;
    node->next = NULL;
    
    return node;
}

static char* safe_strdup(const char *str) {
    if (!str) return NULL;
    return strdup(str);
}

// ============================================================================
// Node Creation Functions
// ============================================================================

gql_ast_node_t* gql_ast_create_match_query(gql_ast_node_t *patterns, 
                                           gql_ast_node_t *where_clause,
                                           gql_ast_node_t *return_clause) {
    gql_ast_node_t *node = create_node(GQL_AST_MATCH_QUERY);
    if (!node) return NULL;
    
    node->data.match_query.patterns = patterns;
    node->data.match_query.where_clause = where_clause;
    node->data.match_query.return_clause = return_clause;
    
    return node;
}

gql_ast_node_t* gql_ast_create_create_query(gql_ast_node_t *patterns) {
    gql_ast_node_t *node = create_node(GQL_AST_CREATE_QUERY);
    if (!node) return NULL;
    
    node->data.create_query.patterns = patterns;
    
    return node;
}

gql_ast_node_t* gql_ast_create_set_query(gql_ast_node_t *patterns,
                                         gql_ast_node_t *where_clause,
                                         gql_ast_node_t *assignments) {
    gql_ast_node_t *node = create_node(GQL_AST_SET_QUERY);
    if (!node) return NULL;
    
    node->data.set_query.patterns = patterns;
    node->data.set_query.where_clause = where_clause;
    node->data.set_query.assignments = assignments;
    
    return node;
}

gql_ast_node_t* gql_ast_create_delete_query(gql_ast_node_t *patterns,
                                            gql_ast_node_t *where_clause,
                                            gql_ast_node_t *identifiers) {
    gql_ast_node_t *node = create_node(GQL_AST_DELETE_QUERY);
    if (!node) return NULL;
    
    node->data.delete_query.patterns = patterns;
    node->data.delete_query.where_clause = where_clause;
    node->data.delete_query.identifiers = identifiers;
    
    return node;
}

gql_ast_node_t* gql_ast_create_pattern(gql_ast_node_t *node_pattern,
                                      gql_ast_node_t *edge,
                                      gql_ast_node_t *target_node) {
    gql_ast_node_t *node = create_node(GQL_AST_PATTERN);
    if (!node) return NULL;
    
    node->data.pattern.node = node_pattern;
    node->data.pattern.edge = edge;
    node->data.pattern.target_node = target_node;
    
    return node;
}

gql_ast_node_t* gql_ast_create_node_pattern(const char *variable,
                                           gql_ast_node_t *labels,
                                           gql_ast_node_t *properties) {
    gql_ast_node_t *node = create_node(GQL_AST_NODE_PATTERN);
    if (!node) return NULL;
    
    node->data.node_pattern.variable = safe_strdup(variable);
    node->data.node_pattern.labels = labels;
    node->data.node_pattern.properties = properties;
    
    return node;
}

gql_ast_node_t* gql_ast_create_edge_pattern(const char *variable,
                                           const char *type,
                                           gql_ast_node_t *properties,
                                           bool directed) {
    gql_ast_node_t *node = create_node(GQL_AST_EDGE_PATTERN);
    if (!node) return NULL;
    
    node->data.edge_pattern.variable = safe_strdup(variable);
    node->data.edge_pattern.type = safe_strdup(type);
    node->data.edge_pattern.properties = properties;
    node->data.edge_pattern.directed = directed;
    
    return node;
}

gql_ast_node_t* gql_ast_create_binary_expr(gql_operator_t operator,
                                          gql_ast_node_t *left,
                                          gql_ast_node_t *right) {
    gql_ast_node_t *node = create_node(GQL_AST_BINARY_EXPR);
    if (!node) return NULL;
    
    node->data.binary_expr.operator = operator;
    node->data.binary_expr.left = left;
    node->data.binary_expr.right = right;
    
    return node;
}

gql_ast_node_t* gql_ast_create_unary_expr(gql_operator_t operator,
                                         gql_ast_node_t *operand) {
    gql_ast_node_t *node = create_node(GQL_AST_UNARY_EXPR);
    if (!node) return NULL;
    
    node->data.unary_expr.operator = operator;
    node->data.unary_expr.operand = operand;
    
    return node;
}

gql_ast_node_t* gql_ast_create_property_access(const char *object,
                                              const char *property) {
    gql_ast_node_t *node = create_node(GQL_AST_PROPERTY_ACCESS);
    if (!node) return NULL;
    
    node->data.property_access.object = safe_strdup(object);
    node->data.property_access.property = safe_strdup(property);
    
    return node;
}

gql_ast_node_t* gql_ast_create_identifier(const char *name) {
    gql_ast_node_t *node = create_node(GQL_AST_IDENTIFIER);
    if (!node) return NULL;
    
    node->data.identifier.name = safe_strdup(name);
    
    return node;
}

gql_ast_node_t* gql_ast_create_return_item(gql_ast_node_t *expression, const char *alias) {
    gql_ast_node_t *node = create_node(GQL_AST_RETURN_ITEM);
    if (!node) return NULL;
    
    node->data.return_item.expression = expression;
    node->data.return_item.alias = alias ? safe_strdup(alias) : NULL;
    
    return node;
}

gql_ast_node_t* gql_ast_create_string_literal(const char *value) {
    gql_ast_node_t *node = create_node(GQL_AST_STRING_LITERAL);
    if (!node) return NULL;
    
    node->data.string_literal.value = safe_strdup(value);
    
    return node;
}

gql_ast_node_t* gql_ast_create_integer_literal(int64_t value) {
    gql_ast_node_t *node = create_node(GQL_AST_INTEGER_LITERAL);
    if (!node) return NULL;
    
    node->data.integer_literal.value = value;
    
    return node;
}

gql_ast_node_t* gql_ast_create_boolean_literal(bool value) {
    gql_ast_node_t *node = create_node(GQL_AST_BOOLEAN_LITERAL);
    if (!node) return NULL;
    
    node->data.boolean_literal.value = value;
    
    return node;
}

gql_ast_node_t* gql_ast_create_null_literal(void) {
    return create_node(GQL_AST_NULL_LITERAL);
}

// ============================================================================
// List Operations
// ============================================================================

gql_ast_node_t* gql_ast_create_list(gql_ast_node_type_t type) {
    gql_ast_node_t *node = create_node(type);
    if (!node) return NULL;
    
    // Lists start with no items
    node->next = NULL;
    
    return node;
}

void gql_ast_list_append(gql_ast_node_t *list, gql_ast_node_t *item) {
    if (!list || !item) return;
    
    if (!list->next) {
        list->next = item;
    } else {
        gql_ast_node_t *current = list->next;
        while (current->next) {
            current = current->next;
        }
        current->next = item;
    }
}

size_t gql_ast_list_length(gql_ast_node_t *list) {
    if (!list) return 0;
    
    size_t count = 0;
    gql_ast_node_t *current = list->next;
    while (current) {
        count++;
        current = current->next;
    }
    
    return count;
}

// ============================================================================
// Memory Management
// ============================================================================

void gql_ast_free(gql_ast_node_t *node) {
    if (!node) return;
    
    if (node->owns_strings) {
        switch (node->type) {
            case GQL_AST_NODE_PATTERN:
                free(node->data.node_pattern.variable);
                gql_ast_free_recursive(node->data.node_pattern.labels);
                break;
            case GQL_AST_EDGE_PATTERN:
                free(node->data.edge_pattern.variable);
                free(node->data.edge_pattern.type);
                break;
            case GQL_AST_PROPERTY_ACCESS:
                free(node->data.property_access.object);
                free(node->data.property_access.property);
                break;
            case GQL_AST_IDENTIFIER:
                free(node->data.identifier.name);
                break;
            case GQL_AST_RETURN_ITEM:
                free(node->data.return_item.alias);
                break;
            case GQL_AST_STRING_LITERAL:
                free(node->data.string_literal.value);
                break;
            default:
                break;
        }
    }
    
    free(node);
}

void gql_ast_free_recursive(gql_ast_node_t *node) {
    if (!node) return;
    
    // Free next node in list
    if (node->next) {
        gql_ast_free_recursive(node->next);
    }
    
    // Free child nodes based on type
    switch (node->type) {
        case GQL_AST_MATCH_QUERY:
            gql_ast_free_recursive(node->data.match_query.patterns);
            gql_ast_free_recursive(node->data.match_query.where_clause);
            gql_ast_free_recursive(node->data.match_query.return_clause);
            break;
        case GQL_AST_CREATE_QUERY:
            gql_ast_free_recursive(node->data.create_query.patterns);
            break;
        case GQL_AST_SET_QUERY:
            gql_ast_free_recursive(node->data.set_query.patterns);
            gql_ast_free_recursive(node->data.set_query.where_clause);
            gql_ast_free_recursive(node->data.set_query.assignments);
            break;
        case GQL_AST_DELETE_QUERY:
            gql_ast_free_recursive(node->data.delete_query.patterns);
            gql_ast_free_recursive(node->data.delete_query.where_clause);
            gql_ast_free_recursive(node->data.delete_query.identifiers);
            break;
        case GQL_AST_PATTERN:
            gql_ast_free_recursive(node->data.pattern.node);
            gql_ast_free_recursive(node->data.pattern.edge);
            gql_ast_free_recursive(node->data.pattern.target_node);
            break;
        case GQL_AST_NODE_PATTERN:
            gql_ast_free_recursive(node->data.node_pattern.properties);
            break;
        case GQL_AST_EDGE_PATTERN:
            gql_ast_free_recursive(node->data.edge_pattern.properties);
            break;
        case GQL_AST_BINARY_EXPR:
            gql_ast_free_recursive(node->data.binary_expr.left);
            gql_ast_free_recursive(node->data.binary_expr.right);
            break;
        case GQL_AST_UNARY_EXPR:
            gql_ast_free_recursive(node->data.unary_expr.operand);
            break;
        case GQL_AST_WHERE_CLAUSE:
            gql_ast_free_recursive(node->data.where_clause.expression);
            break;
        case GQL_AST_RETURN_CLAUSE:
            gql_ast_free_recursive(node->data.return_clause.items);
            break;
        case GQL_AST_RETURN_ITEM:
            gql_ast_free_recursive(node->data.return_item.expression);
            break;
        case GQL_AST_SET_CLAUSE:
            gql_ast_free_recursive(node->data.set_clause.assignments);
            break;
        default:
            break;
    }
    
    gql_ast_free(node);
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* gql_ast_node_type_name(gql_ast_node_type_t type) {
    switch (type) {
        case GQL_AST_MATCH_QUERY: return "MATCH_QUERY";
        case GQL_AST_CREATE_QUERY: return "CREATE_QUERY";
        case GQL_AST_SET_QUERY: return "SET_QUERY";
        case GQL_AST_DELETE_QUERY: return "DELETE_QUERY";
        case GQL_AST_PATTERN: return "PATTERN";
        case GQL_AST_NODE_PATTERN: return "NODE_PATTERN";
        case GQL_AST_EDGE_PATTERN: return "EDGE_PATTERN";
        case GQL_AST_BINARY_EXPR: return "BINARY_EXPR";
        case GQL_AST_UNARY_EXPR: return "UNARY_EXPR";
        case GQL_AST_PROPERTY_ACCESS: return "PROPERTY_ACCESS";
        case GQL_AST_LITERAL: return "LITERAL";
        case GQL_AST_IDENTIFIER: return "IDENTIFIER";
        case GQL_AST_WHERE_CLAUSE: return "WHERE_CLAUSE";
        case GQL_AST_RETURN_CLAUSE: return "RETURN_CLAUSE";
        case GQL_AST_RETURN_ITEM: return "RETURN_ITEM";
        case GQL_AST_SET_CLAUSE: return "SET_CLAUSE";
        case GQL_AST_STRING_LITERAL: return "STRING_LITERAL";
        case GQL_AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case GQL_AST_BOOLEAN_LITERAL: return "BOOLEAN_LITERAL";
        case GQL_AST_NULL_LITERAL: return "NULL_LITERAL";
        default: return "UNKNOWN";
    }
}

const char* gql_operator_name(gql_operator_t op) {
    switch (op) {
        case GQL_OP_AND: return "AND";
        case GQL_OP_OR: return "OR";
        case GQL_OP_NOT: return "NOT";
        case GQL_OP_EQUALS: return "=";
        case GQL_OP_NOT_EQUALS: return "<>";
        case GQL_OP_LESS_THAN: return "<";
        case GQL_OP_LESS_EQUAL: return "<=";
        case GQL_OP_GREATER_THAN: return ">";
        case GQL_OP_GREATER_EQUAL: return ">=";
        case GQL_OP_IS_NULL: return "IS NULL";
        case GQL_OP_IS_NOT_NULL: return "IS NOT NULL";
        case GQL_OP_STARTS_WITH: return "STARTS WITH";
        case GQL_OP_ENDS_WITH: return "ENDS WITH";
        case GQL_OP_CONTAINS: return "CONTAINS";
        default: return "UNKNOWN";
    }
}

void gql_ast_print(gql_ast_node_t *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    printf("%s", gql_ast_node_type_name(node->type));
    
    switch (node->type) {
        case GQL_AST_NODE_PATTERN:
            if (node->data.node_pattern.variable) {
                printf(" var:%s", node->data.node_pattern.variable);
            }
            if (node->data.node_pattern.labels) {
                printf(" labels:[");
                gql_ast_node_t *current = node->data.node_pattern.labels;
                bool first = true;
                while (current) {
                    if (!first) printf(" & ");
                    if (current->type == GQL_AST_STRING_LITERAL) {
                        printf("%s", current->data.string_literal.value);
                    }
                    first = false;
                    current = current->next;
                }
                printf("]");
            }
            break;
        case GQL_AST_EDGE_PATTERN:
            if (node->data.edge_pattern.variable) {
                printf(" var:%s", node->data.edge_pattern.variable);
            }
            if (node->data.edge_pattern.type) {
                printf(" type:%s", node->data.edge_pattern.type);
            }
            printf(" directed:%s", node->data.edge_pattern.directed ? "true" : "false");
            break;
        case GQL_AST_PROPERTY_ACCESS:
            printf(" %s.%s", 
                   node->data.property_access.object ? node->data.property_access.object : "?",
                   node->data.property_access.property ? node->data.property_access.property : "?");
            break;
        case GQL_AST_IDENTIFIER:
            printf(" %s", node->data.identifier.name ? node->data.identifier.name : "?");
            break;
        case GQL_AST_STRING_LITERAL:
            printf(" \"%s\"", node->data.string_literal.value ? node->data.string_literal.value : "");
            break;
        case GQL_AST_INTEGER_LITERAL:
            printf(" %lld", (long long)node->data.integer_literal.value);
            break;
        case GQL_AST_BOOLEAN_LITERAL:
            printf(" %s", node->data.boolean_literal.value ? "true" : "false");
            break;
        case GQL_AST_BINARY_EXPR:
            printf(" %s", gql_operator_name(node->data.binary_expr.operator));
            break;
        case GQL_AST_UNARY_EXPR:
            printf(" %s", gql_operator_name(node->data.unary_expr.operator));
            break;
        default:
            break;
    }
    
    printf("\n");
    
    // Print child nodes
    switch (node->type) {
        case GQL_AST_MATCH_QUERY:
            if (node->data.match_query.patterns) {
                gql_ast_print(node->data.match_query.patterns, indent + 1);
            }
            if (node->data.match_query.where_clause) {
                gql_ast_print(node->data.match_query.where_clause, indent + 1);
            }
            if (node->data.match_query.return_clause) {
                gql_ast_print(node->data.match_query.return_clause, indent + 1);
            }
            break;
        case GQL_AST_PATTERN:
            if (node->data.pattern.node) {
                gql_ast_print(node->data.pattern.node, indent + 1);
            }
            if (node->data.pattern.edge) {
                gql_ast_print(node->data.pattern.edge, indent + 1);
            }
            if (node->data.pattern.target_node) {
                gql_ast_print(node->data.pattern.target_node, indent + 1);
            }
            break;
        case GQL_AST_BINARY_EXPR:
            if (node->data.binary_expr.left) {
                gql_ast_print(node->data.binary_expr.left, indent + 1);
            }
            if (node->data.binary_expr.right) {
                gql_ast_print(node->data.binary_expr.right, indent + 1);
            }
            break;
        case GQL_AST_UNARY_EXPR:
            if (node->data.unary_expr.operand) {
                gql_ast_print(node->data.unary_expr.operand, indent + 1);
            }
            break;
        default:
            break;
    }
    
    // Print next item in list
    if (node->next) {
        gql_ast_print(node->next, indent);
    }
}