#ifndef GQL_MATCHER_H
#define GQL_MATCHER_H

#include "gql_executor.h"
#include "gql_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Pattern Matching Types
// ============================================================================

typedef enum {
    MATCH_RESULT_SUCCESS,
    MATCH_RESULT_NO_MATCHES,
    MATCH_RESULT_ERROR
} match_result_status_t;

typedef struct {
    char *variable_name;
    gql_value_t *value;
} variable_binding_t;

typedef struct {
    variable_binding_t *bindings;
    size_t binding_count;
    size_t binding_capacity;
} variable_binding_set_t;

typedef struct match_result_set {
    variable_binding_set_t *result_sets;
    size_t result_count;
    size_t result_capacity;
    match_result_status_t status;
    char *error_message;
} match_result_set_t;

// ============================================================================
// Pattern Matching Interface
// ============================================================================

// Main pattern matching function
match_result_set_t* match_patterns(gql_execution_context_t *ctx, gql_ast_node_t *patterns);

// Individual pattern matchers
match_result_set_t* match_single_pattern(gql_execution_context_t *ctx, gql_ast_node_t *pattern);
match_result_set_t* match_node_pattern(gql_execution_context_t *ctx, gql_ast_node_t *node_pattern);
match_result_set_t* match_edge_pattern(gql_execution_context_t *ctx, 
                                      gql_ast_node_t *source_node,
                                      gql_ast_node_t *edge_pattern,
                                      gql_ast_node_t *target_node);

// ============================================================================
// Node Matching Functions
// ============================================================================

// Find all nodes matching a label
int64_t* find_nodes_by_label(gql_execution_context_t *ctx, const char *label, size_t *count);

// Find all nodes (no label filter)
int64_t* find_all_nodes(gql_execution_context_t *ctx, size_t *count);

// Check if a node matches property constraints
bool node_matches_properties(gql_execution_context_t *ctx, int64_t node_id, 
                            gql_ast_node_t *property_map);

// Load node data into a gql_value
gql_value_t* load_node_value(gql_execution_context_t *ctx, int64_t node_id);

// ============================================================================
// Edge Matching Functions  
// ============================================================================

// Find edges from source nodes
int64_t* find_outgoing_edges(gql_execution_context_t *ctx, int64_t source_id, 
                            const char *edge_type, size_t *count);

// Find edges to target nodes
int64_t* find_incoming_edges(gql_execution_context_t *ctx, int64_t target_id,
                           const char *edge_type, size_t *count);

// Get target nodes from edges
int64_t* get_edge_targets(gql_execution_context_t *ctx, int64_t *edge_ids, 
                         size_t edge_count, size_t *target_count);

// Load edge data into a gql_value
gql_value_t* load_edge_value(gql_execution_context_t *ctx, int64_t edge_id);

// ============================================================================
// Variable Binding Management
// ============================================================================

variable_binding_set_t* create_binding_set(void);
void destroy_binding_set(variable_binding_set_t *set);
int add_binding(variable_binding_set_t *set, const char *name, gql_value_t *value);
gql_value_t* get_binding(variable_binding_set_t *set, const char *name);

// Copy and merge binding sets
variable_binding_set_t* copy_binding_set(variable_binding_set_t *set);
variable_binding_set_t* merge_binding_sets(variable_binding_set_t *set1, variable_binding_set_t *set2);

// ============================================================================
// Match Result Management
// ============================================================================

match_result_set_t* create_match_result_set(void);
void destroy_match_result_set(match_result_set_t *results);
int add_match_result(match_result_set_t *results, variable_binding_set_t *bindings);
void set_match_error(match_result_set_t *results, const char *message);

// ============================================================================
// WHERE Clause Evaluation
// ============================================================================

// Filter match results by WHERE clause
match_result_set_t* apply_where_filter(gql_execution_context_t *ctx, 
                                      match_result_set_t *matches,
                                      gql_ast_node_t *where_clause);

// Evaluate expression in context of variable bindings
gql_value_t evaluate_expression_with_bindings(gql_execution_context_t *ctx,
                                             gql_ast_node_t *expr,
                                             variable_binding_set_t *bindings);

// ============================================================================
// RETURN Clause Processing
// ============================================================================

// Project match results according to RETURN clause
gql_result_t* project_match_results(gql_execution_context_t *ctx,
                                   match_result_set_t *matches,
                                   gql_ast_node_t *return_clause);

// ============================================================================
// Utility Functions
// ============================================================================

void print_match_results(match_result_set_t *results);
void print_binding_set(variable_binding_set_t *bindings);

#ifdef __cplusplus
}
#endif

#endif // GQL_MATCHER_H