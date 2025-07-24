#ifndef CYPHER_EXECUTOR_H
#define CYPHER_EXECUTOR_H

#include <sqlite3.h>
#include <stdbool.h>

#include "executor/cypher_schema.h"
#include "parser/cypher_parser.h"
#include "transform/cypher_transform.h"

/* Forward declarations */
typedef struct cypher_executor cypher_executor;

/* Execution result structure */
typedef struct cypher_result {
    bool success;
    char *error_message;
    
    /* Result data for queries that return data */
    int row_count;
    int column_count;
    char **column_names;
    char ***data; /* 2D array: data[row][column] */
    
    /* Statistics for modification queries */
    int nodes_created;
    int nodes_deleted;
    int relationships_created;
    int relationships_deleted;
    int properties_set;
} cypher_result;

/* Execution engine - coordinates parser, transformer, and schema manager */
struct cypher_executor {
    sqlite3 *db;
    cypher_schema_manager *schema_mgr;
    bool schema_initialized;
};

/* Executor lifecycle */
cypher_executor* cypher_executor_create(sqlite3 *db);
void cypher_executor_free(cypher_executor *executor);

/* Query execution */
cypher_result* cypher_executor_execute(cypher_executor *executor, const char *query);
cypher_result* cypher_executor_execute_ast(cypher_executor *executor, ast_node *ast);

/* Result management */
void cypher_result_free(cypher_result *result);
void cypher_result_print(cypher_result *result);

/* Utility functions */
bool cypher_executor_is_ready(cypher_executor *executor);
const char* cypher_executor_get_last_error(cypher_executor *executor);

#endif /* CYPHER_EXECUTOR_H */