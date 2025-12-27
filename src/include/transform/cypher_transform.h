#ifndef CYPHER_TRANSFORM_H
#define CYPHER_TRANSFORM_H

#include "graphqlite_sqlite.h"
#include "parser/cypher_ast.h"
#include "transform/transform_variables.h"
#include "transform/sql_builder.h"

/* Forward declarations */
typedef struct cypher_transform_context cypher_transform_context;
typedef struct cypher_query_result cypher_query_result;

/* Path types for shortest path support */
typedef enum {
    TRANSFORM_PATH_NORMAL,          /* Regular path matching */
    TRANSFORM_PATH_SHORTEST,        /* shortestPath() - single shortest path */
    TRANSFORM_PATH_ALL_SHORTEST     /* allShortestPaths() - all paths of minimum length */
} transform_path_type;

/* Transform context - tracks state during AST transformation */
struct cypher_transform_context {
    sqlite3 *db;                    /* SQLite database connection */

    /* Unified variable tracking (includes path variables) */
    transform_var_context *var_ctx;

    /* SQL generation */
    char *sql_buffer;               /* Generated SQL query */
    size_t sql_size;
    size_t sql_capacity;

    /* CTE count for generating unique CTE names */
    int cte_count;

    /* Parameter tracking for parameterized queries */
    char **param_names;             /* Parameter names in order of appearance */
    int param_count;
    int param_capacity;

    /* Error handling */
    bool has_error;
    char *error_message;
    
    /* Context flags */
    bool in_comparison;             /* True when transforming expressions in comparison context */
    bool in_union;                  /* True when transforming UNION branches (skip buffer reset) */
    
    /* Unique alias counters */
    int global_alias_counter;       /* Global counter for all unnamed entities (like AGE) */
    
    /* Query type tracking */
    enum {
        QUERY_TYPE_UNKNOWN,
        QUERY_TYPE_READ,            /* MATCH, RETURN */
        QUERY_TYPE_WRITE,           /* CREATE, SET, DELETE */
        QUERY_TYPE_MIXED            /* Both read and write */
    } query_type;

    /* Unified SQL builder for clause-based SQL generation */
    sql_builder *unified_builder;
};

/* Result structure for executed queries */
struct cypher_query_result {
    /* Result data */
    sqlite3_stmt *stmt;             /* Prepared statement (for reads) */
    int rows_affected;              /* For write operations */
    
    /* Column information */
    char **column_names;
    int column_count;
    
    /* Error information */
    bool has_error;
    char *error_message;
};

/* Transform context management */
cypher_transform_context* cypher_transform_create_context(sqlite3 *db);
void cypher_transform_free_context(cypher_transform_context *ctx);

/* Main transform entry point */
cypher_query_result* cypher_transform_query(cypher_transform_context *ctx, cypher_query *query);

/* Generate SQL only (for EXPLAIN) - returns 0 on success, -1 on error */
int cypher_transform_generate_sql(cypher_transform_context *ctx, cypher_query *query);

/* Individual clause transformers */
int transform_match_clause(cypher_transform_context *ctx, cypher_match *match);
int transform_create_clause(cypher_transform_context *ctx, cypher_create *create);
int transform_set_clause(cypher_transform_context *ctx, cypher_set *set);
int transform_delete_clause(cypher_transform_context *ctx, cypher_delete *delete_clause);
int transform_remove_clause(cypher_transform_context *ctx, cypher_remove *remove);
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret);
int transform_with_clause(cypher_transform_context *ctx, cypher_with *with);
int transform_unwind_clause(cypher_transform_context *ctx, cypher_unwind *unwind);
int transform_foreach_clause(cypher_transform_context *ctx, cypher_foreach *foreach);
int transform_load_csv_clause(cypher_transform_context *ctx, cypher_load_csv *load_csv);
int transform_where_clause(cypher_transform_context *ctx, ast_node *where);

/* Pattern transformers */
int transform_node_pattern(cypher_transform_context *ctx, cypher_node_pattern *node);
int transform_rel_pattern(cypher_transform_context *ctx, cypher_rel_pattern *rel);
int transform_path_pattern(cypher_transform_context *ctx, cypher_path *path);

/* Expression transformers */
int transform_expression(cypher_transform_context *ctx, ast_node *expr);
int transform_property_access(cypher_transform_context *ctx, cypher_property *prop);
int transform_label_expression(cypher_transform_context *ctx, cypher_label_expr *label_expr);
int transform_not_expression(cypher_transform_context *ctx, cypher_not_expr *not_expr);
int transform_null_check(cypher_transform_context *ctx, cypher_null_check *null_check);
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op);
int transform_exists_expression(cypher_transform_context *ctx, cypher_exists_expr *exists_expr);
int transform_function_call(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_type_function(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_count_function(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_aggregate_function(cypher_transform_context *ctx, cypher_function_call *func_call);

/* Alias generation */
char* get_next_default_alias(cypher_transform_context *ctx);

/* Path variable registration (uses unified transform_var system) */
int register_path_variable(cypher_transform_context *ctx, const char *name, cypher_path *path);

/* SQL generation helpers */
void append_sql(cypher_transform_context *ctx, const char *format, ...);
void append_identifier(cypher_transform_context *ctx, const char *name);
void append_string_literal(cypher_transform_context *ctx, const char *value);

/* Parameter tracking */
int register_parameter(cypher_transform_context *ctx, const char *name);

/* SQL builder finalization - assembles unified_builder into sql_buffer */
int finalize_sql_generation(cypher_transform_context *ctx);

/* Variable-length relationship SQL generation */
int generate_varlen_cte(cypher_transform_context *ctx, cypher_rel_pattern *rel,
                       const char *source_alias, const char *target_alias,
                       const char *cte_name);
void prepend_cte_to_sql(cypher_transform_context *ctx);

/* Result management */
void cypher_free_result(cypher_query_result *result);
bool cypher_result_next(cypher_query_result *result);
const char* cypher_result_get_string(cypher_query_result *result, int column);
int cypher_result_get_int(cypher_query_result *result, int column);

#endif /* CYPHER_TRANSFORM_H */