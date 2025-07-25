/*
 * GraphQLite SQLite Extension
 * Based on working old architecture pattern
 */

#include <sqlite3ext.h>
#include "executor/cypher_schema.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_parser.h"

SQLITE_EXTENSION_INIT1

/* Simple test function */
static void simple_test_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "GraphQLite extension loaded successfully!", -1, SQLITE_STATIC);
}

/* Cypher function - full implementation */
static void graphqlite_cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc != 1) {
        sqlite3_result_error(context, "cypher() function requires exactly one argument", -1);
        return;
    }
    
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "cypher() argument must be text", -1);
        return;
    }
    
    const char *query = (const char*)sqlite3_value_text(argv[0]);
    if (!query) {
        sqlite3_result_error(context, "cypher() query cannot be null", -1);
        return;
    }
    
    /* Get database connection from SQLite context */
    sqlite3 *db = sqlite3_context_db_handle(context);
    
    /* Create executor */
    cypher_executor *executor = cypher_executor_create(db);
    if (!executor) {
        sqlite3_result_error(context, "Failed to create cypher executor", -1);
        return;
    }
    
    /* Execute query */
    cypher_result *result = cypher_executor_execute(executor, query);
    if (!result) {
        cypher_executor_free(executor);
        sqlite3_result_error(context, "Failed to execute cypher query", -1);
        return;
    }
    
    /* Format result based on success/failure */
    if (result->success) {
        if (result->row_count > 0) {
            /* For now, just return success with row count */
            char response[256];
            snprintf(response, sizeof(response), "Query executed successfully - %d rows", result->row_count);
            sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
        } else {
            /* Modification query - show statistics */
            char response[256];
            snprintf(response, sizeof(response), "Query executed successfully - nodes created: %d, relationships created: %d", 
                    result->nodes_created, result->relationships_created);
            sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
        }
    } else {
        sqlite3_result_error(context, result->error_message ? result->error_message : "Query execution failed", -1);
    }
    
    /* Cleanup */
    cypher_result_free(result);
    cypher_executor_free(executor);
}

/* Create schema function matching old architecture */
static int create_schema(sqlite3 *db) {
    cypher_schema_manager *schema_manager = cypher_schema_create_manager(db);
    if (!schema_manager) {
        return -1;
    }
    
    int result = cypher_schema_initialize(schema_manager);
    cypher_schema_free_manager(schema_manager);
    
    return result;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_graphqlite_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg;  /* Unused parameter */
  
  /* Register the graphqlite_test function */
  sqlite3_create_function(db, "graphqlite_test", 0, SQLITE_UTF8, 0,
                         simple_test_func, 0, 0);
  
  /* Register the main cypher() function */
  sqlite3_create_function(db, "cypher", 1, SQLITE_UTF8, 0,
                         graphqlite_cypher_func, 0, 0);
  
  /* Create schema during initialization */
  create_schema(db);
  
  return rc;
}