/*
 * GraphQLite SQLite Extension
 * Adds cypher() function to SQLite for executing Cypher queries
 */

#include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

/* Extension metadata */
#define GRAPHQLITE_VERSION "0.1.0"

/* Create executor for this database connection */
static cypher_executor* create_executor(sqlite3 *db)
{
    return cypher_executor_create(db);
}

/* cypher(query) - Execute a Cypher query and return JSON results */
static void cypher_function(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
)
{
    if (argc != 1) {
        sqlite3_result_error(context, "cypher() requires exactly one argument", -1);
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
    
    /* Get database connection */
    sqlite3 *db = sqlite3_context_db_handle(context);
    
    /* Create GraphQLite executor */
    cypher_executor *executor = create_executor(db);
    if (!executor) {
        sqlite3_result_error(context, "Failed to initialize GraphQLite", -1);
        return;
    }
    
    /* Execute Cypher query */
    cypher_result *result = cypher_executor_execute(executor, query);
    if (!result) {
        sqlite3_result_error(context, "Failed to execute Cypher query", -1);
        cypher_executor_free(executor);
        return;
    }
    
    if (!result->success) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Cypher execution failed: %s", 
                 result->error_message ? result->error_message : "Unknown error");
        sqlite3_result_error(context, error_msg, -1);
        cypher_result_free(result);
        cypher_executor_free(executor);
        return;
    }
    
    /* Format result as JSON */
    char *json_result = sqlite3_malloc(4096);
    if (!json_result) {
        sqlite3_result_error_nomem(context);
        cypher_result_free(result);
        cypher_executor_free(executor);
        return;
    }
    
    /* Build JSON response */
    int pos = 0;
    pos += snprintf(json_result + pos, 4096 - pos, "{");
    pos += snprintf(json_result + pos, 4096 - pos, "\"success\":true,");
    pos += snprintf(json_result + pos, 4096 - pos, "\"nodes_created\":%d,", result->nodes_created);
    pos += snprintf(json_result + pos, 4096 - pos, "\"relationships_created\":%d,", result->relationships_created);
    pos += snprintf(json_result + pos, 4096 - pos, "\"properties_set\":%d,", result->properties_set);
    pos += snprintf(json_result + pos, 4096 - pos, "\"rows\":");
    
    if (result->row_count > 0 && result->column_count > 0) {
        pos += snprintf(json_result + pos, 4096 - pos, "[");
        for (int row = 0; row < result->row_count; row++) {
            if (row > 0) pos += snprintf(json_result + pos, 4096 - pos, ",");
            pos += snprintf(json_result + pos, 4096 - pos, "{");
            
            for (int col = 0; col < result->column_count; col++) {
                if (col > 0) pos += snprintf(json_result + pos, 4096 - pos, ",");
                pos += snprintf(json_result + pos, 4096 - pos, "\"%s\":\"%s\"",
                               result->column_names[col] ? result->column_names[col] : "null",
                               result->data[row][col] ? result->data[row][col] : "null");
            }
            pos += snprintf(json_result + pos, 4096 - pos, "}");
        }
        pos += snprintf(json_result + pos, 4096 - pos, "]");
    } else {
        pos += snprintf(json_result + pos, 4096 - pos, "[]");
    }
    
    pos += snprintf(json_result + pos, 4096 - pos, "}");
    
    /* Return JSON result */
    sqlite3_result_text(context, json_result, -1, sqlite3_free);
    
    /* Cleanup */
    cypher_result_free(result);
    cypher_executor_free(executor);
}

/* cypher_info() - Return information about GraphQLite */
static void cypher_info_function(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
)
{
    (void)argc;
    (void)argv;
    
    char *info = sqlite3_malloc(256);
    if (!info) {
        sqlite3_result_error_nomem(context);
        return;
    }
    
    snprintf(info, 256, "{\"version\":\"%s\",\"description\":\"GraphQLite Cypher Extension\"}", 
             GRAPHQLITE_VERSION);
    
    sqlite3_result_text(context, info, -1, sqlite3_free);
}

/* cypher_schema() - Show GraphQLite schema information */
static void cypher_schema_function(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
)
{
    (void)argc;
    (void)argv;
    
    /* Get database connection */
    sqlite3 *db = sqlite3_context_db_handle(context);
    
    /* Create GraphQLite executor to ensure schema exists */
    cypher_executor *executor = create_executor(db);
    if (!executor) {
        sqlite3_result_error(context, "Failed to initialize GraphQLite", -1);
        return;
    }
    
    /* Free executor immediately since we just needed it for schema setup */
    cypher_executor_free(executor);
    
    /* Query table information */
    const char *sql = "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'nodes' OR name LIKE 'edges' ORDER BY name";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_result_error(context, "Failed to query schema", -1);
        return;
    }
    
    char *schema = sqlite3_malloc(1024);
    if (!schema) {
        sqlite3_finalize(stmt);
        sqlite3_result_error_nomem(context);
        return;
    }
    
    int pos = 0;
    pos += snprintf(schema + pos, 1024 - pos, "{\"tables\":[");
    
    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) pos += snprintf(schema + pos, 1024 - pos, ",");
        const char *table_name = (const char*)sqlite3_column_text(stmt, 0);
        pos += snprintf(schema + pos, 1024 - pos, "\"%s\"", table_name);
        first = false;
    }
    
    pos += snprintf(schema + pos, 1024 - pos, "]}");
    
    sqlite3_finalize(stmt);
    sqlite3_result_text(context, schema, -1, sqlite3_free);
}

/* Extension entry point */
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_graphqlite_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
)
{
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;  /* Unused parameter */
    
    /* Register the cypher() function */
    rc = sqlite3_create_function(db, "cypher", 1, SQLITE_UTF8, NULL,
                                cypher_function, NULL, NULL);
    if (rc != SQLITE_OK) {
        return rc;
    }
    
    /* Register the cypher_info() function */
    rc = sqlite3_create_function(db, "cypher_info", 0, SQLITE_UTF8, NULL,
                                cypher_info_function, NULL, NULL);
    if (rc != SQLITE_OK) {
        return rc;
    }
    
    /* Register the cypher_schema() function */
    rc = sqlite3_create_function(db, "cypher_schema", 0, SQLITE_UTF8, NULL,
                                cypher_schema_function, NULL, NULL);
    if (rc != SQLITE_OK) {
        return rc;
    }
    
    return SQLITE_OK;
}

/* Standard entry point for auto-loading */
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_extension_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
)
{
    return sqlite3_graphqlite_init(db, pzErrMsg, pApi);
}