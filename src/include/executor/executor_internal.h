/*
 * Internal header for executor modules
 * Not part of the public API - shared between executor implementation files
 */

#ifndef EXECUTOR_INTERNAL_H
#define EXECUTOR_INTERNAL_H

#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"
#include "parser/cypher_ast.h"
#include "parser/cypher_parser.h"
#include "transform/cypher_transform.h"
#include "transform/transform_helpers.h"

#include <stdbool.h>
#include <stdint.h>

/* Variable type for mapping (node vs edge) */
typedef enum {
    VAR_MAP_TYPE_NODE,
    VAR_MAP_TYPE_EDGE
} variable_map_type;

/* Variable to entity ID mapping structure */
typedef struct {
    char *variable;
    int entity_id;          /* node_id for nodes, edge_id for edges */
    variable_map_type type; /* NODE or EDGE */
} variable_mapping;

typedef struct {
    variable_mapping *mappings;
    int count;
    int capacity;
} variable_map;

/* FOREACH variable binding - stores current iteration value */
typedef struct {
    char *variable;
    int literal_type;  /* LITERAL_INTEGER, LITERAL_STRING, etc. */
    union {
        int64_t integer;
        double decimal;
        char *string;
        bool boolean;
    } value;
} foreach_binding;

typedef struct {
    foreach_binding *bindings;
    int count;
    int capacity;
} foreach_context;

/* Thread-local foreach context for nested property resolution */
extern __thread foreach_context *g_foreach_ctx;

/* Variable map functions */
variable_map* create_variable_map(void);
void free_variable_map(variable_map *map);
int get_variable_node_id(variable_map *map, const char *variable);
int get_variable_edge_id(variable_map *map, const char *variable);
bool is_variable_edge(variable_map *map, const char *variable);
int set_variable_node_id(variable_map *map, const char *variable, int node_id);
int set_variable_edge_id(variable_map *map, const char *variable, int edge_id);

/* FOREACH context functions */
foreach_context* create_foreach_context(void);
void free_foreach_context(foreach_context *ctx);
int set_foreach_binding_int(foreach_context *ctx, const char *variable, int64_t value);
int set_foreach_binding_string(foreach_context *ctx, const char *variable, const char *value);
foreach_binding* get_foreach_binding(foreach_context *ctx, const char *variable);

/* Result helper functions */
cypher_result* create_empty_result(void);
void set_result_error(cypher_result *result, const char *error_msg);

/* Helper to bind parameters from JSON to a prepared statement */
int bind_params_from_json(sqlite3_stmt *stmt, const char *params_json);

/* Helper to lookup a parameter value from JSON */
int get_param_value(const char *params_json, const char *param_name,
                    property_type *out_type, void *out_value, size_t value_size);

/* Clause execution functions (used by main dispatcher and other clauses) */
int execute_create_clause(cypher_executor *executor, cypher_create *create, cypher_result *result);
int execute_foreach_clause(cypher_executor *executor, cypher_foreach *foreach, cypher_result *result);
int execute_merge_clause(cypher_executor *executor, cypher_merge *merge, cypher_result *result);
int execute_set_clause(cypher_executor *executor, cypher_set *set, cypher_result *result);
int execute_match_clause(cypher_executor *executor, cypher_match *match, cypher_result *result);

/* SET operations with variable map */
int execute_set_operations(cypher_executor *executor, cypher_set *set, variable_map *var_map, cypher_result *result);
int execute_set_items(cypher_executor *executor, ast_list *items, variable_map *var_map, cypher_result *result);

/* MATCH-based query execution functions */
int execute_match_return_query(cypher_executor *executor, cypher_match *match, cypher_return *return_clause, cypher_result *result);
int execute_match_create_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_result *result);
int execute_match_create_return_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_return *return_clause, cypher_result *result);
int execute_match_set_query(cypher_executor *executor, cypher_match *match, cypher_set *set, cypher_result *result);
int execute_match_delete_query(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause, cypher_result *result);
int execute_match_merge_query(cypher_executor *executor, cypher_match *match, cypher_merge *merge, cypher_result *result);
int execute_match_remove_query(cypher_executor *executor, cypher_match *match, cypher_remove *remove, cypher_result *result);

/* REMOVE operations with variable map */
int execute_remove_operations(cypher_executor *executor, cypher_remove *remove, variable_map *var_map, cypher_result *result);

/* Pattern matching functions */
int find_node_by_pattern(cypher_executor *executor, cypher_node_pattern *node_pattern);
int find_edge_by_pattern(cypher_executor *executor, int source_id, int target_id,
                         const char *type, cypher_rel_pattern *rel_pattern);

/* DELETE functions */
int delete_edge_by_id(cypher_executor *executor, int64_t edge_id);
int delete_node_by_id(cypher_executor *executor, int64_t node_id, bool detach);

/* Path and CREATE functions */
int execute_path_pattern_with_variables(cypher_executor *executor, cypher_path *path,
                                       cypher_result *result, variable_map *var_map);

/* Result building functions */
int build_query_results(cypher_executor *executor, sqlite3_stmt *stmt, cypher_return *return_clause,
                        cypher_result *result, cypher_transform_context *ctx);
agtype_value* create_property_agtype_value(const char* value);
agtype_value* build_path_from_ids(cypher_executor *executor, cypher_transform_context *ctx,
                                  const char *path_name, const char *json_ids);

#endif /* EXECUTOR_INTERNAL_H */
