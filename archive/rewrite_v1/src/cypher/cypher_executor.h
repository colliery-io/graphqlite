#ifndef CYPHER_EXECUTOR_H
#define CYPHER_EXECUTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cypher_ast.h"
#include "../core/graphqlite_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Execution Result Types
// ============================================================================

// Represents a value in the result set
typedef struct {
    property_type_t type;
    union {
        int64_t int_val;
        char *text_val;
        double real_val;
        bool bool_val;
        struct {
            int64_t id;
            char **labels;
            size_t label_count;
            property_set_t *properties;
        } node_val;
        struct {
            int64_t id;
            int64_t source_id;
            int64_t target_id;
            char *type;
            property_set_t *properties;
        } edge_val;
    } value;
} cypher_value_t;

// Column information for the result set
typedef struct {
    char *name;
    property_type_t type;
} cypher_column_t;

// A single row in the result set
typedef struct {
    cypher_value_t *values;
    size_t column_count;
} cypher_row_t;

// Complete execution result
typedef struct {
    cypher_column_t *columns;
    size_t column_count;
    cypher_row_t *rows;
    size_t row_count;
    
    // Error handling
    bool has_error;
    char *error_message;
    
    // Statistics
    size_t nodes_created;
    size_t edges_created;
    size_t properties_set;
    size_t nodes_deleted;
    size_t edges_deleted;
} cypher_result_t;

// ============================================================================
// Execution Context
// ============================================================================

// Variable binding during execution
typedef struct cypher_binding {
    char *name;
    cypher_value_t value;
    struct cypher_binding *next;
} cypher_binding_t;

// Execution context
typedef struct {
    graphqlite_db_t *db;
    cypher_binding_t *bindings;
    cypher_result_t *result;
    bool in_transaction;
} cypher_execution_context_t;

// ============================================================================
// Main Executor Functions
// ============================================================================

/**
 * Execute a parsed openCypher query
 */
cypher_result_t* cypher_execute(graphqlite_db_t *db, cypher_ast_node_t *ast);

/**
 * Execute a openCypher query string (parse + execute)
 */
cypher_result_t* cypher_execute_query(graphqlite_db_t *db, const char *query);

/**
 * Free execution result
 */
void cypher_result_destroy(cypher_result_t *result);

// ============================================================================
// Result Access Functions
// ============================================================================

/**
 * Get number of columns in result
 */
size_t cypher_result_get_column_count(cypher_result_t *result);

/**
 * Get column name by index
 */
const char* cypher_result_get_column_name(cypher_result_t *result, size_t column_index);

/**
 * Get number of rows in result
 */
size_t cypher_result_get_row_count(cypher_result_t *result);

/**
 * Get value at specific row/column
 */
cypher_value_t* cypher_result_get_value(cypher_result_t *result, size_t row_index, size_t column_index);

/**
 * Check if result has errors
 */
bool cypher_result_has_error(cypher_result_t *result);

/**
 * Get error message
 */
const char* cypher_result_get_error(cypher_result_t *result);

// ============================================================================
// Value Utility Functions
// ============================================================================

/**
 * Convert cypher_value_t to string representation
 */
char* cypher_value_to_string(cypher_value_t *value);

/**
 * Free a cypher value's internal data
 */
void cypher_value_free(cypher_value_t *value);

/**
 * Create cypher value from property value
 */
cypher_value_t cypher_value_from_property(property_value_t *prop);

/**
 * Create cypher value for a node
 */
cypher_value_t cypher_value_from_node(graphqlite_db_t *db, int64_t node_id);

/**
 * Create cypher value for an edge
 */
cypher_value_t cypher_value_from_edge(graphqlite_db_t *db, int64_t edge_id);

// ============================================================================
// Internal Execution Functions (Implementation Details)
// ============================================================================

// Context management
cypher_execution_context_t* cypher_execution_context_create(graphqlite_db_t *db);
void cypher_execution_context_destroy(cypher_execution_context_t *ctx);

// Variable binding
void cypher_bind_variable(cypher_execution_context_t *ctx, const char *name, cypher_value_t value);
cypher_value_t* cypher_get_binding(cypher_execution_context_t *ctx, const char *name);

// Statement execution
int cypher_execute_statement(cypher_execution_context_t *ctx, cypher_ast_node_t *stmt);
int cypher_execute_linear_statement(cypher_execution_context_t *ctx, cypher_ast_node_t *stmt);
int cypher_execute_composite_statement(cypher_execution_context_t *ctx, cypher_ast_node_t *stmt);
int execute_basic_match_return(cypher_execution_context_t *ctx);
int cypher_execute_match_clause(cypher_execution_context_t *ctx, cypher_ast_node_t *match);
int cypher_execute_return_clause(cypher_execution_context_t *ctx, cypher_ast_node_t *return_clause);
int cypher_execute_create_clause(cypher_execution_context_t *ctx, cypher_ast_node_t *create);

// Pattern matching
int cypher_match_pattern(cypher_execution_context_t *ctx, cypher_ast_node_t *pattern);
int cypher_match_node_pattern(cypher_execution_context_t *ctx, cypher_ast_node_t *node_pattern);

#ifdef __cplusplus
}
#endif

#endif // CYPHER_EXECUTOR_H