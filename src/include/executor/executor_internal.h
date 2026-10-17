/*
 * Internal header for executor modules
 * Not part of the public API - shared between executor implementation files
 */

#ifndef EXECUTOR_INTERNAL_H
#define EXECUTOR_INTERNAL_H

#include "executor/cypher_executor.h"
#include "executor/cypher_schema.h"
#include "executor/query_patterns.h"
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

/* entity_id of a variable bound to null (an OPTIONAL MATCH miss): SET on
 * it does nothing. Below 0, so it reads as unbound everywhere else. */
#define GQL_NULL_ENTITY_ID (-2)

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
    bool is_json;      /* LITERAL_STRING holding a map or list as JSON text */
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
int set_foreach_binding_real(foreach_context *ctx, const char *variable, double value);
int set_foreach_binding_bool(foreach_context *ctx, const char *variable, bool value);
int set_foreach_binding_null(foreach_context *ctx, const char *variable);
/* A map or list, as JSON text. */
int set_foreach_binding_json(foreach_context *ctx, const char *variable, const char *json);

/* Bind one UNWIND/FOREACH item, keeping its type (#124). Maps and lists
 * bind as JSON text, so `item.key` can read them. Returns -1 for an item
 * that cannot be bound; the caller skips it. */
int bind_foreach_item_ast(foreach_context *ctx, const char *variable, ast_node *item);
/* Bind the current row of `SELECT value, type FROM json_each(...)`. */
int bind_foreach_item_json_each(foreach_context *ctx, const char *variable,
                                sqlite3_stmt *stmt, int value_col, int type_col);

/* The value pointer that the schema setters take for a property_value. */
static inline const void *property_value_ptr(property_type type, const property_value *pv)
{
    switch (type) {
        case PROP_TYPE_INTEGER: return &pv->as_int;
        case PROP_TYPE_REAL:    return &pv->as_real;
        case PROP_TYPE_BOOLEAN: return &pv->as_bool;
        default:                return pv->as_str;
    }
}

/* Typed value of `x` or `x.key` where x is a FOREACH/UNWIND binding.
 * Returns 0 with *out_type and *out set (caller frees out), -2 for null,
 * -1 when expr is not one of these. */
int foreach_value(sqlite3 *db, foreach_context *ctx, ast_node *expr,
                  property_type *out_type, property_value *out);

/* Read `key` from a JSON object, keeping the JSON type (true/false become
 * PROP_TYPE_BOOLEAN). Returns 0 with *out_type and *out set (caller frees
 * out), -2 when the key is missing or null, -1 when json is not an object. */
int json_map_property(sqlite3 *db, const char *json, const char *key,
                      property_type *out_type, property_value *out);

/* Read `var.key` where var is a binding that holds a JSON map, e.g.
 * `UNWIND $rows AS row CREATE (:X {id: row.id})` (#124). Returns 0 with
 * *out_type and *out set (caller frees out), -2 when the key is missing or
 * null, -1 when expr is not a property of such a binding. */
int foreach_map_property(sqlite3 *db, foreach_context *ctx, ast_node *expr,
                         property_type *out_type, property_value *out);

/* Result helper functions */
cypher_result* create_empty_result(void);

/* CREATE / DROP / SHOW of indexes and constraints (GQLITE-T-0008): the
 * result, or NULL when query is not such a statement. */
cypher_result *schema_command_try(cypher_executor *executor, const char *query);
/* NULL when the data meets every constraint, else the error (owned). */
char *schema_check_constraints(cypher_executor *executor);
/* 1 when query is a valid schema statement, -1 with msg when it is an
 * invalid one, 0 when it is not one. */
int schema_command_check(const char *query, char *msg, size_t msglen);
void set_result_error(cypher_result *result, const char *error_msg);
/* set_result_error plus a GQL_ERR_* code and a 1-based source location
 * (0 = unknown). The code must point at static storage. */
void set_result_error_ex(cypher_result *result, const char *error_msg,
                         const char *error_code, int line, int column);

/* Helper to bind parameters from JSON to a prepared statement */
int bind_params_from_json(sqlite3_stmt *stmt, const char *params_json);

/* Decode one JSON escape sequence from a parameter string. `*pp` points at the
 * character after the backslash and is advanced past the sequence; the decoded
 * UTF-8 bytes (at most 4) are written to `out` and their count returned.
 * Handles \uXXXX and surrogate pairs, which json.dumps() emits for every
 * non-ASCII character by default. */
int gql_json_decode_escape(const char **pp, char *out);

/* Helper to lookup a parameter value from JSON */
int get_param_value(const char *params_json, const char *param_name,
                    property_type *out_type, property_value *out_value);

/* Clause execution functions (used by main dispatcher and other clauses) */
int execute_create_clause(cypher_executor *executor, cypher_create *create, cypher_result *result);
int execute_create_clause_with_varmap(cypher_executor *executor, cypher_create *create,
                                      cypher_result *result, variable_map **out_var_map);
int execute_foreach_clause(cypher_executor *executor, cypher_foreach *foreach, cypher_result *result);
/* Register the current FOREACH/UNWIND bindings as scalar values of a
 * transform context, so an expression can read them. */
struct cypher_transform_context;
void executor_register_foreach_values(struct cypher_transform_context *ctx);
int execute_foreach_clause_vm(cypher_executor *executor, cypher_foreach *foreach,
                              variable_map *outer, cypher_result *result);
/* Canonical MERGE entry point (I-0041 C8). Pass NULL for external_vars
 * if no inbound scope; pass NULL for out_var_map if you don't want the
 * resulting bindings. The four pre-collapse variants (no-arg,
 * _with_vars, _with_varmap, _with_vars_ex) all map to this signature. */
int execute_merge_clause(cypher_executor *executor, cypher_merge *merge,
                         cypher_result *result, variable_map *external_vars,
                         variable_map **out_var_map);
int execute_merge_with_variables(cypher_executor *executor, cypher_merge *merge,
                                 variable_map *var_map, cypher_result *result);
int execute_set_clause(cypher_executor *executor, cypher_set *set, cypher_result *result);
int execute_match_clause(cypher_executor *executor, cypher_match *match, cypher_result *result);

/* Function evaluation via SQLite (defined in executor_set.c) */
int evaluate_function_call_via_sqlite(cypher_executor *executor,
    cypher_function_call *func_call,
    property_type *out_type, property_value *out_value);

/* T-0328: evaluate an AST expression as a boolean predicate
 * against a variable_map. Returns 1 (pass), 0 (drop), -1 (error). */
int executor_eval_predicate(cypher_executor *executor,
                            ast_node *expr,
                            variable_map *var_map);

/* Evaluate an AST expression against a variable_map and return its value.
 * Returns 0 (value in out params), -1 (error), -2 (NULL). */
int executor_eval_value(cypher_executor *executor, ast_node *expr,
                        variable_map *var_map, property_type *out_type,
                        property_value *out_value);

/* SET operations with variable map */
int execute_set_operations(cypher_executor *executor, cypher_set *set, variable_map *var_map, cypher_result *result);
int execute_set_items(cypher_executor *executor, ast_list *items, variable_map *var_map, cypher_result *result);

/* MATCH-based query execution functions */
int execute_match_return_query(cypher_executor *executor, cypher_match *match, cypher_return *return_clause, cypher_result *result);
int execute_match_create_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_result *result);
/* Canonical multi-MATCH + CREATE entry point (I-0041 C11). Pass NULL
 * for out_var_map if the caller doesn't need the accumulated bindings.
 * Pass non-NULL out_row_maps/out_row_count to receive one variable_map per
 * processed MATCH row (matched + CREATE-introduced bindings) for RETURN
 * projection of CREATE-only variables (GitHub #95); caller owns the array
 * and each map. */
int execute_multi_match_create_query(cypher_executor *executor, cypher_query *query,
                                     cypher_create *create, cypher_result *result,
                                     variable_map **out_var_map,
                                     variable_map ***out_row_maps, int *out_row_count);
int bind_match_clause_into_varmap(cypher_executor *executor, cypher_match *match,
                                  variable_map *var_map, cypher_result *result);
int execute_match_merge_query_with_varmap(cypher_executor *executor, cypher_match *match, cypher_merge *merge,
                                          cypher_result *result, variable_map **out_var_map);
int execute_match_create_return_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_return *return_clause, cypher_result *result);
int execute_match_set_query(cypher_executor *executor, cypher_match *match, cypher_set *set, cypher_result *result);
int execute_match_delete_query(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause, cypher_result *result);
/* GQLITE-T-0253: 1 if the MATCH binds a non-null entity for any DELETE
 * target, 0 if none, -1 on error. */
int delete_targets_bound(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause);
int execute_match_merge_query(cypher_executor *executor, cypher_match *match, cypher_merge *merge, cypher_result *result);
int execute_match_remove_query(cypher_executor *executor, cypher_match *match, cypher_remove *remove, cypher_result *result);

/* REMOVE operations with variable map */
int execute_remove_operations(cypher_executor *executor, cypher_remove *remove, variable_map *var_map, cypher_result *result);

/* DELETE operations with variable map (I-0042 E4): deletes nodes/edges
 * whose ids are bound in var_map per the variables named in delete->items.
 * Increments result->nodes_deleted / relationships_deleted. */
int execute_delete_operations(cypher_executor *executor, cypher_delete *del, variable_map *var_map, cypher_result *result);

/* Pattern matching functions */
int find_node_by_pattern(cypher_executor *executor, cypher_node_pattern *node_pattern);
int find_edge_by_pattern(cypher_executor *executor, int source_id, int target_id,
                         const char *type, cypher_rel_pattern *rel_pattern);

/* DELETE functions */
int delete_edge_by_id(cypher_executor *executor, int64_t edge_id);
/* When detach is true and detached_edges is non-NULL, receives the number of
 * incident edges removed before the node itself was deleted. */
int delete_node_by_id(cypher_executor *executor, int64_t node_id, bool detach,
                      int *detached_edges);

/* Path and CREATE functions */
int execute_path_pattern_with_variables(cypher_executor *executor, cypher_path *path,
                                       cypher_result *result, variable_map *var_map);

/* AST map/list to JSON string serialization */
char* serialize_ast_to_json(ast_node *expr);

/* Result building functions */
int build_query_results(cypher_executor *executor, sqlite3_stmt *stmt, cypher_return *return_clause,
                        cypher_result *result, cypher_transform_context *ctx);

/* Query-pattern handlers extracted from query_dispatch.c (I-0040 M1-M3) */
struct query_pattern;  /* forward-declared in executor/query_patterns.h */
int handle_generic_transform(cypher_executor *executor, cypher_query *query,
                             cypher_result *result, clause_flags flags);
int handle_call_subquery(cypher_executor *executor, cypher_query *query,
                         cypher_result *result, clause_flags flags);
int handle_merge_with_pipeline(cypher_executor *executor, cypher_query *query,
                               cypher_result *result, clause_flags flags);

/* RETURN-projection + aggregation helpers (executor_result_project.c, I-0040 M3) */
bool synthesize_delete_return(cypher_return *ret, cypher_result *result);
bool return_has_aggregation(cypher_return *ret);
const char *aggregating_call_name(ast_node *expr);
void project_aggregate_cell(cypher_executor *executor,
                            cypher_return_item *item,
                            variable_map **maps, int n_maps,
                            cypher_result *result, int col_idx);
void project_return_row_from_var_map(cypher_executor *executor,
                                     cypher_return *ret,
                                     variable_map *var_map,
                                     cypher_result *result,
                                     int row_idx);
void set_return_column_names(cypher_return *ret, cypher_result *result);
agtype_value* create_property_agtype_value(const char* value, int col_type);

/* GQLITE-A-0007: result columns with the hidden type tags of RETURN. */
int gql_visible_column_count(sqlite3_stmt *stmt);
int gql_result_cell_type(sqlite3_stmt *stmt, int col, int visible);
agtype_value* build_path_from_ids(cypher_executor *executor, cypher_transform_context *ctx,
                                  const char *path_name, const char *json_ids);

#endif /* EXECUTOR_INTERNAL_H */
