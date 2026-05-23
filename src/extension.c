/*
 * GraphQLite SQLite Extension
 * Based on working old architecture pattern
 */

#include <sqlite3ext.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <time.h>
#include <stdbool.h>

/* MSYS2/MinGW's <time.h> doesn't expose `timegm`; the Microsoft CRT
 * equivalent is `_mkgmtime`. Provide a portable alias so the temporal
 * UDFs compile on Windows. */
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
#  define timegm(tm) _mkgmtime(tm)
#endif
#include "executor/cypher_schema.h"
#include "executor/cypher_executor.h"
#include "executor/agtype.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_parser.h"
#include "parser/cypher_debug.h"
#include "runtime/udf_register.h"
#include "runtime/gql_error.h"

/*
 * Define the sqlite3_api pointer with external linkage.
 * SQLITE_EXTENSION_INIT1 defines this as static, but we need it accessible
 * from other compilation units when building as an extension.
 * Other files access this via SQLITE_EXTENSION_INIT3 (extern declaration)
 * in graphqlite_sqlite.h.
 */
const sqlite3_api_routines *sqlite3_api = 0;

/* Error helper + GQL_ERR_* macros: moved to runtime/gql_error.h (I-0040 M6) */

/* Per-connection executor cache structure */
typedef struct {
    sqlite3 *db;
    cypher_executor *executor;
    csr_graph *cached_graph;  /* Cached CSR graph for algorithm acceleration */
} connection_cache;

/* Destructor called when database connection closes */
static void connection_cache_destroy(void *data) {
    connection_cache *cache = (connection_cache *)data;
    if (cache) {
        if (cache->cached_graph) {
            CYPHER_DEBUG("Connection closing - freeing cached graph %p", (void*)cache->cached_graph);
            csr_graph_free(cache->cached_graph);
        }
        if (cache->executor) {
            CYPHER_DEBUG("Connection closing - freeing executor %p", (void*)cache->executor);
            cypher_executor_free(cache->executor);
        }
        free(cache);
    }
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
        graphqlite_result_error(context, "cypher() requires 1 or 2 arguments: (query) or (query, params_json)", GQL_ERR_VALIDATION);
        return;
    }

    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
        graphqlite_result_error(context, "cypher() first argument (query) must be text", GQL_ERR_VALIDATION);
        return;
    }

    const char *query = (const char*)sqlite3_value_text(argv[0]);
    if (!query) {
        graphqlite_result_error(context, "cypher() query cannot be null", GQL_ERR_VALIDATION);
        return;
    }

    /* Optional parameters JSON */
    const char *params_json = NULL;
    if (argc == 2) {
        if (sqlite3_value_type(argv[1]) == SQLITE_NULL) {
            /* NULL is allowed - treat as no params */
        } else if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) {
            graphqlite_result_error(context, "cypher() second argument (params) must be JSON text or NULL", GQL_ERR_VALIDATION);
            return;
        } else {
            params_json = (const char*)sqlite3_value_text(argv[1]);
        }
    }

    /* Get database connection from SQLite context */
    sqlite3 *db = sqlite3_context_db_handle(context);

    /* Get per-connection cache from user data */
    connection_cache *cache = (connection_cache *)sqlite3_user_data(context);
    cypher_executor *executor = NULL;

    if (cache && cache->executor) {
        /* Reuse cached executor for this connection */
        executor = cache->executor;
        CYPHER_DEBUG("Reusing cached executor %p", (void*)executor);
    } else {
        /* First call - create new executor */
        CYPHER_DEBUG("Creating new executor for db=%p", (void*)db);
        executor = cypher_executor_create(db);
        if (!executor) {
            graphqlite_result_error(context, "Failed to create cypher executor", GQL_ERR_INTERNAL);
            return;
        }

        /* Cache for reuse */
        if (cache) {
            cache->executor = executor;
        }
    }

    /* Ensure executor has current cached graph reference */
    if (cache) {
        executor->cached_graph = cache->cached_graph;
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
        graphqlite_result_error(context, "Failed to execute cypher query", GQL_ERR_EXECUTION);
        return;
    }
    
    /* Format result based on success/failure */
    if (result->success) {
        if (result->row_count > 0 && result->use_agtype && result->agtype_data) {
            /* Use AGE-compatible format */
            if (result->row_count == 1 && result->column_count == 1) {
                /* Single result - wrap with column name for consistent format.
                 * T-0309: when agtype cell is unset (varlen edge JSON array
                 * lives in result->data verbatim), agtype_value_to_string(NULL)
                 * returns 'null' — we want the text data instead. */
                int use_text_fallback = (result->agtype_data[0][0] == NULL &&
                                          result->data && result->data[0] && result->data[0][0]);
                char *agtype_str = use_text_fallback ? NULL : agtype_value_to_string(result->agtype_data[0][0]);
                const char *text_val = use_text_fallback ? result->data[0][0] : NULL;
                if (agtype_str || text_val) {
                    const char *render = agtype_str ? agtype_str : text_val;
                    const char *col_name = (result->column_names && result->column_names[0])
                        ? result->column_names[0] : "result";
                    int json_size = strlen(render) + strlen(col_name) + 32;
                    char *json_result = malloc(json_size);
                    if (json_result) {
                        snprintf(json_result, json_size, "[{\"%s\": %s}]", col_name, render);
                        sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
                        free(json_result);
                    } else {
                        sqlite3_result_text(context, render, -1, SQLITE_TRANSIENT);
                    }
                    free(agtype_str);
                } else {
                    sqlite3_result_text(context, "[{\"result\": null}]", -1, SQLITE_STATIC);
                }
            } else {
                /* Multiple results - return as JSON array */
                size_t buffer_size = 1024;
                for (int row = 0; row < result->row_count; row++) {
                    for (int col = 0; col < result->column_count; col++) {
                        if (result->agtype_data[row][col]) {
                            char *temp_str = agtype_value_to_string(result->agtype_data[row][col]);
                            if (temp_str) {
                                buffer_size += strlen(temp_str) + 20;
                                free(temp_str);
                            }
                        } else if (result->data && result->data[row] && result->data[row][col]) {
                            /* T-0309: include text fallback data in sizing. */
                            buffer_size += strlen(result->data[row][col]) + 20;
                        }
                    }
                }

                char *json_result = malloc(buffer_size);
                if (!json_result) {
                    graphqlite_result_error(context, "Memory allocation failed for agtype result formatting", GQL_ERR_MEMORY);
                    cypher_result_free(result);
                    return;
                }
                size_t offset = 0;

                offset += snprintf(json_result + offset, buffer_size - offset, "[");

                for (int row = 0; row < result->row_count; row++) {
                    if (row > 0) offset += snprintf(json_result + offset, buffer_size - offset, ",");

                    if (result->column_count == 1) {
                        /* Single column - wrap with column name for consistent format */
                        offset += snprintf(json_result + offset, buffer_size - offset, "{\"");
                        if (result->column_names && result->column_names[0]) {
                            size_t slen = strlen(result->column_names[0]);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, result->column_names[0], slen); offset += slen; }
                        } else {
                            offset += snprintf(json_result + offset, buffer_size - offset, "result");
                        }
                        offset += snprintf(json_result + offset, buffer_size - offset, "\":");
                        /* T-0309: when agtype cell unset (varlen edge JSON
                         * array in result->data), fall back to text data
                         * verbatim. agtype_value_to_string(NULL) returns
                         * 'null' — so the fallback must check the agtype
                         * pointer itself, not the rendered string. */
                        if (result->agtype_data[row][0] == NULL &&
                            result->data && result->data[row] && result->data[row][0]) {
                            const char *txt = result->data[row][0];
                            size_t slen = strlen(txt);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, txt, slen); offset += slen; }
                        } else {
                            char *agtype_str = agtype_value_to_string(result->agtype_data[row][0]);
                            if (agtype_str) {
                                size_t slen = strlen(agtype_str);
                                if (offset + slen < buffer_size) { memcpy(json_result + offset, agtype_str, slen); offset += slen; }
                                free(agtype_str);
                            } else {
                                offset += snprintf(json_result + offset, buffer_size - offset, "null");
                            }
                        }
                        offset += snprintf(json_result + offset, buffer_size - offset, "}");
                    } else {
                        /* Multiple columns - create object */
                        offset += snprintf(json_result + offset, buffer_size - offset, "{");
                        for (int col = 0; col < result->column_count; col++) {
                            if (col > 0) offset += snprintf(json_result + offset, buffer_size - offset, ",");

                            offset += snprintf(json_result + offset, buffer_size - offset, "\"");
                            if (result->column_names && result->column_names[col]) {
                                size_t slen = strlen(result->column_names[col]);
                                if (offset + slen < buffer_size) { memcpy(json_result + offset, result->column_names[col], slen); offset += slen; }
                            } else {
                                char col_name[32];
                                snprintf(col_name, sizeof(col_name), "column_%d", col);
                                size_t slen = strlen(col_name);
                                if (offset + slen < buffer_size) { memcpy(json_result + offset, col_name, slen); offset += slen; }
                            }
                            offset += snprintf(json_result + offset, buffer_size - offset, "\":");

                            /* T-0309: agtype_value_to_string(NULL) returns
                             * 'null' — so the text-fallback path must be
                             * gated on agtype_data being NULL, not the
                             * return value. */
                            if (result->agtype_data[row][col] == NULL &&
                                result->data && result->data[row] && result->data[row][col]) {
                                const char *txt = result->data[row][col];
                                size_t slen = strlen(txt);
                                if (offset + slen < buffer_size) { memcpy(json_result + offset, txt, slen); offset += slen; }
                            } else {
                                char *agtype_str = agtype_value_to_string(result->agtype_data[row][col]);
                                if (agtype_str) {
                                    size_t slen = strlen(agtype_str);
                                    if (offset + slen < buffer_size) { memcpy(json_result + offset, agtype_str, slen); offset += slen; }
                                    free(agtype_str);
                                } else {
                                    offset += snprintf(json_result + offset, buffer_size - offset, "null");
                                }
                            }
                        }
                        offset += snprintf(json_result + offset, buffer_size - offset, "}");
                    }
                }
                offset += snprintf(json_result + offset, buffer_size - offset, "]");
                json_result[offset] = '\0';

                sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
                free(json_result);
            }
        } else if (result->row_count > 0 && result->data) {
            /* Format results as JSON with column names */
            size_t buffer_size = 1024;
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
                graphqlite_result_error(context, "Memory allocation failed for result formatting", GQL_ERR_MEMORY);
                cypher_result_free(result);
                return;
            }
            size_t offset = 0;

            offset += snprintf(json_result + offset, buffer_size - offset, "[");

            for (int row = 0; row < result->row_count; row++) {
                if (row > 0) offset += snprintf(json_result + offset, buffer_size - offset, ",");
                offset += snprintf(json_result + offset, buffer_size - offset, "{");

                for (int col = 0; col < result->column_count; col++) {
                    if (col > 0) offset += snprintf(json_result + offset, buffer_size - offset, ",");

                    offset += snprintf(json_result + offset, buffer_size - offset, "\"");
                    if (result->column_names && result->column_names[col]) {
                        size_t slen = strlen(result->column_names[col]);
                        if (offset + slen < buffer_size) { memcpy(json_result + offset, result->column_names[col], slen); offset += slen; }
                    } else {
                        char col_name[32];
                        snprintf(col_name, sizeof(col_name), "column_%d", col);
                        size_t slen = strlen(col_name);
                        if (offset + slen < buffer_size) { memcpy(json_result + offset, col_name, slen); offset += slen; }
                    }
                    offset += snprintf(json_result + offset, buffer_size - offset, "\":");

                    if (result->data[row][col]) {
                        const char *val = result->data[row][col];
                        /* Get SQLite type if available */
                        int col_type = SQLITE_TEXT;
                        if (result->data_types && result->data_types[row]) {
                            col_type = result->data_types[row][col];
                        }

                        /* Check if value is already JSON (starts with [, { or "). */
                        if (val[0] == '[' || val[0] == '{' || val[0] == '"') {
                            size_t slen = strlen(val);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, val, slen); offset += slen; }
                        } else if (col_type == SQLITE_INTEGER) {
                            size_t slen = strlen(val);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, val, slen); offset += slen; }
                        } else if (col_type == SQLITE_FLOAT) {
                            /* Cypher distinguishes Float from Integer in result
                             * type. Whole-number floats must render with a
                             * trailing ".0" so 20.0 doesn't look like the
                             * integer 20. Append ".0" if val has no decimal
                             * point or exponent marker. */
                            size_t slen = strlen(val);
                            if (offset + slen < buffer_size) {
                                memcpy(json_result + offset, val, slen);
                                offset += slen;
                            }
                            bool has_frac = false;
                            for (const char *p = val; *p; p++) {
                                if (*p == '.' || *p == 'e' || *p == 'E') { has_frac = true; break; }
                            }
                            if (!has_frac && offset + 2 < buffer_size) {
                                json_result[offset++] = '.';
                                json_result[offset++] = '0';
                            }
                        } else if (col_type == GQL_COL_TYPE_BOOLEAN) {
                            /* UDF-tagged boolean — emit unquoted JSON
                             * true/false. Replaces the historical
                             * strcmp(val, "true") heuristic which mis-tagged
                             * legitimate string values "true"/"false"
                             * (e.g. toString(true) result). I-0040 M13. */
                            size_t slen = strlen(val);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, val, slen); offset += slen; }
                        } else {
                            /* String value - quote and escape */
                            offset += snprintf(json_result + offset, buffer_size - offset, "\"");
                            while (*val) {
                                if (*val == '"' || *val == '\\') {
                                    if (offset < buffer_size) json_result[offset++] = '\\';
                                }
                                if (offset < buffer_size) json_result[offset++] = *val;
                                val++;
                            }
                            offset += snprintf(json_result + offset, buffer_size - offset, "\"");
                        }
                    } else {
                        offset += snprintf(json_result + offset, buffer_size - offset, "null");
                    }
                }
                offset += snprintf(json_result + offset, buffer_size - offset, "}");
            }
            offset += snprintf(json_result + offset, buffer_size - offset, "]");
            json_result[offset] = '\0';

            sqlite3_result_text(context, json_result, -1, SQLITE_TRANSIENT);
            free(json_result);
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
        const char *err_code = GQL_ERR_EXECUTION;
        if (result->error_message && (strstr(result->error_message, "syntax error") || strstr(result->error_message, "Line "))) {
            err_code = GQL_ERR_PARSE;
        } else if (result->error_message && strstr(result->error_message, "not yet implemented")) {
            err_code = GQL_ERR_NOT_IMPL;
        }
        const char *err_msg = result->error_message ? result->error_message : "Query execution failed";
        /* Sanitize double quotes in dynamic error messages to avoid breaking JSON */
        char sanitized_msg[512];
        const char *src = err_msg;
        char *dst = sanitized_msg;
        char *end = sanitized_msg + sizeof(sanitized_msg) - 1;
        while (*src && dst < end) {
            *dst++ = (*src == '"') ? '\'' : *src;
            src++;
        }
        *dst = '\0';
        graphqlite_result_error(context, sanitized_msg, err_code);
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
 * Graph Cache Management Functions
 * Provide per-connection CSR graph caching for algorithm acceleration.
 */

/* gql_load_graph() - Build CSR from SQLite and cache in connection memory */
static void gql_load_graph_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;

    connection_cache *cache = (connection_cache *)sqlite3_user_data(context);
    if (!cache) {
        graphqlite_result_error(context, "No connection cache available", GQL_ERR_INTERNAL);
        return;
    }

    sqlite3 *db = sqlite3_context_db_handle(context);

    /* If already loaded, return current stats */
    if (cache->cached_graph) {
        char response[256];
        snprintf(response, sizeof(response),
                 "{\"status\":\"already_loaded\",\"nodes\":%d,\"edges\":%d}",
                 cache->cached_graph->node_count,
                 cache->cached_graph->edge_count);
        sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
        return;
    }

    /* Load graph from SQLite */
    csr_graph *graph = csr_graph_load(db);
    if (!graph) {
        sqlite3_result_text(context, "{\"status\":\"loaded\",\"nodes\":0,\"edges\":0}", -1, SQLITE_STATIC);
        return;
    }

    /* Cache the graph */
    cache->cached_graph = graph;

    /* Also update executor if it exists */
    if (cache->executor) {
        cache->executor->cached_graph = graph;
    }

    char response[256];
    snprintf(response, sizeof(response),
             "{\"status\":\"loaded\",\"nodes\":%d,\"edges\":%d}",
             graph->node_count, graph->edge_count);
    sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
}

/* gql_unload_graph() - Free cached graph memory */
static void gql_unload_graph_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;

    connection_cache *cache = (connection_cache *)sqlite3_user_data(context);
    if (!cache) {
        graphqlite_result_error(context, "No connection cache available", GQL_ERR_INTERNAL);
        return;
    }

    if (cache->cached_graph) {
        csr_graph_free(cache->cached_graph);
        cache->cached_graph = NULL;

        /* Also clear executor reference */
        if (cache->executor) {
            cache->executor->cached_graph = NULL;
        }

        sqlite3_result_text(context, "{\"status\":\"unloaded\"}", -1, SQLITE_STATIC);
    } else {
        sqlite3_result_text(context, "{\"status\":\"not_loaded\"}", -1, SQLITE_STATIC);
    }
}

/* gql_reload_graph() - Invalidate cache and rebuild from current database state */
static void gql_reload_graph_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;

    connection_cache *cache = (connection_cache *)sqlite3_user_data(context);
    if (!cache) {
        graphqlite_result_error(context, "No connection cache available", GQL_ERR_INTERNAL);
        return;
    }

    sqlite3 *db = sqlite3_context_db_handle(context);

    int prev_nodes = 0, prev_edges = 0;

    /* Free existing cache if present */
    if (cache->cached_graph) {
        prev_nodes = cache->cached_graph->node_count;
        prev_edges = cache->cached_graph->edge_count;
        csr_graph_free(cache->cached_graph);
        cache->cached_graph = NULL;
    }

    /* Load fresh graph from SQLite */
    csr_graph *graph = csr_graph_load(db);
    cache->cached_graph = graph;

    /* Also update executor if it exists */
    if (cache->executor) {
        cache->executor->cached_graph = graph;
    }

    int new_nodes = graph ? graph->node_count : 0;
    int new_edges = graph ? graph->edge_count : 0;

    char response[512];
    snprintf(response, sizeof(response),
             "{\"status\":\"reloaded\",\"previous_nodes\":%d,\"previous_edges\":%d,\"nodes\":%d,\"edges\":%d}",
             prev_nodes, prev_edges, new_nodes, new_edges);
    sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
}

/* gql_graph_loaded() - Return cache status */
static void gql_graph_loaded_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;

    connection_cache *cache = (connection_cache *)sqlite3_user_data(context);
    if (!cache) {
        graphqlite_result_error(context, "No connection cache available", GQL_ERR_INTERNAL);
        return;
    }

    if (cache->cached_graph) {
        char response[256];
        snprintf(response, sizeof(response),
                 "{\"loaded\":true,\"nodes\":%d,\"edges\":%d}",
                 cache->cached_graph->node_count,
                 cache->cached_graph->edge_count);
        sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_result_text(context, "{\"loaded\":false,\"nodes\":0,\"edges\":0}", -1, SQLITE_STATIC);
    }
}

/* SQLite's `.load <path>` looks up the entry point by name from the
 * shared library's exported symbol table. On Windows DLLs, only
 * symbols marked with `__declspec(dllexport)` are exported by
 * default (or via a .def file or `--export-all-symbols`). Without
 * this, `.load build/graphqlite` fails with
 * "The specified procedure could not be found." */
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)
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

  /* Create per-connection cache that will be freed when connection closes */
  connection_cache *cache = calloc(1, sizeof(connection_cache));
  if (!cache) {
    return SQLITE_NOMEM;
  }
  cache->db = db;
  cache->executor = NULL;

  /* Register all functions — check each return value.
   * On any failure, free the cache and return the error. */

  rc = sqlite3_create_function(db, "graphqlite_test", 0, SQLITE_UTF8, 0,
                         simple_test_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function_v2(db, "cypher", -1, SQLITE_UTF8, cache,
                             graphqlite_cypher_func, 0, 0,
                             connection_cache_destroy);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = graphqlite_register_helper_udfs(db);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "gql_load_graph", 0, SQLITE_UTF8, cache,
                         gql_load_graph_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "gql_unload_graph", 0, SQLITE_UTF8, cache,
                         gql_unload_graph_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "gql_reload_graph", 0, SQLITE_UTF8, cache,
                         gql_reload_graph_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "gql_graph_loaded", 0, SQLITE_UTF8, cache,
                         gql_graph_loaded_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  /* Create schema during initialization */
  create_schema(db);

  return SQLITE_OK;
}