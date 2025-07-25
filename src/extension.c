/*
 * GraphQLite SQLite Extension
 * Based on working old architecture pattern
 */

#include <sqlite3ext.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "executor/cypher_schema.h"
#include "executor/cypher_executor.h"
#include "executor/agtype.h"
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
        if (result->row_count > 0 && result->use_agtype && result->agtype_data) {
            /* Use AGE-compatible format */
            if (result->row_count == 1 && result->column_count == 1) {
                /* Single result - return just the value */
                char *agtype_str = agtype_value_to_string(result->agtype_data[0][0]);
                sqlite3_result_text(context, agtype_str, -1, SQLITE_TRANSIENT);
                free(agtype_str);
            } else {
                /* Multiple results - return as JSON array */
                int buffer_size = 1024;
                for (int row = 0; row < result->row_count; row++) {
                    for (int col = 0; col < result->column_count; col++) {
                        if (result->agtype_data[row][col]) {
                            char *temp_str = agtype_value_to_string(result->agtype_data[row][col]);
                            if (temp_str) {
                                buffer_size += strlen(temp_str) + 20;
                                free(temp_str);
                            }
                        }
                    }
                }
                
                char *json_result = malloc(buffer_size);
                if (!json_result) {
                    sqlite3_result_error(context, "Memory allocation failed for agtype result formatting", -1);
                    cypher_result_free(result);
                    cypher_executor_free(executor);
                    return;
                }
                
                strcpy(json_result, "[");
                
                for (int row = 0; row < result->row_count; row++) {
                    if (row > 0) strcat(json_result, ",");
                    
                    if (result->column_count == 1) {
                        /* Single column - just append the agtype value */
                        char *agtype_str = agtype_value_to_string(result->agtype_data[row][0]);
                        if (agtype_str) {
                            strcat(json_result, agtype_str);
                            free(agtype_str);
                        } else {
                            strcat(json_result, "null");
                        }
                    } else {
                        /* Multiple columns - create object */
                        strcat(json_result, "{");
                        for (int col = 0; col < result->column_count; col++) {
                            if (col > 0) strcat(json_result, ",");
                            
                            strcat(json_result, "\"");
                            if (result->column_names && result->column_names[col]) {
                                strcat(json_result, result->column_names[col]);
                            } else {
                                char col_name[32];
                                snprintf(col_name, sizeof(col_name), "column_%d", col);
                                strcat(json_result, col_name);
                            }
                            strcat(json_result, "\":");
                            
                            char *agtype_str = agtype_value_to_string(result->agtype_data[row][col]);
                            if (agtype_str) {
                                strcat(json_result, agtype_str);
                                free(agtype_str);
                            } else {
                                strcat(json_result, "null");
                            }
                        }
                        strcat(json_result, "}");
                    }
                }
                strcat(json_result, "]");
                
                sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
                free(json_result);
            }
        } else if (result->row_count > 0 && result->data) {
            /* Fallback to legacy JSON format */
            int buffer_size = 1024;
            for (int row = 0; row < result->row_count; row++) {
                for (int col = 0; col < result->column_count; col++) {
                    if (result->data[row][col]) {
                        buffer_size += strlen(result->data[row][col]) + 20;
                    }
                }
            }
            
            char *json_result = malloc(buffer_size);
            if (!json_result) {
                sqlite3_result_error(context, "Memory allocation failed for result formatting", -1);
                cypher_result_free(result);
                cypher_executor_free(executor);
                return;
            }
            
            strcpy(json_result, "[");
            
            for (int row = 0; row < result->row_count; row++) {
                if (row > 0) strcat(json_result, ",");
                strcat(json_result, "{");
                
                for (int col = 0; col < result->column_count; col++) {
                    if (col > 0) strcat(json_result, ",");
                    
                    strcat(json_result, "\"");
                    if (result->column_names && result->column_names[col]) {
                        strcat(json_result, result->column_names[col]);
                    } else {
                        char col_name[32];
                        snprintf(col_name, sizeof(col_name), "column_%d", col);
                        strcat(json_result, col_name);
                    }
                    strcat(json_result, "\":\"");
                    
                    if (result->data[row][col]) {
                        strcat(json_result, result->data[row][col]);
                    } else {
                        strcat(json_result, "null");
                    }
                    strcat(json_result, "\"");
                }
                strcat(json_result, "}");
            }
            strcat(json_result, "]");
            
            sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
            free(json_result);
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