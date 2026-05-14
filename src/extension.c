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
#include "executor/cypher_schema.h"
#include "executor/cypher_executor.h"
#include "executor/agtype.h"
#include "executor/graph_algorithms.h"
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

/* Error codes for structured error responses */
#define GQL_ERR_VALIDATION    "VALIDATION_ERROR"
#define GQL_ERR_PARSE         "PARSE_ERROR"
#define GQL_ERR_EXECUTION     "EXECUTION_ERROR"
#define GQL_ERR_MEMORY        "MEMORY_ERROR"
#define GQL_ERR_INTERNAL      "INTERNAL_ERROR"
#define GQL_ERR_NOT_IMPL      "NOT_IMPLEMENTED"

/* Return a structured JSON error via sqlite3_result_error.
 * Format: {"error": "message", "code": "ERROR_CODE"} */
static void graphqlite_result_error(sqlite3_context *context, const char *message, const char *code) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\",\"code\":\"%s\"}", message, code);
    sqlite3_result_error(context, buf, -1);
}

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
                size_t buffer_size = 1024;
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
                        char *agtype_str = agtype_value_to_string(result->agtype_data[row][0]);
                        if (agtype_str) {
                            size_t slen = strlen(agtype_str);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, agtype_str, slen); offset += slen; }
                            free(agtype_str);
                        } else {
                            offset += snprintf(json_result + offset, buffer_size - offset, "null");
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

                            char *agtype_str = agtype_value_to_string(result->agtype_data[row][col]);
                            if (agtype_str) {
                                size_t slen = strlen(agtype_str);
                                if (offset + slen < buffer_size) { memcpy(json_result + offset, agtype_str, slen); offset += slen; }
                                free(agtype_str);
                            } else {
                                offset += snprintf(json_result + offset, buffer_size - offset, "null");
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

                        /* Check if value is already JSON (starts with [ or {) */
                        if (val[0] == '[' || val[0] == '{') {
                            size_t slen = strlen(val);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, val, slen); offset += slen; }
                        } else if (col_type == SQLITE_INTEGER || col_type == SQLITE_FLOAT) {
                            /* Numeric value - output without quotes */
                            size_t slen = strlen(val);
                            if (offset + slen < buffer_size) { memcpy(json_result + offset, val, slen); offset += slen; }
                        } else if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) {
                            /* Boolean literal - output without quotes (GQLITE-T-0227) */
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

/* Extract nanosecond portion from ISO datetime string.
 * Input forms: 'YYYY-MM-DDTHH:MM:SS.<digits><tz>?', returns digits zero-padded
 * to 9 places as integer. Returns 0 if no fractional second present. */
static void gql_extract_ns_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (argc != 1) { sqlite3_result_int64(context, 0); return; }
    const char *s = (const char*)sqlite3_value_text(argv[0]);
    if (!s) { sqlite3_result_int64(context, 0); return; }
    const char *dot = strchr(s, '.');
    if (!dot) { sqlite3_result_int64(context, 0); return; }
    const char *p = dot + 1;
    char buf[10]; int n = 0;
    while (*p && n < 9 && *p >= '0' && *p <= '9') buf[n++] = *p++;
    while (n < 9) buf[n++] = '0';
    buf[9] = '\0';
    sqlite3_result_int64(context, atoll(buf));
}

/* Extract timezone suffix from ISO datetime string.
 * Returns the substring from the first +/-/Z after the time portion, or ''. */
static void gql_extract_tz_func(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
    if (argc != 1) { sqlite3_result_text(context, "", 0, SQLITE_TRANSIENT); return; }
    const char *s = (const char*)sqlite3_value_text(argv[0]);
    if (!s) { sqlite3_result_text(context, "", 0, SQLITE_TRANSIENT); return; }
    /* For datetime strings ('YYYY-MM-DDT...'), scan after the 'T'.
     * For time-only strings ('HH:MM:SS...'), scan from the start.
     * Pure date strings ('YYYY-MM-DD') have no tz; return ''.
     */
    const char *t = strchr(s, 'T');
    const char *start;
    if (t) {
        start = t + 1;
    } else if (strlen(s) >= 8 && s[2] == ':') {
        start = s;
    } else {
        sqlite3_result_text(context, "", 0, SQLITE_TRANSIENT); return;
    }
    const char *p = start;
    while (*p && !(*p == '+' || (*p == '-' && p > start) || *p == 'Z' || *p == '[')) p++;
    sqlite3_result_text(context, p, -1, SQLITE_TRANSIENT);
}

/* Parse an ISO temporal string into UTC epoch nanoseconds (int64).
 * Supports:
 *   YYYY-MM-DDTHH:MM:SS[.frac][tz]
 *   YYYY-MM-DD
 *   HH:MM:SS[.frac][tz]            (treated as 1970-01-01 with the time)
 * Where [tz] is one of:  Z | (+|-)HH:MM | (+|-)HH:MM[Region/Name]
 * Returns 0 on parse failure.
 */
static int64_t parse_temporal_ns(const char *s) {
    if (!s) return 0;
    int y = 1970, mo = 1, d = 1, h = 0, mi = 0, sec = 0;
    int64_t ns = 0;
    int tz_offset_min = 0;

    const char *time_start = NULL;
    if (strlen(s) >= 10 && s[4] == '-') {
        if (sscanf(s, "%d-%d-%d", &y, &mo, &d) < 3) return 0;
        if (s[10] == 'T' || s[10] == ' ') time_start = s + 11;
    } else if (strlen(s) >= 5 && s[2] == ':') {
        time_start = s;
    } else {
        return 0;
    }

    if (time_start) {
        if (sscanf(time_start, "%d:%d:%d", &h, &mi, &sec) < 2) { /* allow H:M only */ }
        const char *dot = strchr(time_start, '.');
        const char *tz_search_from;
        if (dot) {
            char buf[10] = { '0','0','0','0','0','0','0','0','0', 0 };
            int i;
            for (i = 0; i < 9 && dot[i + 1] >= '0' && dot[i + 1] <= '9'; i++)
                buf[i] = dot[i + 1];
            ns = atoll(buf);
            tz_search_from = dot + 1 + i;
        } else {
            tz_search_from = time_start;
        }
        const char *tz = NULL;
        for (const char *p = tz_search_from; *p; p++) {
            if (*p == 'Z' || *p == '+' || (*p == '-' && p > time_start + 2)) {
                tz = p; break;
            }
        }
        if (tz) {
            if (*tz == 'Z') {
                tz_offset_min = 0;
            } else {
                int sign = (*tz == '+') ? 1 : -1;
                int oh = 0, om = 0;
                if (sscanf(tz + 1, "%d:%d", &oh, &om) >= 1) {
                    tz_offset_min = sign * (oh * 60 + om);
                }
            }
        }
    }

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
    t.tm_hour = h; t.tm_min = mi; t.tm_sec = sec;
    time_t epoch = timegm(&t);
    epoch -= tz_offset_min * 60;
    return (int64_t)epoch * 1000000000LL + ns;
}

/* Build openCypher Duration JSON object from a signed total-nanoseconds value.
 * Returns: {"_iso8601": "...", "months": 0, "days": D, "seconds": S, "nanosecondsOfSecond": N}
 *
 * Decomposition rules (matches openCypher TCK):
 *   days        = total_ns / 86_400_000_000_000          (truncate toward zero)
 *   residue_ns  = total_ns - days * 86_400_000_000_000
 *   seconds     = floorDiv(residue_ns, 1_000_000_000)
 *   nanos       = residue_ns - seconds * 1_000_000_000   (always in [0, 1e9))
 *
 * ISO format truncates each (H, M, S) toward zero so signs are per-component.
 */
static void gql_duration_from_total_ns_func(
    sqlite3_context *ctx, int argc, sqlite3_value **argv
) {
    if (argc != 1) { sqlite3_result_null(ctx); return; }
    int64_t total_ns = sqlite3_value_int64(argv[0]);

    int64_t DAY_NS = 86400LL * 1000000000LL;
    int64_t days = total_ns / DAY_NS;
    int64_t residue = total_ns - days * DAY_NS;
    int64_t seconds, nanos;
    if (residue >= 0) {
        seconds = residue / 1000000000LL;
        nanos = residue - seconds * 1000000000LL;
    } else {
        /* floor division: round toward -infinity */
        seconds = -(((-residue) + 999999999LL) / 1000000000LL);
        nanos = residue - seconds * 1000000000LL;
    }

    /* ISO 8601 PT-form. Build hours/minutes/seconds via trunc-toward-zero. */
    int64_t rem = total_ns;
    int64_t hours = rem / (3600LL * 1000000000LL);
    rem -= hours * 3600LL * 1000000000LL;
    int64_t mins = rem / (60LL * 1000000000LL);
    rem -= mins * 60LL * 1000000000LL;
    int64_t isec = rem / 1000000000LL;
    int64_t subns = rem - isec * 1000000000LL;

    char iso[160];
    char *p = iso;
    *p++ = 'P'; *p++ = 'T';
    bool wrote = false;
    if (hours != 0) { p += snprintf(p, 24, "%lldH", (long long)hours); wrote = true; }
    if (mins != 0)  { p += snprintf(p, 24, "%lldM", (long long)mins); wrote = true; }
    if (isec != 0 || subns != 0) {
        if (subns != 0) {
            char frac[12];
            int64_t abs_subns = subns < 0 ? -subns : subns;
            snprintf(frac, sizeof(frac), "%09lld", (long long)abs_subns);
            int flen = 9;
            while (flen > 0 && frac[flen - 1] == '0') flen--;
            frac[flen] = 0;
            bool neg = (isec < 0) || (isec == 0 && subns < 0);
            int64_t abs_isec = isec < 0 ? -isec : isec;
            p += snprintf(p, 40, "%s%lld.%sS", neg ? "-" : "", (long long)abs_isec, frac);
        } else {
            p += snprintf(p, 24, "%lldS", (long long)isec);
        }
        wrote = true;
    }
    if (!wrote) { *p++ = '0'; *p++ = 'S'; }
    *p = 0;

    char json[400];
    snprintf(json, sizeof(json),
        "{\"_iso8601\":\"%s\",\"months\":0,\"days\":%lld,\"seconds\":%lld,\"nanosecondsOfSecond\":%lld}",
        iso, (long long)days, (long long)seconds, (long long)nanos);
    sqlite3_result_text(ctx, json, -1, SQLITE_TRANSIENT);
}

/* Diff (b - a) in nanoseconds between two ISO temporal strings. */
static void gql_temporal_diff_ns_func(
    sqlite3_context *ctx, int argc, sqlite3_value **argv
) {
    if (argc != 2) { sqlite3_result_int64(ctx, 0); return; }
    const char *a = (const char*)sqlite3_value_text(argv[0]);
    const char *b = (const char*)sqlite3_value_text(argv[1]);
    int64_t ta = parse_temporal_ns(a);
    int64_t tb = parse_temporal_ns(b);
    sqlite3_result_int64(ctx, tb - ta);
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
        graphqlite_result_error(context, "regexp() requires 2 arguments", GQL_ERR_VALIDATION);
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
        /* Sanitize double quotes in dynamic error message */
        for (char *p = errbuf; *p; p++) { if (*p == '"') *p = '\''; }
        graphqlite_result_error(context, errbuf, GQL_ERR_EXECUTION);
        return;
    }

    /* Execute the regex */
    ret = regexec(&regex, string, 0, NULL, 0);
    regfree(&regex);

    /* Return result: 1 for match, 0 for no match */
    sqlite3_result_int(context, ret == 0 ? 1 : 0);
}

/* cypher_validate() - Parse and validate a Cypher query without executing it.
 * Returns a JSON object with validation results:
 *   {"valid": true} or {"valid": false, "error": "...", "line": N, "column": N}
 */
static void cypher_validate_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc < 1) {
        graphqlite_result_error(context, "cypher_validate requires a query argument", GQL_ERR_VALIDATION);
        return;
    }

    const char *query = (const char*)sqlite3_value_text(argv[0]);
    if (!query) {
        sqlite3_result_text(context, "{\"valid\": false, \"error\": \"Query is NULL\"}", -1, SQLITE_STATIC);
        return;
    }

    /* Parse the query */
    cypher_parse_result *parse_result = parse_cypher_query_ext(query);
    if (!parse_result) {
        sqlite3_result_text(context, "{\"valid\": false, \"error\": \"Parser allocation failed\"}", -1, SQLITE_STATIC);
        return;
    }

    if (parse_result->ast != NULL && parse_result->error_message == NULL) {
        /* Valid query */
        sqlite3_result_text(context, "{\"valid\": true}", -1, SQLITE_STATIC);
    } else {
        /* Invalid query - build JSON response with error details */
        char *response = malloc(1024);
        if (response) {
            const char *err = parse_result->error_message ? parse_result->error_message : "Unknown parse error";
            /* Escape quotes in error message for JSON */
            char escaped_err[512];
            char *dst = escaped_err;
            const char *src = err;
            while (*src && (dst - escaped_err) < 500) {
                if (*src == '"') { *dst++ = '\\'; *dst++ = '"'; }
                else if (*src == '\\') { *dst++ = '\\'; *dst++ = '\\'; }
                else { *dst++ = *src; }
                src++;
            }
            *dst = '\0';

            snprintf(response, 1024,
                "{\"valid\": false, \"error\": \"%s\", \"line\": %d, \"column\": %d}",
                escaped_err,
                parse_result->error_line > 0 ? parse_result->error_line : 1,
                parse_result->error_column > 0 ? parse_result->error_column : 0);
            sqlite3_result_text(context, response, -1, SQLITE_TRANSIENT);
            free(response);
        } else {
            sqlite3_result_text(context, "{\"valid\": false, \"error\": \"Memory allocation failed\"}", -1, SQLITE_STATIC);
        }
    }

    cypher_parse_result_free(parse_result);
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

  rc = sqlite3_create_function(db, "regexp", 2, SQLITE_UTF8, 0,
                         regexp_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "_gql_extract_ns", 1, SQLITE_UTF8, 0,
                         gql_extract_ns_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "_gql_extract_tz", 1, SQLITE_UTF8, 0,
                         gql_extract_tz_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "_gql_duration_from_total_ns", 1, SQLITE_UTF8, 0,
                         gql_duration_from_total_ns_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "_gql_temporal_diff_ns", 2, SQLITE_UTF8, 0,
                         gql_temporal_diff_ns_func, 0, 0);
  if (rc != SQLITE_OK) { free(cache); return rc; }

  rc = sqlite3_create_function(db, "cypher_validate", 1, SQLITE_UTF8, 0,
                         cypher_validate_func, 0, 0);
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