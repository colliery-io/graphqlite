/*
 * GraphQLite SQLite Extension
 * Based on working old architecture pattern
 */

#include <sqlite3ext.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include "executor/cypher_schema.h"
#include "executor/cypher_executor.h"
#include "executor/agtype.h"
#include "parser/cypher_parser.h"
#include "parser/cypher_debug.h"

/*
 * Define the sqlite3_api pointer with external linkage.
 * SQLITE_EXTENSION_INIT1 defines this as static, but we need it accessible
 * from other compilation units when building as an extension.
 * Other files access this via SQLITE_EXTENSION_INIT3 (extern declaration)
 * in graphqlite_sqlite.h.
 */
const sqlite3_api_routines *sqlite3_api = 0;

/* Global executor cache - simple single-connection cache for now */
static struct {
    sqlite3 *db;
    cypher_executor *executor;
} g_executor_cache = { NULL, NULL };

/* Cleanup the global cache */
static void cleanup_global_cache(void) {
    if (g_executor_cache.executor) {
        cypher_executor_free(g_executor_cache.executor);
        g_executor_cache.executor = NULL;
    }
    g_executor_cache.db = NULL;
}

/* Simple test function */
static void simple_test_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "GraphQLite extension loaded successfully!", -1, SQLITE_STATIC);
}

/* Cypher function - full implementation with cached executor */
static void graphqlite_cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc < 1 || argc > 2) {
        sqlite3_result_error(context, "cypher() requires 1 or 2 arguments: (query) or (query, params_json)", -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        sqlite3_result_error(context, "cypher() first argument (query) must be text", -1);
        return;
    }

    const char *query = (const char*)sqlite3_value_text(argv[0]);
    if (!query) {
        sqlite3_result_error(context, "cypher() query cannot be null", -1);
        return;
    }

    /* Optional parameters JSON */
    const char *params_json = NULL;
    if (argc == 2) {
        if (sqlite3_value_type(argv[1]) == SQLITE_NULL) {
            /* NULL is allowed - treat as no params */
        } else if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
            sqlite3_result_error(context, "cypher() second argument (params) must be JSON text or NULL", -1);
            return;
        } else {
            params_json = (const char*)sqlite3_value_text(argv[1]);
        }
    }

    /* Get database connection from SQLite context */
    sqlite3 *db = sqlite3_context_db_handle(context);

    /* Get or create cached executor using global cache */
    cypher_executor *executor = NULL;

    if (g_executor_cache.db == db && g_executor_cache.executor != NULL) {
        /* Reuse cached executor for this connection */
        executor = g_executor_cache.executor;
        CYPHER_DEBUG("Reusing cached executor %p", (void*)executor);
    } else {
        /* Different connection or first call - create new executor */
        if (g_executor_cache.executor != NULL) {
            /* Clean up old executor from different connection */
            CYPHER_DEBUG("Cleaning up old executor for different connection");
            cleanup_global_cache();
        }

        CYPHER_DEBUG("Creating new executor for db=%p", (void*)db);
        executor = cypher_executor_create(db);
        if (!executor) {
            sqlite3_result_error(context, "Failed to create cypher executor", -1);
            return;
        }

        /* Cache for reuse */
        g_executor_cache.db = db;
        g_executor_cache.executor = executor;
    }

    /* Execute query (with or without parameters) */
    cypher_result *result;
    if (params_json) {
        result = cypher_executor_execute_params(executor, query, params_json);
    } else {
        result = cypher_executor_execute(executor, query);
    }
    if (!result) {
        /* Don't free cached executor on error */
        sqlite3_result_error(context, "Failed to execute cypher query", -1);
        return;
    }
    
    /* Format result based on success/failure */
    if (result->success) {
        if (result->row_count > 0 && result->use_agtype && result->agtype_data) {
            /* Use AGE-compatible format */
            if (result->row_count == 1 && result->column_count == 1) {
                /* Single result - wrap with column name for consistent format */
                char *agtype_str = agtype_value_to_string(result->agtype_data[0][0]);
                if (agtype_str) {
                    const char *col_name = (result->column_names && result->column_names[0])
                        ? result->column_names[0] : "result";
                    int json_size = strlen(agtype_str) + strlen(col_name) + 32;
                    char *json_result = malloc(json_size);
                    if (json_result) {
                        snprintf(json_result, json_size, "[{\"%s\": %s}]", col_name, agtype_str);
                        sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
                        free(json_result);
                    } else {
                        sqlite3_result_text(context, agtype_str, -1, SQLITE_TRANSIENT);
                    }
                    free(agtype_str);
                } else {
                    sqlite3_result_text(context, "[{\"result\": null}]", -1, SQLITE_STATIC);
                }
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
                    return;
                }
                
                strcpy(json_result, "[");
                
                for (int row = 0; row < result->row_count; row++) {
                    if (row > 0) strcat(json_result, ",");
                    
                    if (result->column_count == 1) {
                        /* Single column - wrap with column name for consistent format */
                        strcat(json_result, "{\"");
                        if (result->column_names && result->column_names[0]) {
                            strcat(json_result, result->column_names[0]);
                        } else {
                            strcat(json_result, "result");
                        }
                        strcat(json_result, "\":");
                        char *agtype_str = agtype_value_to_string(result->agtype_data[row][0]);
                        if (agtype_str) {
                            strcat(json_result, agtype_str);
                            free(agtype_str);
                        } else {
                            strcat(json_result, "null");
                        }
                        strcat(json_result, "}");
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
            /* Check for single-cell JSON array result (e.g., from graph algorithms) */
            if (result->row_count == 1 && result->column_count == 1 &&
                result->data[0][0] && result->data[0][0][0] == '[') {
                /* Return JSON array directly without double-wrapping */
                sqlite3_result_text(context, result->data[0][0], -1, SQLITE_TRANSIENT);
            } else {
                /* Fallback to legacy JSON format with proper escaping */
                int buffer_size = 1024;
                for (int row = 0; row < result->row_count; row++) {
                    for (int col = 0; col < result->column_count; col++) {
                        if (result->data[row][col]) {
                            /* Account for possible JSON escaping (2x size for worst case) */
                            buffer_size += strlen(result->data[row][col]) * 2 + 20;
                        }
                    }
                }

                char *json_result = malloc(buffer_size);
                if (!json_result) {
                    sqlite3_result_error(context, "Memory allocation failed for result formatting", -1);
                    cypher_result_free(result);
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
                        strcat(json_result, "\":");

                        if (result->data[row][col]) {
                            const char *val = result->data[row][col];
                            /* Get SQLite type if available */
                            int col_type = SQLITE_TEXT;
                            if (result->data_types && result->data_types[row]) {
                                col_type = result->data_types[row][col];
                            }

                            /* Check if value is already JSON (starts with [ or {) */
                            if (val[0] == '[' || val[0] == '{') {
                                strcat(json_result, val);
                            } else if (col_type == SQLITE_INTEGER || col_type == SQLITE_FLOAT) {
                                /* Numeric value - output without quotes */
                                strcat(json_result, val);
                            } else {
                                /* String value - quote and escape */
                                strcat(json_result, "\"");
                                char *p = json_result + strlen(json_result);
                                while (*val) {
                                    if (*val == '"' || *val == '\\') {
                                        *p++ = '\\';
                                    }
                                    *p++ = *val++;
                                }
                                *p = '\0';
                                strcat(json_result, "\"");
                            }
                        } else {
                            strcat(json_result, "null");
                        }
                    }
                    strcat(json_result, "}");
                }
                strcat(json_result, "]");

                sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
                free(json_result);
            }
        } else if (result->column_count > 0) {
            /* Query with RETURN clause but zero rows - return empty array */
            sqlite3_result_text(context, "[]", -1, SQLITE_STATIC);
        } else {
            /* Modification query without RETURN - show statistics */
            char response[256];
            snprintf(response, sizeof(response), "Query executed successfully - nodes created: %d, relationships created: %d",
                    result->nodes_created, result->relationships_created);
            sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
        }
    } else {
        sqlite3_result_error(context, result->error_message ? result->error_message : "Query execution failed", -1);
    }
    
    /* Cleanup - only free result, executor is cached */
    cypher_result_free(result);
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

/*
 * REGEXP function for SQLite
 * Implements the =~ operator from Cypher
 * Usage: regexp(pattern, string) returns 1 if string matches pattern, 0 otherwise
 */
static void regexp_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    const char *pattern;
    const char *string;
    regex_t regex;
    int ret;
    int cflags = REG_EXTENDED | REG_NOSUB;

    if (argc != 2) {
        sqlite3_result_error(context, "regexp() requires 2 arguments", -1);
        return;
    }

    /* Get pattern and string */
    pattern = (const char*)sqlite3_value_text(argv[0]);
    string = (const char*)sqlite3_value_text(argv[1]);

    /* Handle NULL arguments */
    if (!pattern || !string) {
        sqlite3_result_null(context);
        return;
    }

    /* Check for (?i) flag at start of pattern for case-insensitive matching */
    if (strncmp(pattern, "(?i)", 4) == 0) {
        cflags |= REG_ICASE;
        pattern += 4;  /* Skip the flag */
    }

    /* Compile the regex */
    ret = regcomp(&regex, pattern, cflags);
    if (ret != 0) {
        char errbuf[256];
        regerror(ret, &regex, errbuf, sizeof(errbuf));
        sqlite3_result_error(context, errbuf, -1);
        return;
    }

    /* Execute the regex */
    ret = regexec(&regex, string, 0, NULL, 0);
    regfree(&regex);

    /* Return result: 1 for match, 0 for no match */
    sqlite3_result_int(context, ret == 0 ? 1 : 0);
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
  
  /* Register the main cypher() function (1 or 2 args: query, optional params_json) */
  sqlite3_create_function(db, "cypher", -1, SQLITE_UTF8, 0,
                         graphqlite_cypher_func, 0, 0);

  /* Register the regexp() function for =~ operator support */
  sqlite3_create_function(db, "regexp", 2, SQLITE_UTF8, 0,
                         regexp_func, 0, 0);

  /* Create schema during initialization */
  create_schema(db);
  
  return rc;
}