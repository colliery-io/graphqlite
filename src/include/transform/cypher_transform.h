#ifndef CYPHER_TRANSFORM_H
#define CYPHER_TRANSFORM_H

#include <sqlite3.h>
#include "parser/cypher_ast.h"

/* Forward declarations */
typedef struct cypher_transform_context cypher_transform_context;
typedef struct cypher_query_result cypher_query_result;
typedef struct transform_entity transform_entity;

/* Entity types following AGE patterns */
typedef enum {
    ENTITY_TYPE_VERTEX,
    ENTITY_TYPE_EDGE
} entity_type;

/* Transform entity structure (similar to AGE's transform_entity) */
struct transform_entity {
    char *name;                     /* Variable name (e.g., "p", "r") */
    char *table_alias;              /* Generated SQL alias (e.g., "gql_default_alias_0") */
    entity_type type;               /* VERTEX or EDGE */
    bool is_current_clause;         /* True if declared in current clause, false if inherited */
};

/* Path variable metadata */
typedef struct path_variable {
    char *name;                     /* Path variable name */
    ast_list *elements;             /* AST nodes in the path (nodes and relationships) */
} path_variable;

/* Transform context - tracks state during AST transformation */
struct cypher_transform_context {
    sqlite3 *db;                    /* SQLite database connection */
    
    /* Entity tracking (AGE-style) */
    transform_entity *entities;     /* List of entities */
    int entity_count;
    int entity_capacity;
    
    /* Path variable tracking */
    path_variable *path_variables;  /* List of path variables */
    int path_variable_count;
    int path_variable_capacity;
    
    /* Legacy variable tracking (will be phased out) */
    struct {
        char *name;                 /* Variable name (e.g., "n", "m") */
        char *table_alias;          /* SQL table alias */
        int node_id;                /* For already-bound nodes */
        bool is_bound;              /* Whether variable has a value */
        enum {
            VAR_TYPE_NODE,          /* Node variable */
            VAR_TYPE_EDGE,          /* Edge/relationship variable */
            VAR_TYPE_PATH           /* Path variable */
        } type;                     /* Variable type */
    } *variables;
    int variable_count;
    int variable_capacity;
    
    /* SQL generation */
    char *sql_buffer;               /* Generated SQL query */
    size_t sql_size;
    size_t sql_capacity;
    
    /* SQL builder for two-pass generation (OPTIONAL MATCH support) */
    struct {
        char *from_clause;          /* FROM nodes AS alias */
        size_t from_size;
        size_t from_capacity;
        
        char *join_clauses;         /* LEFT JOIN ... LEFT JOIN ... */
        size_t join_size;
        size_t join_capacity;
        
        char *where_clauses;        /* WHERE ... AND ... AND ... */
        size_t where_size;
        size_t where_capacity;
        
        bool using_builder;         /* True when using two-pass generation */
    } sql_builder;
    
    /* Error handling */
    bool has_error;
    char *error_message;
    
    /* Context flags */
    bool in_comparison;             /* True when transforming expressions in comparison context */
    
    /* Unique alias counters */
    int global_alias_counter;       /* Global counter for all unnamed entities (like AGE) */
    
    /* Query type tracking */
    enum {
        QUERY_TYPE_UNKNOWN,
        QUERY_TYPE_READ,            /* MATCH, RETURN */
        QUERY_TYPE_WRITE,           /* CREATE, SET, DELETE */
        QUERY_TYPE_MIXED            /* Both read and write */
    } query_type;
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

/* Individual clause transformers */
int transform_match_clause(cypher_transform_context *ctx, cypher_match *match);
int transform_create_clause(cypher_transform_context *ctx, cypher_create *create);
int transform_set_clause(cypher_transform_context *ctx, cypher_set *set);
int transform_delete_clause(cypher_transform_context *ctx, cypher_delete *delete_clause);
int transform_return_clause(cypher_transform_context *ctx, cypher_return *ret);
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
int transform_binary_operation(cypher_transform_context *ctx, cypher_binary_op *binary_op);
int transform_function_call(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_type_function(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_count_function(cypher_transform_context *ctx, cypher_function_call *func_call);
int transform_aggregate_function(cypher_transform_context *ctx, cypher_function_call *func_call);

/* Entity management (AGE-style) */
int add_entity(cypher_transform_context *ctx, const char *name, entity_type type, bool is_current_clause);
transform_entity* lookup_entity(cypher_transform_context *ctx, const char *name);
void mark_entities_as_inherited(cypher_transform_context *ctx);
char* get_next_default_alias(cypher_transform_context *ctx);

/* Legacy variable management (will be phased out) */
int register_variable(cypher_transform_context *ctx, const char *name, const char *alias);
int register_node_variable(cypher_transform_context *ctx, const char *name, const char *alias);
int register_edge_variable(cypher_transform_context *ctx, const char *name, const char *alias);
int register_path_variable(cypher_transform_context *ctx, const char *name, cypher_path *path);
const char* lookup_variable_alias(cypher_transform_context *ctx, const char *name);
bool is_variable_bound(cypher_transform_context *ctx, const char *name);
bool is_edge_variable(cypher_transform_context *ctx, const char *name);
bool is_path_variable(cypher_transform_context *ctx, const char *name);
path_variable* get_path_variable(cypher_transform_context *ctx, const char *name);

/* SQL generation helpers */
void append_sql(cypher_transform_context *ctx, const char *format, ...);
void append_identifier(cypher_transform_context *ctx, const char *name);
void append_string_literal(cypher_transform_context *ctx, const char *value);

/* SQL builder functions for two-pass generation */
int init_sql_builder(cypher_transform_context *ctx);
void free_sql_builder(cypher_transform_context *ctx);
void append_from_clause(cypher_transform_context *ctx, const char *format, ...);
void append_join_clause(cypher_transform_context *ctx, const char *format, ...);
void append_where_clause(cypher_transform_context *ctx, const char *format, ...);
int finalize_sql_generation(cypher_transform_context *ctx);

/* Result management */
void cypher_free_result(cypher_query_result *result);
bool cypher_result_next(cypher_query_result *result);
const char* cypher_result_get_string(cypher_query_result *result, int column);
int cypher_result_get_int(cypher_query_result *result, int column);

#endif /* CYPHER_TRANSFORM_H */