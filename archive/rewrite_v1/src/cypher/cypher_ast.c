#include "cypher_ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Basic Node Creation
// ============================================================================

cypher_ast_node_t* cypher_ast_create_node(cypher_ast_node_type_t type) {
    cypher_ast_node_t *node = malloc(sizeof(cypher_ast_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(cypher_ast_node_t));
    node->type = type;
    node->owns_children = true;
    
    return node;
}

// ============================================================================
// Statement Construction
// ============================================================================

cypher_ast_node_t* cypher_ast_create_composite_statement(cypher_ast_node_t *left, cypher_ast_node_t *operator, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_COMPOSITE_STATEMENT);
    if (!node) return NULL;
    
    node->data.composite_statement.left = left;
    node->data.composite_statement.operator = operator;
    node->data.composite_statement.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_linear_statement(cypher_ast_node_t *clauses) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_LINEAR_STATEMENT);
    if (!node) return NULL;
    
    node->data.linear_statement.clauses = clauses;
    
    return node;
}

// ============================================================================
// Clause Construction
// ============================================================================

cypher_ast_node_t* cypher_ast_create_match_clause(bool optional, cypher_ast_node_t *pattern, cypher_ast_node_t *where) {
    cypher_ast_node_t *node = cypher_ast_create_node(optional ? CYPHER_AST_OPTIONAL_MATCH_CLAUSE : CYPHER_AST_MATCH_CLAUSE);
    if (!node) return NULL;
    
    node->data.match_clause.optional = optional;
    node->data.match_clause.pattern = pattern;
    node->data.match_clause.where = where;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_unwind_clause(cypher_ast_node_t *expression, cypher_ast_node_t *variable) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_UNWIND_CLAUSE);
    if (!node) return NULL;
    
    node->data.unwind_clause.expression = expression;
    node->data.unwind_clause.variable = variable;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_with_clause(bool distinct, cypher_ast_node_t *return_items, cypher_ast_node_t *order_by, cypher_ast_node_t *skip, cypher_ast_node_t *limit, cypher_ast_node_t *where) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_WITH_CLAUSE);
    if (!node) return NULL;
    
    node->data.with_clause.distinct = distinct;
    node->data.with_clause.return_items = return_items;
    node->data.with_clause.order_by = order_by;
    node->data.with_clause.skip = skip;
    node->data.with_clause.limit = limit;
    node->data.with_clause.where = where;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_return_clause(bool distinct, cypher_ast_node_t *return_items, cypher_ast_node_t *order_by, cypher_ast_node_t *skip, cypher_ast_node_t *limit) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_RETURN_CLAUSE);
    if (!node) return NULL;
    
    node->data.return_clause.distinct = distinct;
    node->data.return_clause.return_items = return_items;
    node->data.return_clause.order_by = order_by;
    node->data.return_clause.skip = skip;
    node->data.return_clause.limit = limit;
    node->data.return_clause.where = NULL; // RETURN doesn't have WHERE
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_create_clause(cypher_ast_node_t *pattern) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_CREATE_CLAUSE);
    if (!node) return NULL;
    
    node->data.create_clause.pattern = pattern;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_where_clause(cypher_ast_node_t *expression) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_WHERE_CLAUSE);
    if (!node) return NULL;
    
    node->data.where_clause.expression = expression;
    
    return node;
}

// ============================================================================
// Pattern Construction
// ============================================================================

cypher_ast_node_t* cypher_ast_create_pattern_list(cypher_ast_node_t *patterns) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PATTERN_LIST);
    if (!node) return NULL;
    
    node->data.pattern_list.patterns = patterns;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_pattern(cypher_ast_node_t *element) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PATTERN);
    if (!node) return NULL;
    
    node->data.pattern.element = element;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_pattern_element(cypher_ast_node_t *node_pattern, cypher_ast_node_t *pattern_element_chain) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PATTERN_ELEMENT);
    if (!node) return NULL;
    
    node->data.pattern_element.node = node_pattern;
    node->data.pattern_element.pattern_element_chain = pattern_element_chain;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_node_pattern(cypher_ast_node_t *variable, cypher_ast_node_t *node_labels, cypher_ast_node_t *properties) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_NODE_PATTERN);
    if (!node) return NULL;
    
    node->data.node_pattern.variable = variable;
    node->data.node_pattern.node_labels = node_labels;
    node->data.node_pattern.properties = properties;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_relationship_pattern(cypher_ast_node_t *detail, cypher_ast_node_t *target_node) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_RELATIONSHIP_PATTERN);
    if (!node) return NULL;
    
    node->data.relationship_pattern.detail = detail;
    node->data.relationship_pattern.node = target_node;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_relationship_detail(bool left_arrow, bool right_arrow, cypher_ast_node_t *variable, cypher_ast_node_t *rel_types, cypher_ast_node_t *variable_length, cypher_ast_node_t *properties) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_RELATIONSHIP_DETAIL);
    if (!node) return NULL;
    
    node->data.relationship_detail.left_arrow = left_arrow;
    node->data.relationship_detail.right_arrow = right_arrow;
    node->data.relationship_detail.variable = variable;
    node->data.relationship_detail.rel_types = rel_types;
    node->data.relationship_detail.variable_length = variable_length;
    node->data.relationship_detail.properties = properties;
    
    return node;
}

// ============================================================================
// Expression Construction
// ============================================================================

cypher_ast_node_t* cypher_ast_create_or_expression(cypher_ast_node_t *left, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_OR_EXPRESSION);
    if (!node) return NULL;
    
    node->data.or_expression.left = left;
    node->data.or_expression.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_and_expression(cypher_ast_node_t *left, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_AND_EXPRESSION);
    if (!node) return NULL;
    
    node->data.and_expression.left = left;
    node->data.and_expression.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_not_expression(cypher_ast_node_t *expression) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_NOT_EXPRESSION);
    if (!node) return NULL;
    
    node->data.not_expression.expression = expression;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_comparison_expression(cypher_ast_node_t *left, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_COMPARISON_EXPRESSION);
    if (!node) return NULL;
    
    node->data.comparison_expression.left = left;
    node->data.comparison_expression.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_add_expression(cypher_ast_node_t *left, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_ADD_EXPRESSION);
    if (!node) return NULL;
    
    node->data.add_expression.left = left;
    node->data.add_expression.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_multiply_expression(cypher_ast_node_t *left, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_MULTIPLY_EXPRESSION);
    if (!node) return NULL;
    
    node->data.multiply_expression.left = left;
    node->data.multiply_expression.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_power_expression(cypher_ast_node_t *left, cypher_ast_node_t *right) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_POWER_EXPRESSION);
    if (!node) return NULL;
    
    node->data.power_expression.left = left;
    node->data.power_expression.right = right;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_postfix_expression(cypher_ast_node_t *atom, cypher_ast_node_t *property_lookups, cypher_ast_node_t *node_labels) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_POSTFIX_EXPRESSION);
    if (!node) return NULL;
    
    node->data.postfix_expression.atom = atom;
    node->data.postfix_expression.property_lookups = property_lookups;
    node->data.postfix_expression.node_labels = node_labels;
    
    return node;
}

// ============================================================================
// Literal Construction
// ============================================================================

cypher_ast_node_t* cypher_ast_create_null_literal(void) {
    return cypher_ast_create_node(CYPHER_AST_NULL_LITERAL);
}

cypher_ast_node_t* cypher_ast_create_boolean_literal(bool value) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_BOOLEAN_LITERAL);
    if (!node) return NULL;
    
    node->data.boolean_literal.value = value;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_number_literal(const char *value) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_NUMBER_LITERAL);
    if (!node) return NULL;
    
    node->data.number_literal.value = strdup(value);
    
    // Determine if it's integer or float
    node->data.number_literal.is_integer = (strchr(value, '.') == NULL && strchr(value, 'e') == NULL && strchr(value, 'E') == NULL);
    node->data.number_literal.is_float = !node->data.number_literal.is_integer;
    
    // Parse numeric value
    if (node->data.number_literal.is_integer) {
        node->data.number_literal.numeric.integer_val = strtoll(value, NULL, 0);
    } else {
        node->data.number_literal.numeric.float_val = strtod(value, NULL);
    }
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_string_literal(const char *value) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_STRING_LITERAL);
    if (!node) return NULL;
    
    node->data.string_literal.value = strdup(value);
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_map_literal(cypher_ast_node_t *properties) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_MAP_LITERAL);
    if (!node) return NULL;
    
    node->data.map_literal.properties = properties;
    
    return node;
}

// ============================================================================
// Variable and Name Construction
// ============================================================================

cypher_ast_node_t* cypher_ast_create_variable(const char *name) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_VARIABLE);
    if (!node) return NULL;
    
    node->data.variable.name = strdup(name);
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_binding_variable(const char *name) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_BINDING_VARIABLE);
    if (!node) return NULL;
    
    node->data.binding_variable.name = strdup(name);
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_property_key_name(const char *name) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PROPERTY_KEY_NAME);
    if (!node) return NULL;
    
    node->data.property_key_name.name = strdup(name);
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_label_name(const char *name) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_LABEL_NAME);
    if (!node) return NULL;
    
    node->data.label_name.name = strdup(name);
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_relationship_type(const char *name) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_RELATIONSHIP_TYPE);
    if (!node) return NULL;
    
    node->data.relationship_type.name = strdup(name);
    
    return node;
}

// ============================================================================
// Property Access and Functions
// ============================================================================

cypher_ast_node_t* cypher_ast_create_property_access(cypher_ast_node_t *expression, cypher_ast_node_t *property_key) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PROPERTY_ACCESS);
    if (!node) return NULL;
    
    node->data.property_access.expression = expression;
    node->data.property_access.property_key = property_key;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_property_lookup(cypher_ast_node_t *expression) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PROPERTY_LOOKUP);
    if (!node) return NULL;
    
    node->data.property_lookup.expression = expression;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_node_labels(cypher_ast_node_t *labels) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_NODE_LABELS);
    if (!node) return NULL;
    
    node->data.node_labels.labels = labels;
    
    return node;
}

// ============================================================================
// Return Items
// ============================================================================

cypher_ast_node_t* cypher_ast_create_return_items(cypher_ast_node_t *items) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_RETURN_ITEMS);
    if (!node) return NULL;
    
    node->data.return_items.items = items;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_return_item(cypher_ast_node_t *expression, cypher_ast_node_t *variable) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_RETURN_ITEM);
    if (!node) return NULL;
    
    node->data.return_item.expression = expression;
    node->data.return_item.variable = variable;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_projection_item(cypher_ast_node_t *expression, cypher_ast_node_t *variable) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_PROJECTION_ITEM);
    if (!node) return NULL;
    
    node->data.projection_item.expression = expression;
    node->data.projection_item.variable = variable;
    
    return node;
}

// ============================================================================
// Operators
// ============================================================================

cypher_ast_node_t* cypher_ast_create_union_operator(bool all) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_UNION_OPERATOR);
    if (!node) return NULL;
    
    node->data.union_operator.all = all;
    
    return node;
}

cypher_ast_node_t* cypher_ast_create_comparison_operator(const char *operator) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_COMPARISON_OPERATOR);
    if (!node) return NULL;
    
    node->data.comparison_operator.operator = strdup(operator);
    
    return node;
}

// ============================================================================
// List Operations
// ============================================================================

cypher_ast_node_t* cypher_ast_create_expression_list(void) {
    cypher_ast_node_t *node = cypher_ast_create_node(CYPHER_AST_EXPRESSION_LIST);
    if (!node) return NULL;
    
    node->data.expression_list.expressions = NULL;
    node->data.expression_list.count = 0;
    node->data.expression_list.capacity = 0;
    
    return node;
}

void cypher_ast_list_append(cypher_ast_node_t *list, cypher_ast_node_t *item) {
    if (!list || list->type != CYPHER_AST_EXPRESSION_LIST || !item) return;
    
    // Grow array if needed
    if (list->data.expression_list.count >= list->data.expression_list.capacity) {
        size_t new_capacity = list->data.expression_list.capacity == 0 ? 4 : list->data.expression_list.capacity * 2;
        cypher_ast_node_t **new_array = realloc(list->data.expression_list.expressions, 
                                                new_capacity * sizeof(cypher_ast_node_t*));
        if (!new_array) return;
        
        list->data.expression_list.expressions = new_array;
        list->data.expression_list.capacity = new_capacity;
    }
    
    list->data.expression_list.expressions[list->data.expression_list.count++] = item;
}

size_t cypher_ast_list_length(cypher_ast_node_t *list) {
    if (!list || list->type != CYPHER_AST_EXPRESSION_LIST) return 0;
    return list->data.expression_list.count;
}

cypher_ast_node_t* cypher_ast_list_get(cypher_ast_node_t *list, size_t index) {
    if (!list || list->type != CYPHER_AST_EXPRESSION_LIST || index >= list->data.expression_list.count) return NULL;
    return list->data.expression_list.expressions[index];
}

// ============================================================================
// Memory Management
// ============================================================================

static bool is_freeing = false;
static void* currently_freeing[100];
static int free_count = 0;

void cypher_ast_free_recursive(cypher_ast_node_t *node) {
    if (!node) return;
    
    // Check for double-free
    for (int i = 0; i < free_count; i++) {
        if (currently_freeing[i] == node) {
            printf("DEBUG: DOUBLE FREE DETECTED! Pointer %p already freed\n", (void*)node);
            return;
        }
    }
    
    // Track this pointer as being freed
    if (free_count < 100) {
        currently_freeing[free_count++] = node;
    }
    
    printf("DEBUG: Freeing AST node %p, type: %d\n", (void*)node, node->type);
    
    // Free children first
    switch (node->type) {
        case CYPHER_AST_COMPOSITE_STATEMENT:
            printf("DEBUG: COMPOSITE_STATEMENT left: %p, operator: %p, right: %p\n", 
                   (void*)node->data.composite_statement.left,
                   (void*)node->data.composite_statement.operator,
                   (void*)node->data.composite_statement.right);
            cypher_ast_free_recursive(node->data.composite_statement.left);
            cypher_ast_free_recursive(node->data.composite_statement.operator);
            cypher_ast_free_recursive(node->data.composite_statement.right);
            break;
            
        case CYPHER_AST_LINEAR_STATEMENT:
            printf("DEBUG: LINEAR_STATEMENT clauses pointer: %p\n", (void*)node->data.linear_statement.clauses);
            cypher_ast_free_recursive(node->data.linear_statement.clauses);
            break;
            
        case CYPHER_AST_MATCH_CLAUSE:
        case CYPHER_AST_OPTIONAL_MATCH_CLAUSE:
            cypher_ast_free_recursive(node->data.match_clause.pattern);
            cypher_ast_free_recursive(node->data.match_clause.where);
            break;
            
        case CYPHER_AST_EXPRESSION_LIST:
            for (size_t i = 0; i < node->data.expression_list.count; i++) {
                cypher_ast_free_recursive(node->data.expression_list.expressions[i]);
            }
            free(node->data.expression_list.expressions);
            break;
            
        case CYPHER_AST_STRING_LITERAL:
            if (node->data.string_literal.value && 
                node->data.string_literal.value != (char*)0xdeadbeef) {
                free(node->data.string_literal.value);
            }
            break;
            
        case CYPHER_AST_NUMBER_LITERAL:
            if (node->data.number_literal.value && 
                node->data.number_literal.value != (char*)0xdeadbeef) {
                free(node->data.number_literal.value);
            }
            break;
            
        case CYPHER_AST_VARIABLE:
            if (node->data.variable.name && 
                node->data.variable.name != (char*)0xdeadbeef) {
                free(node->data.variable.name);
            }
            break;
            
        case CYPHER_AST_BINDING_VARIABLE:
            if (node->data.binding_variable.name && 
                node->data.binding_variable.name != (char*)0xdeadbeef) {
                free(node->data.binding_variable.name);
            }
            break;
            
        case CYPHER_AST_PROPERTY_KEY_NAME:
            if (node->data.property_key_name.name && 
                node->data.property_key_name.name != (char*)0xdeadbeef) {
                free(node->data.property_key_name.name);
            }
            break;
            
        case CYPHER_AST_LABEL_NAME:
            if (node->data.label_name.name && 
                node->data.label_name.name != (char*)0xdeadbeef) {
                free(node->data.label_name.name);
            }
            break;
            
        case CYPHER_AST_RELATIONSHIP_TYPE:
            if (node->data.relationship_type.name && 
                node->data.relationship_type.name != (char*)0xdeadbeef) {
                free(node->data.relationship_type.name);
            }
            break;
            
        case CYPHER_AST_COMPARISON_OPERATOR:
            if (node->data.comparison_operator.operator && 
                node->data.comparison_operator.operator != (char*)0xdeadbeef) {
                free(node->data.comparison_operator.operator);
            }
            break;
            
        case CYPHER_AST_ADD_EXPRESSION:
            cypher_ast_free_recursive(node->data.add_expression.left);
            cypher_ast_free_recursive(node->data.add_expression.right);
            break;
            
        case CYPHER_AST_MULTIPLY_EXPRESSION:
            cypher_ast_free_recursive(node->data.multiply_expression.left);
            cypher_ast_free_recursive(node->data.multiply_expression.right);
            break;
            
        case CYPHER_AST_POWER_EXPRESSION:
            cypher_ast_free_recursive(node->data.power_expression.left);
            cypher_ast_free_recursive(node->data.power_expression.right);
            break;
            
        // Add more cases as needed...
        default:
            // For other node types, we'd need to add specific cleanup
            break;
    }
    
    // Free the node itself
    free(node);
}

void cypher_ast_free(cypher_ast_node_t *node) {
    cypher_ast_free_recursive(node);
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* cypher_ast_node_type_name(cypher_ast_node_type_t type) {
    switch (type) {
        case CYPHER_AST_COMPOSITE_STATEMENT: return "COMPOSITE_STATEMENT";
        case CYPHER_AST_LINEAR_STATEMENT: return "LINEAR_STATEMENT";
        case CYPHER_AST_MATCH_CLAUSE: return "MATCH_CLAUSE";
        case CYPHER_AST_OPTIONAL_MATCH_CLAUSE: return "OPTIONAL_MATCH_CLAUSE";
        case CYPHER_AST_RETURN_CLAUSE: return "RETURN_CLAUSE";
        case CYPHER_AST_CREATE_CLAUSE: return "CREATE_CLAUSE";
        case CYPHER_AST_WHERE_CLAUSE: return "WHERE_CLAUSE";
        case CYPHER_AST_PATTERN_LIST: return "PATTERN_LIST";
        case CYPHER_AST_PATTERN: return "PATTERN";
        case CYPHER_AST_NODE_PATTERN: return "NODE_PATTERN";
        case CYPHER_AST_RELATIONSHIP_PATTERN: return "RELATIONSHIP_PATTERN";
        case CYPHER_AST_OR_EXPRESSION: return "OR_EXPRESSION";
        case CYPHER_AST_AND_EXPRESSION: return "AND_EXPRESSION";
        case CYPHER_AST_NOT_EXPRESSION: return "NOT_EXPRESSION";
        case CYPHER_AST_COMPARISON_EXPRESSION: return "COMPARISON_EXPRESSION";
        case CYPHER_AST_ADD_EXPRESSION: return "ADD_EXPRESSION";
        case CYPHER_AST_MULTIPLY_EXPRESSION: return "MULTIPLY_EXPRESSION";
        case CYPHER_AST_POWER_EXPRESSION: return "POWER_EXPRESSION";
        case CYPHER_AST_NULL_LITERAL: return "NULL_LITERAL";
        case CYPHER_AST_BOOLEAN_LITERAL: return "BOOLEAN_LITERAL";
        case CYPHER_AST_NUMBER_LITERAL: return "NUMBER_LITERAL";
        case CYPHER_AST_STRING_LITERAL: return "STRING_LITERAL";
        case CYPHER_AST_VARIABLE: return "VARIABLE";
        case CYPHER_AST_BINDING_VARIABLE: return "BINDING_VARIABLE";
        case CYPHER_AST_PROPERTY_KEY_NAME: return "PROPERTY_KEY_NAME";
        case CYPHER_AST_LABEL_NAME: return "LABEL_NAME";
        case CYPHER_AST_RELATIONSHIP_TYPE: return "RELATIONSHIP_TYPE";
        case CYPHER_AST_EXPRESSION_LIST: return "EXPRESSION_LIST";
        default: return "UNKNOWN";
    }
}

void cypher_ast_print(cypher_ast_node_t *node, int indent) {
    if (!node) {
        printf("%*sNULL\n", indent, "");
        return;
    }
    
    printf("%*s%s", indent, "", cypher_ast_node_type_name(node->type));
    
    switch (node->type) {
        case CYPHER_AST_STRING_LITERAL:
            printf(": \"%s\"", node->data.string_literal.value);
            break;
        case CYPHER_AST_NUMBER_LITERAL:
            printf(": %s", node->data.number_literal.value);
            break;
        case CYPHER_AST_BOOLEAN_LITERAL:
            printf(": %s", node->data.boolean_literal.value ? "true" : "false");
            break;
        case CYPHER_AST_VARIABLE:
            printf(": %s", node->data.variable.name);
            break;
        case CYPHER_AST_BINDING_VARIABLE:
            printf(": %s", node->data.binding_variable.name);
            break;
        case CYPHER_AST_PROPERTY_KEY_NAME:
            printf(": %s", node->data.property_key_name.name);
            break;
        case CYPHER_AST_LABEL_NAME:
            printf(": %s", node->data.label_name.name);
            break;
        case CYPHER_AST_RELATIONSHIP_TYPE:
            printf(": %s", node->data.relationship_type.name);
            break;
        default:
            // For complex nodes, just print the type
            break;
    }
    
    printf("\n");
    
    // Print children
    switch (node->type) {
        case CYPHER_AST_COMPOSITE_STATEMENT:
            cypher_ast_print(node->data.composite_statement.left, indent + 2);
            cypher_ast_print(node->data.composite_statement.operator, indent + 2);
            cypher_ast_print(node->data.composite_statement.right, indent + 2);
            break;
            
        case CYPHER_AST_LINEAR_STATEMENT:
            cypher_ast_print(node->data.linear_statement.clauses, indent + 2);
            break;
            
        case CYPHER_AST_MATCH_CLAUSE:
        case CYPHER_AST_OPTIONAL_MATCH_CLAUSE:
            cypher_ast_print(node->data.match_clause.pattern, indent + 2);
            cypher_ast_print(node->data.match_clause.where, indent + 2);
            break;
            
        case CYPHER_AST_EXPRESSION_LIST:
            for (size_t i = 0; i < node->data.expression_list.count; i++) {
                cypher_ast_print(node->data.expression_list.expressions[i], indent + 2);
            }
            break;
            
        // Add more cases as needed...
        default:
            break;
    }
}