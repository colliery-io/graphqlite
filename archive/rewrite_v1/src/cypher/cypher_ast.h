#ifndef CYPHER_AST_H
#define CYPHER_AST_H

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
    // Top-level statements
    CYPHER_AST_COMPOSITE_STATEMENT,
    CYPHER_AST_LINEAR_STATEMENT,
    
    // Clauses
    CYPHER_AST_MATCH_CLAUSE,
    CYPHER_AST_OPTIONAL_MATCH_CLAUSE,
    CYPHER_AST_UNWIND_CLAUSE,
    CYPHER_AST_WITH_CLAUSE,
    CYPHER_AST_RETURN_CLAUSE,
    CYPHER_AST_CREATE_CLAUSE,
    CYPHER_AST_MERGE_CLAUSE,
    CYPHER_AST_SET_CLAUSE,
    CYPHER_AST_REMOVE_CLAUSE,
    CYPHER_AST_DELETE_CLAUSE,
    CYPHER_AST_CALL_CLAUSE,
    CYPHER_AST_WHERE_CLAUSE,
    CYPHER_AST_ORDER_BY_CLAUSE,
    CYPHER_AST_SKIP_CLAUSE,
    CYPHER_AST_LIMIT_CLAUSE,
    
    // Patterns
    CYPHER_AST_PATTERN_LIST,
    CYPHER_AST_PATTERN,
    CYPHER_AST_PATTERN_ELEMENT,
    CYPHER_AST_NODE_PATTERN,
    CYPHER_AST_RELATIONSHIP_PATTERN,
    CYPHER_AST_RELATIONSHIP_DETAIL,
    CYPHER_AST_VARIABLE_LENGTH,
    
    // Expressions
    CYPHER_AST_OR_EXPRESSION,
    CYPHER_AST_XOR_EXPRESSION,
    CYPHER_AST_AND_EXPRESSION,
    CYPHER_AST_NOT_EXPRESSION,
    CYPHER_AST_COMPARISON_EXPRESSION,
    CYPHER_AST_PARTIAL_COMPARISON_EXPRESSION,
    CYPHER_AST_ADD_EXPRESSION,
    CYPHER_AST_MULTIPLY_EXPRESSION,
    CYPHER_AST_POWER_EXPRESSION,
    CYPHER_AST_UNARY_ADD_EXPRESSION,
    CYPHER_AST_POSTFIX_EXPRESSION,
    CYPHER_AST_ATOM,
    
    // Literals and values
    CYPHER_AST_NULL_LITERAL,
    CYPHER_AST_BOOLEAN_LITERAL,
    CYPHER_AST_NUMBER_LITERAL,
    CYPHER_AST_STRING_LITERAL,
    CYPHER_AST_LIST_LITERAL,
    CYPHER_AST_MAP_LITERAL,
    CYPHER_AST_PARAMETER,
    CYPHER_AST_VARIABLE,
    
    // Property access and function calls
    CYPHER_AST_PROPERTY_ACCESS,
    CYPHER_AST_PROPERTY_LOOKUP,
    CYPHER_AST_NODE_LABELS,
    CYPHER_AST_FUNCTION_INVOCATION,
    CYPHER_AST_COUNT_EXPRESSION,
    CYPHER_AST_EXISTS_EXPRESSION,
    
    // Control flow
    CYPHER_AST_CASE_EXPRESSION,
    CYPHER_AST_CASE_ALTERNATIVE,
    
    // Comprehensions
    CYPHER_AST_LIST_COMPREHENSION,
    CYPHER_AST_PATTERN_COMPREHENSION,
    CYPHER_AST_FILTER_EXPRESSION,
    CYPHER_AST_EXTRACT_EXPRESSION,
    CYPHER_AST_REDUCE_EXPRESSION,
    CYPHER_AST_ALL_EXPRESSION,
    CYPHER_AST_ANY_EXPRESSION,
    CYPHER_AST_NONE_EXPRESSION,
    CYPHER_AST_SINGLE_EXPRESSION,
    
    // Path patterns
    CYPHER_AST_SHORTEST_PATH_PATTERN,
    CYPHER_AST_ALL_SHORTEST_PATHS_PATTERN,
    
    // Operators
    CYPHER_AST_UNION_OPERATOR,
    CYPHER_AST_COMPARISON_OPERATOR,
    CYPHER_AST_PARTIAL_COMPARISON_OPERATOR,
    
    // Other constructs
    CYPHER_AST_RETURN_ITEMS,
    CYPHER_AST_RETURN_ITEM,
    CYPHER_AST_PROJECTION_ITEM,
    CYPHER_AST_SORT_ITEM,
    CYPHER_AST_PROPERTY_KEY_NAME,
    CYPHER_AST_LABEL_NAME,
    CYPHER_AST_RELATIONSHIP_TYPE,
    CYPHER_AST_BINDING_VARIABLE,
    CYPHER_AST_SYMBOLIC_NAME,
    CYPHER_AST_NAMESPACE,
    CYPHER_AST_PROCEDURE_NAME,
    CYPHER_AST_FUNCTION_NAME,
    
    // Expression lists
    CYPHER_AST_EXPRESSION_LIST,
    CYPHER_AST_PROPERTY_LIST,
    CYPHER_AST_LABEL_EXPRESSION
} cypher_ast_node_type_t;

// Forward declaration
typedef struct cypher_ast_node cypher_ast_node_t;

// ============================================================================
// AST Node Structure
// ============================================================================

struct cypher_ast_node {
    cypher_ast_node_type_t type;
    
    union {
        // Statements
        struct {
            cypher_ast_node_t *left;
            cypher_ast_node_t *operator;
            cypher_ast_node_t *right;
        } composite_statement;
        
        struct {
            cypher_ast_node_t *clauses;
        } linear_statement;
        
        // Match clause
        struct {
            bool optional;
            cypher_ast_node_t *pattern;
            cypher_ast_node_t *where;
        } match_clause;
        
        // Unwind clause
        struct {
            cypher_ast_node_t *expression;
            cypher_ast_node_t *variable;
        } unwind_clause;
        
        // With/Return clause
        struct {
            bool distinct;
            cypher_ast_node_t *return_items;
            cypher_ast_node_t *order_by;
            cypher_ast_node_t *skip;
            cypher_ast_node_t *limit;
            cypher_ast_node_t *where;
        } with_clause, return_clause;
        
        // Create clause
        struct {
            cypher_ast_node_t *pattern;
        } create_clause;
        
        // Merge clause
        struct {
            cypher_ast_node_t *pattern_part;
            cypher_ast_node_t *merge_action;
        } merge_clause;
        
        // Set clause
        struct {
            cypher_ast_node_t *set_items;
        } set_clause;
        
        // Remove clause
        struct {
            cypher_ast_node_t *remove_items;
        } remove_clause;
        
        // Delete clause
        struct {
            bool detach;
            cypher_ast_node_t *expressions;
        } delete_clause;
        
        // Call clause
        struct {
            cypher_ast_node_t *procedure_name;
            cypher_ast_node_t *arguments;
            cypher_ast_node_t *yield_items;
            cypher_ast_node_t *where;
        } call_clause;
        
        // Where clause
        struct {
            cypher_ast_node_t *expression;
        } where_clause;
        
        // Order by clause
        struct {
            cypher_ast_node_t *sort_items;
        } order_by_clause;
        
        // Skip/Limit clause
        struct {
            cypher_ast_node_t *expression;
        } skip_clause, limit_clause;
        
        // Patterns
        struct {
            cypher_ast_node_t *patterns;
        } pattern_list;
        
        struct {
            cypher_ast_node_t *element;
        } pattern;
        
        struct {
            cypher_ast_node_t *node;
            cypher_ast_node_t *pattern_element_chain;
        } pattern_element;
        
        struct {
            cypher_ast_node_t *variable;
            cypher_ast_node_t *node_labels;
            cypher_ast_node_t *properties;
        } node_pattern;
        
        struct {
            cypher_ast_node_t *detail;
            cypher_ast_node_t *node;
        } relationship_pattern;
        
        struct {
            bool left_arrow;
            bool right_arrow;
            cypher_ast_node_t *variable;
            cypher_ast_node_t *rel_types;
            cypher_ast_node_t *variable_length;
            cypher_ast_node_t *properties;
        } relationship_detail;
        
        struct {
            cypher_ast_node_t *range_start;
            cypher_ast_node_t *range_end;
        } variable_length;
        
        // Expressions
        struct {
            cypher_ast_node_t *left;
            cypher_ast_node_t *right;
        } or_expression, xor_expression, and_expression;
        
        struct {
            cypher_ast_node_t *expression;
        } not_expression, unary_add_expression;
        
        struct {
            cypher_ast_node_t *left;
            cypher_ast_node_t *right;
        } comparison_expression, add_expression, multiply_expression, power_expression;
        
        struct {
            cypher_ast_node_t *operator;
            cypher_ast_node_t *right;
        } partial_comparison_expression;
        
        struct {
            cypher_ast_node_t *atom;
            cypher_ast_node_t *property_lookups;
            cypher_ast_node_t *node_labels;
        } postfix_expression;
        
        // Literals
        struct {
            bool value;
        } boolean_literal;
        
        struct {
            char *value;
            bool is_integer;
            bool is_float;
            union {
                int64_t integer_val;
                double float_val;
            } numeric;
        } number_literal;
        
        struct {
            char *value;
        } string_literal;
        
        struct {
            cypher_ast_node_t *expressions;
        } list_literal;
        
        struct {
            cypher_ast_node_t *properties;
        } map_literal;
        
        struct {
            char *name;
        } parameter, variable, property_key_name, label_name, relationship_type, binding_variable, symbolic_name;
        
        // Property access
        struct {
            cypher_ast_node_t *expression;
            cypher_ast_node_t *property_key;
        } property_access;
        
        struct {
            cypher_ast_node_t *expression;
        } property_lookup;
        
        struct {
            cypher_ast_node_t *labels;
        } node_labels;
        
        // Function calls
        struct {
            cypher_ast_node_t *function_name;
            bool distinct;
            cypher_ast_node_t *arguments;
        } function_invocation;
        
        struct {
            cypher_ast_node_t *expression;
        } count_expression, exists_expression;
        
        // Case expression
        struct {
            cypher_ast_node_t *test_expression;
            cypher_ast_node_t *alternatives;
            cypher_ast_node_t *else_expression;
        } case_expression;
        
        struct {
            cypher_ast_node_t *when_expression;
            cypher_ast_node_t *then_expression;
        } case_alternative;
        
        // Comprehensions
        struct {
            cypher_ast_node_t *filter_expression;
            cypher_ast_node_t *extract_expression;
        } list_comprehension;
        
        struct {
            cypher_ast_node_t *variable;
            cypher_ast_node_t *pattern;
            cypher_ast_node_t *where;
            cypher_ast_node_t *projection;
        } pattern_comprehension;
        
        struct {
            cypher_ast_node_t *id_in_coll;
            cypher_ast_node_t *where;
        } filter_expression, all_expression, any_expression, none_expression, single_expression;
        
        struct {
            cypher_ast_node_t *id_in_coll;
            cypher_ast_node_t *expression;
        } extract_expression;
        
        struct {
            cypher_ast_node_t *accumulator;
            cypher_ast_node_t *initial;
            cypher_ast_node_t *id_in_coll;
            cypher_ast_node_t *expression;
        } reduce_expression;
        
        // Path patterns
        struct {
            cypher_ast_node_t *pattern_element;
        } shortest_path_pattern, all_shortest_paths_pattern;
        
        // Return items
        struct {
            cypher_ast_node_t *items;
        } return_items;
        
        struct {
            cypher_ast_node_t *expression;
            cypher_ast_node_t *variable;
        } return_item;
        
        struct {
            cypher_ast_node_t *expression;
            cypher_ast_node_t *variable;
        } projection_item;
        
        struct {
            cypher_ast_node_t *expression;
            bool ascending;
        } sort_item;
        
        // Operators
        struct {
            bool all;
        } union_operator;
        
        struct {
            char *operator;
        } comparison_operator;
        
        // Namespace/procedure/function names
        struct {
            cypher_ast_node_t *names;
        } namespace;
        
        struct {
            cypher_ast_node_t *namespace;
            cypher_ast_node_t *name;
        } procedure_name, function_name;
        
        // Generic expression list
        struct {
            cypher_ast_node_t **expressions;
            size_t count;
            size_t capacity;
        } expression_list;
        
        // Label expression (for complex label patterns)
        struct {
            cypher_ast_node_t *expression;
        } label_expression;
    } data;
    
    // Linked list for sequences
    cypher_ast_node_t *next;
    
    // Source location info
    size_t line;
    size_t column;
    size_t length;
    
    // Memory management
    bool owns_children;
};

// ============================================================================
// AST Construction Functions
// ============================================================================

// Node creation
cypher_ast_node_t* cypher_ast_create_node(cypher_ast_node_type_t type);

// Statements
cypher_ast_node_t* cypher_ast_create_composite_statement(cypher_ast_node_t *left, cypher_ast_node_t *operator, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_linear_statement(cypher_ast_node_t *clauses);

// Clauses
cypher_ast_node_t* cypher_ast_create_match_clause(bool optional, cypher_ast_node_t *pattern, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_unwind_clause(cypher_ast_node_t *expression, cypher_ast_node_t *variable);
cypher_ast_node_t* cypher_ast_create_with_clause(bool distinct, cypher_ast_node_t *return_items, cypher_ast_node_t *order_by, cypher_ast_node_t *skip, cypher_ast_node_t *limit, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_return_clause(bool distinct, cypher_ast_node_t *return_items, cypher_ast_node_t *order_by, cypher_ast_node_t *skip, cypher_ast_node_t *limit);
cypher_ast_node_t* cypher_ast_create_create_clause(cypher_ast_node_t *pattern);
cypher_ast_node_t* cypher_ast_create_merge_clause(cypher_ast_node_t *pattern_part, cypher_ast_node_t *merge_action);
cypher_ast_node_t* cypher_ast_create_set_clause(cypher_ast_node_t *set_items);
cypher_ast_node_t* cypher_ast_create_remove_clause(cypher_ast_node_t *remove_items);
cypher_ast_node_t* cypher_ast_create_delete_clause(bool detach, cypher_ast_node_t *expressions);
cypher_ast_node_t* cypher_ast_create_call_clause(cypher_ast_node_t *procedure_name, cypher_ast_node_t *arguments, cypher_ast_node_t *yield_items, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_where_clause(cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_order_by_clause(cypher_ast_node_t *sort_items);
cypher_ast_node_t* cypher_ast_create_skip_clause(cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_limit_clause(cypher_ast_node_t *expression);

// Patterns
cypher_ast_node_t* cypher_ast_create_pattern_list(cypher_ast_node_t *patterns);
cypher_ast_node_t* cypher_ast_create_pattern(cypher_ast_node_t *element);
cypher_ast_node_t* cypher_ast_create_pattern_element(cypher_ast_node_t *node, cypher_ast_node_t *pattern_element_chain);
cypher_ast_node_t* cypher_ast_create_node_pattern(cypher_ast_node_t *variable, cypher_ast_node_t *node_labels, cypher_ast_node_t *properties);
cypher_ast_node_t* cypher_ast_create_relationship_pattern(cypher_ast_node_t *detail, cypher_ast_node_t *node);
cypher_ast_node_t* cypher_ast_create_relationship_detail(bool left_arrow, bool right_arrow, cypher_ast_node_t *variable, cypher_ast_node_t *rel_types, cypher_ast_node_t *variable_length, cypher_ast_node_t *properties);
cypher_ast_node_t* cypher_ast_create_variable_length(cypher_ast_node_t *range_start, cypher_ast_node_t *range_end);

// Expressions
cypher_ast_node_t* cypher_ast_create_or_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_xor_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_and_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_not_expression(cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_comparison_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_partial_comparison_expression(cypher_ast_node_t *operator, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_add_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_multiply_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_power_expression(cypher_ast_node_t *left, cypher_ast_node_t *right);
cypher_ast_node_t* cypher_ast_create_unary_add_expression(cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_postfix_expression(cypher_ast_node_t *atom, cypher_ast_node_t *property_lookups, cypher_ast_node_t *node_labels);

// Literals
cypher_ast_node_t* cypher_ast_create_null_literal(void);
cypher_ast_node_t* cypher_ast_create_boolean_literal(bool value);
cypher_ast_node_t* cypher_ast_create_number_literal(const char *value);
cypher_ast_node_t* cypher_ast_create_string_literal(const char *value);
cypher_ast_node_t* cypher_ast_create_list_literal(cypher_ast_node_t *expressions);
cypher_ast_node_t* cypher_ast_create_map_literal(cypher_ast_node_t *properties);
cypher_ast_node_t* cypher_ast_create_parameter(const char *name);
cypher_ast_node_t* cypher_ast_create_variable(const char *name);

// Property access and function calls
cypher_ast_node_t* cypher_ast_create_property_access(cypher_ast_node_t *expression, cypher_ast_node_t *property_key);
cypher_ast_node_t* cypher_ast_create_property_lookup(cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_node_labels(cypher_ast_node_t *labels);
cypher_ast_node_t* cypher_ast_create_function_invocation(cypher_ast_node_t *function_name, bool distinct, cypher_ast_node_t *arguments);
cypher_ast_node_t* cypher_ast_create_count_expression(cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_exists_expression(cypher_ast_node_t *expression);

// Case expression
cypher_ast_node_t* cypher_ast_create_case_expression(cypher_ast_node_t *test_expression, cypher_ast_node_t *alternatives, cypher_ast_node_t *else_expression);
cypher_ast_node_t* cypher_ast_create_case_alternative(cypher_ast_node_t *when_expression, cypher_ast_node_t *then_expression);

// Comprehensions
cypher_ast_node_t* cypher_ast_create_list_comprehension(cypher_ast_node_t *filter_expression, cypher_ast_node_t *extract_expression);
cypher_ast_node_t* cypher_ast_create_pattern_comprehension(cypher_ast_node_t *variable, cypher_ast_node_t *pattern, cypher_ast_node_t *where, cypher_ast_node_t *projection);
cypher_ast_node_t* cypher_ast_create_filter_expression(cypher_ast_node_t *id_in_coll, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_extract_expression(cypher_ast_node_t *id_in_coll, cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_reduce_expression(cypher_ast_node_t *accumulator, cypher_ast_node_t *initial, cypher_ast_node_t *id_in_coll, cypher_ast_node_t *expression);
cypher_ast_node_t* cypher_ast_create_all_expression(cypher_ast_node_t *id_in_coll, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_any_expression(cypher_ast_node_t *id_in_coll, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_none_expression(cypher_ast_node_t *id_in_coll, cypher_ast_node_t *where);
cypher_ast_node_t* cypher_ast_create_single_expression(cypher_ast_node_t *id_in_coll, cypher_ast_node_t *where);

// Path patterns
cypher_ast_node_t* cypher_ast_create_shortest_path_pattern(cypher_ast_node_t *pattern_element);
cypher_ast_node_t* cypher_ast_create_all_shortest_paths_pattern(cypher_ast_node_t *pattern_element);

// Return items
cypher_ast_node_t* cypher_ast_create_return_items(cypher_ast_node_t *items);
cypher_ast_node_t* cypher_ast_create_return_item(cypher_ast_node_t *expression, cypher_ast_node_t *variable);
cypher_ast_node_t* cypher_ast_create_projection_item(cypher_ast_node_t *expression, cypher_ast_node_t *variable);
cypher_ast_node_t* cypher_ast_create_sort_item(cypher_ast_node_t *expression, bool ascending);

// Names and identifiers
cypher_ast_node_t* cypher_ast_create_property_key_name(const char *name);
cypher_ast_node_t* cypher_ast_create_label_name(const char *name);
cypher_ast_node_t* cypher_ast_create_relationship_type(const char *name);
cypher_ast_node_t* cypher_ast_create_binding_variable(const char *name);
cypher_ast_node_t* cypher_ast_create_symbolic_name(const char *name);
cypher_ast_node_t* cypher_ast_create_namespace(cypher_ast_node_t *names);
cypher_ast_node_t* cypher_ast_create_procedure_name(cypher_ast_node_t *namespace, cypher_ast_node_t *name);
cypher_ast_node_t* cypher_ast_create_function_name(cypher_ast_node_t *namespace, cypher_ast_node_t *name);

// Operators
cypher_ast_node_t* cypher_ast_create_union_operator(bool all);
cypher_ast_node_t* cypher_ast_create_comparison_operator(const char *operator);

// List operations
cypher_ast_node_t* cypher_ast_create_expression_list(void);
void cypher_ast_list_append(cypher_ast_node_t *list, cypher_ast_node_t *item);
size_t cypher_ast_list_length(cypher_ast_node_t *list);
cypher_ast_node_t* cypher_ast_list_get(cypher_ast_node_t *list, size_t index);

// ============================================================================
// Memory Management
// ============================================================================

void cypher_ast_free(cypher_ast_node_t *node);
void cypher_ast_free_recursive(cypher_ast_node_t *node);

// ============================================================================
// Utility Functions
// ============================================================================

const char* cypher_ast_node_type_name(cypher_ast_node_type_t type);
void cypher_ast_print(cypher_ast_node_t *node, int indent);
cypher_ast_node_t* cypher_ast_clone(cypher_ast_node_t *node);

#ifdef __cplusplus
}
#endif

#endif // CYPHER_AST_H