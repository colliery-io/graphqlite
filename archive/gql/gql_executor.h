#ifndef GQL_EXECUTOR_H
#define GQL_EXECUTOR_H

#include "gql_ast.h"
#include "../core/graphqlite_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Result Types
// ============================================================================

typedef enum {
    GQL_RESULT_SUCCESS,
    GQL_RESULT_ERROR,
    GQL_RESULT_EMPTY
} gql_result_status_t;

typedef enum {
    GQL_VALUE_NULL,
    GQL_VALUE_INTEGER,
    GQL_VALUE_STRING,
    GQL_VALUE_BOOLEAN,
    GQL_VALUE_NODE,
    GQL_VALUE_EDGE,
    GQL_VALUE_ARRAY
} gql_value_type_t;

// Forward declarations
typedef struct gql_value gql_value_t;
typedef struct gql_result_row gql_result_row_t;
typedef struct gql_result gql_result_t;

// ============================================================================
// Value System
// ============================================================================

struct gql_value {
    gql_value_type_t type;
    union {
        int64_t integer;
        char *string;
        bool boolean;
        struct {
            int64_t id;
            char **labels;
            size_t label_count;
            property_set_t *properties;
        } node;
        struct {
            int64_t id;
            int64_t source_id;
            int64_t target_id;
            char *type;
            property_set_t *properties;
        } edge;
        struct {
            gql_value_t *items;
            size_t count;
            size_t capacity;
        } array;
    } data;
};

// ============================================================================
// Result Structures
// ============================================================================

struct gql_result_row {
    gql_value_t *columns;
    size_t column_count;
    char **column_names;
    gql_result_row_t *next;
};

struct gql_result {
    gql_result_status_t status;
    char *error_message;
    
    // Result data
    gql_result_row_t *rows;
    size_t row_count;
    char **column_names;
    size_t column_count;
    
    // Execution statistics
    uint64_t execution_time_us;
    uint64_t nodes_created;
    uint64_t edges_created;
    uint64_t properties_set;
};

// ============================================================================
// Execution Context
// ============================================================================

typedef struct {
    char *name;
    gql_value_t value;
} gql_variable_t;

typedef struct gql_execution_context {
    graphqlite_db_t *db;
    
    // Variable bindings
    gql_variable_t *variables;
    size_t variable_count;
    size_t variable_capacity;
    
    // Current result being built
    gql_result_t *current_result;
    
    // Execution state
    bool in_transaction;
    char *error_message;
} gql_execution_context_t;

// ============================================================================
// Executor Interface
// ============================================================================

// Main execution function
gql_result_t* gql_execute(graphqlite_db_t *db, gql_ast_node_t *ast);
gql_result_t* gql_execute_query(const char *query, graphqlite_db_t *db);

// Execution context management
gql_execution_context_t* gql_context_create(graphqlite_db_t *db);
void gql_context_destroy(gql_execution_context_t *ctx);

// Variable management
int gql_context_set_variable(gql_execution_context_t *ctx, const char *name, gql_value_t *value);
gql_value_t* gql_context_get_variable(gql_execution_context_t *ctx, const char *name);

// ============================================================================
// Query Execution Functions
// ============================================================================

gql_result_t* execute_match_query(gql_execution_context_t *ctx, gql_ast_node_t *ast);
gql_result_t* execute_create_query(gql_execution_context_t *ctx, gql_ast_node_t *ast);
gql_result_t* execute_set_query(gql_execution_context_t *ctx, gql_ast_node_t *ast);
gql_result_t* execute_delete_query(gql_execution_context_t *ctx, gql_ast_node_t *ast);

// ============================================================================
// Pattern Execution Functions
// ============================================================================

int execute_pattern_list(gql_execution_context_t *ctx, gql_ast_node_t *patterns);
int execute_pattern(gql_execution_context_t *ctx, gql_ast_node_t *pattern);
int execute_node_pattern(gql_execution_context_t *ctx, gql_ast_node_t *node_pattern);
int execute_edge_pattern(gql_execution_context_t *ctx, gql_ast_node_t *edge_pattern);

// ============================================================================
// Expression Evaluation
// ============================================================================

gql_value_t evaluate_expression(gql_execution_context_t *ctx, gql_ast_node_t *expr);
bool evaluate_where_clause(gql_execution_context_t *ctx, gql_ast_node_t *where_clause);
gql_result_t* evaluate_return_clause(gql_execution_context_t *ctx, gql_ast_node_t *return_clause);

// ============================================================================
// Value Management
// ============================================================================

gql_value_t* gql_value_create_null(void);
gql_value_t* gql_value_create_integer(int64_t value);
gql_value_t* gql_value_create_string(const char *value);
gql_value_t* gql_value_create_boolean(bool value);
gql_value_t* gql_value_create_node(int64_t id, char **labels, size_t label_count, property_set_t *props);
gql_value_t* gql_value_create_edge(int64_t id, int64_t source, int64_t target, const char *type, property_set_t *props);

void gql_value_free(gql_value_t *value);
void gql_value_free_contents(gql_value_t *value);
char* gql_value_to_string(gql_value_t *value);
gql_value_t* gql_value_copy(gql_value_t *value);

// ============================================================================
// Result Management
// ============================================================================

gql_result_t* gql_result_create(void);
void gql_result_destroy(gql_result_t *result);
int gql_result_add_column(gql_result_t *result, const char *name);
int gql_result_add_row(gql_result_t *result, gql_value_t *values, size_t count);
void gql_result_set_error(gql_result_t *result, const char *message);

// ============================================================================
// Utility Functions
// ============================================================================

const char* gql_value_type_name(gql_value_type_t type);
void gql_result_print(gql_result_t *result);
bool gql_values_equal(gql_value_t *a, gql_value_t *b);
int gql_value_compare(gql_value_t *a, gql_value_t *b);

#ifdef __cplusplus
}
#endif

#endif // GQL_EXECUTOR_H