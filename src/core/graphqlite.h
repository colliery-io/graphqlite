#ifndef GRAPHQLITE_H
#define GRAPHQLITE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct graphqlite graphqlite_t;
typedef struct graphqlite_node graphqlite_node_t;
typedef struct graphqlite_edge graphqlite_edge_t;
typedef struct graphqlite_result graphqlite_result_t;

typedef enum {
    GRAPHQLITE_OK = 0,
    GRAPHQLITE_ERROR = 1,
    GRAPHQLITE_NOMEM = 2,
    GRAPHQLITE_NOTFOUND = 3,
    GRAPHQLITE_INVALID = 4,
    GRAPHQLITE_READONLY = 5
} graphqlite_result_code_t;

typedef enum {
    GRAPHQLITE_TYPE_NULL = 0,
    GRAPHQLITE_TYPE_INTEGER = 1,
    GRAPHQLITE_TYPE_FLOAT = 2,
    GRAPHQLITE_TYPE_TEXT = 3,
    GRAPHQLITE_TYPE_BLOB = 4,
    GRAPHQLITE_TYPE_BOOLEAN = 5
} graphqlite_value_type_t;

typedef enum {
    GRAPHQLITE_OPEN_READONLY = 0x00000001,
    GRAPHQLITE_OPEN_READWRITE = 0x00000002,
    GRAPHQLITE_OPEN_CREATE = 0x00000004
} graphqlite_open_flags_t;

graphqlite_t* graphqlite_open(const char *path, int flags);
int graphqlite_close(graphqlite_t *db);
int graphqlite_exec(graphqlite_t *db, const char *gql, graphqlite_result_t **result);

graphqlite_node_t* graphqlite_node_create(graphqlite_t *db, const char *labels[]);
int graphqlite_node_set_property(graphqlite_t *db, graphqlite_node_t *node, 
                                  const char *key, const void *value, 
                                  graphqlite_value_type_t type);
const void* graphqlite_node_get_property(graphqlite_t *db, graphqlite_node_t *node, 
                                          const char *key, graphqlite_value_type_t *type);
int graphqlite_node_delete(graphqlite_t *db, graphqlite_node_t *node);
int64_t graphqlite_node_id(graphqlite_node_t *node);

graphqlite_edge_t* graphqlite_edge_create(graphqlite_t *db, 
                                           graphqlite_node_t *source,
                                           graphqlite_node_t *target,
                                           const char *type);
int graphqlite_edge_set_property(graphqlite_t *db, graphqlite_edge_t *edge, 
                                  const char *key, const void *value, 
                                  graphqlite_value_type_t type);
const void* graphqlite_edge_get_property(graphqlite_t *db, graphqlite_edge_t *edge, 
                                          const char *key, graphqlite_value_type_t *type);
int graphqlite_edge_delete(graphqlite_t *db, graphqlite_edge_t *edge);
int64_t graphqlite_edge_id(graphqlite_edge_t *edge);
graphqlite_node_t* graphqlite_edge_source(graphqlite_edge_t *edge);
graphqlite_node_t* graphqlite_edge_target(graphqlite_edge_t *edge);

int graphqlite_result_step(graphqlite_result_t *result);
int graphqlite_result_column_count(graphqlite_result_t *result);
const char* graphqlite_result_column_name(graphqlite_result_t *result, int col);
graphqlite_value_type_t graphqlite_result_column_type(graphqlite_result_t *result, int col);
const void* graphqlite_result_column_value(graphqlite_result_t *result, int col);
int64_t graphqlite_result_column_int64(graphqlite_result_t *result, int col);
double graphqlite_result_column_double(graphqlite_result_t *result, int col);
const char* graphqlite_result_column_text(graphqlite_result_t *result, int col);
const void* graphqlite_result_column_blob(graphqlite_result_t *result, int col, int *size);
int graphqlite_result_column_bool(graphqlite_result_t *result, int col);
void graphqlite_result_free(graphqlite_result_t *result);

const char* graphqlite_version(void);
const char* graphqlite_error_message(graphqlite_t *db);
int graphqlite_error_code(graphqlite_t *db);

int graphqlite_transaction_begin(graphqlite_t *db);
int graphqlite_transaction_commit(graphqlite_t *db);
int graphqlite_transaction_rollback(graphqlite_t *db);

typedef void (*graphqlite_log_callback_t)(int level, const char *message);
void graphqlite_set_log_callback(graphqlite_log_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif