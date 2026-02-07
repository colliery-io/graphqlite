/*
 * Cypher Execution Engine
 * Orchestrates parser, transformer, and schema manager for end-to-end query execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sqlite3.h>

#include "executor/cypher_executor.h"
#include "executor/executor_internal.h"
#include "executor/query_patterns.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"

/* SQLite custom function: REVERSE(string) - reverses a string */
static void sqlite_reverse_func(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    if (argc != 1) {
        sqlite3_result_error(context, "reverse() requires exactly 1 argument", -1);
        return;
    }

    if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(context);
        return;
    }

    const unsigned char *input = sqlite3_value_text(argv[0]);
    if (!input) {
        sqlite3_result_null(context);
        return;
    }

    int len = sqlite3_value_bytes(argv[0]);
    char *result = sqlite3_malloc(len + 1);
    if (!result) {
        sqlite3_result_error_nomem(context);
        return;
    }

    /* Reverse the string */
    for (int i = 0; i < len; i++) {
        result[i] = input[len - 1 - i];
    }
    result[len] = '\0';

    sqlite3_result_text(context, result, len, sqlite3_free);
}

/* Register custom SQLite functions needed for Cypher execution */
static int register_custom_functions(sqlite3 *db)
{
    int rc = sqlite3_create_function(db, "REVERSE", 1, SQLITE_UTF8, NULL,
                                      sqlite_reverse_func, NULL, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }
    return 0;
}

/* Helper functions moved to executor_helpers.c:
 * - get_label_string
 * - has_labels
 * - bind_params_from_json
 */

/* Performance timing instrumentation - enable with -DGRAPHQLITE_PERF_TIMING */


/* Create execution engine */
cypher_executor* cypher_executor_create(sqlite3 *db)
{
    if (!db) {
        return NULL;
    }
    
    cypher_executor *executor = calloc(1, sizeof(cypher_executor));
    if (!executor) {
        return NULL;
    }
    
    executor->db = db;
    executor->schema_initialized = false;
    executor->params_json = NULL;

    /* Register custom SQLite functions */
    if (register_custom_functions(db) < 0) {
        free(executor);
        return NULL;
    }

    /* Create schema manager */
    executor->schema_mgr = cypher_schema_create_manager(db);
    if (!executor->schema_mgr) {
        free(executor);
        return NULL;
    }
    
    /* Initialize schema */
    if (cypher_schema_initialize(executor->schema_mgr) < 0) {
        cypher_schema_free_manager(executor->schema_mgr);
        free(executor);
        return NULL;
    }
    
    executor->schema_initialized = true;
    
    CYPHER_DEBUG("Created cypher executor with initialized schema");
    
    return executor;
}

void cypher_executor_free(cypher_executor *executor)
{
    if (!executor) {
        return;
    }
    
    cypher_schema_free_manager(executor->schema_mgr);
    free(executor);
    
    CYPHER_DEBUG("Freed cypher executor");
}

/* Forward declarations - all are non-static since declared in executor_internal.h */
/* Functions moved to extracted modules are declared there too */



/* Execute AST node */
cypher_result* cypher_executor_execute_ast(cypher_executor *executor, ast_node *ast)
{
    if (!executor || !ast) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor or AST");
        }
        return result;
    }
    
    if (!executor->schema_initialized) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Schema not initialized");
        }
        return result;
    }
    
    cypher_result *result = create_empty_result();
    if (!result) {
        return NULL;
    }
    
    CYPHER_DEBUG("Executing AST node type: %d", ast->type);
    
    /* Handle different query types */
    switch (ast->type) {
        case AST_NODE_QUERY:
        case AST_NODE_SINGLE_QUERY:
            /* Query node - cast the AST node to cypher_query and process its clauses */  
            {
                cypher_query *query = (cypher_query*)ast;
                CYPHER_DEBUG("Found query node with %d clauses", query->clauses ? query->clauses->count : 0);
                
                if (query->clauses) {
                    /* Handle EXPLAIN - return generated SQL and pattern info */
                    if (query->explain) {
                        CYPHER_DEBUG("EXPLAIN mode - returning generated SQL and pattern info");

                        /* Analyze query to find matched pattern */
                        clause_flags flags = analyze_query_clauses(query);
                        const query_pattern *pattern = find_matching_pattern(flags);
                        const char *pattern_name = pattern ? pattern->name : "NONE";
                        const char *flags_str = clause_flags_to_string(flags);

                        cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
                        if (!ctx) {
                            set_result_error(result, "Failed to create transform context");
                            return result;
                        }

                        /* Transform the query to SQL */
                        int transform_status = cypher_transform_generate_sql(ctx, query);
                        if (transform_status < 0 || ctx->has_error) {
                            set_result_error(result, ctx->error_message ? ctx->error_message : "Transform error");
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        /* Return pattern info + SQL as formatted output */
                        result->column_count = 1;
                        result->row_count = 1;
                        result->data = malloc(sizeof(char**));
                        result->data[0] = malloc(sizeof(char*));

                        /* Format: Pattern: NAME\nClauses: FLAGS\nSQL: query */
                        const char *sql = ctx->sql_buffer ? ctx->sql_buffer : "";
                        size_t len = strlen(pattern_name) + strlen(flags_str) + strlen(sql) + 64;
                        char *explain_output = malloc(len);
                        snprintf(explain_output, len, "Pattern: %s\nClauses: %s\nSQL: %s",
                                 pattern_name, flags_str, sql);
                        result->data[0][0] = explain_output;
                        result->success = true;

                        cypher_transform_free_context(ctx);
                        return result;
                    }

                    /* Table-driven pattern dispatch */
                    if (dispatch_query_pattern(executor, query, result) < 0) {
                        return result; /* Error already set */
                    }
                } else {
                    CYPHER_DEBUG("No clauses found in query");
                }
            }
            break;
            
        case AST_NODE_CREATE:
            if (execute_create_clause(executor, (cypher_create*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;

        case AST_NODE_MERGE:
            if (execute_merge_clause(executor, (cypher_merge*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;

        case AST_NODE_SET:
            if (execute_set_clause(executor, (cypher_set*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;
            
        case AST_NODE_MATCH:
            if (execute_match_clause(executor, (cypher_match*)ast, result) < 0) {
                return result; /* Error already set */
            }
            break;
            
        case AST_NODE_UNION:
            /* UNION query - transform and execute via the transform layer */
            {
                CYPHER_DEBUG("Executing UNION query");
                cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
                if (!ctx) {
                    set_result_error(result, "Failed to create transform context");
                    return result;
                }

                /* The transform layer handles UNION queries directly when passed the union node */
                cypher_query_result *transform_result = cypher_transform_query(ctx, (cypher_query*)ast);
                if (!transform_result) {
                    set_result_error(result, "Failed to transform UNION query");
                    cypher_transform_free_context(ctx);
                    return result;
                }

                if (transform_result->has_error) {
                    set_result_error(result, transform_result->error_message ? transform_result->error_message : "UNION transform error");
                    free(transform_result);
                    cypher_transform_free_context(ctx);
                    return result;
                }

                /* Execute the prepared statement */
                if (transform_result->stmt) {
                    /* Bind parameters if provided */
                    if (executor->params_json) {
                        if (bind_params_from_json(transform_result->stmt, executor->params_json) < 0) {
                            set_result_error(result, "Failed to bind query parameters");
                            free(transform_result);
                            cypher_transform_free_context(ctx);
                            return result;
                        }
                    }

                    result->data = NULL;
                    result->row_count = 0;
                    result->column_count = sqlite3_column_count(transform_result->stmt);

                    /* Get column names from the SQL result */
                    result->column_names = malloc(result->column_count * sizeof(char*));
                    if (result->column_names) {
                        for (int c = 0; c < result->column_count; c++) {
                            const char *name = sqlite3_column_name(transform_result->stmt, c);
                            result->column_names[c] = name ? strdup(name) : NULL;
                        }
                    }

                    /* Collect results with type information */
                    while (sqlite3_step(transform_result->stmt) == SQLITE_ROW) {
                        /* Allocate/resize data and data_types arrays */
                        result->data = realloc(result->data, (result->row_count + 1) * sizeof(char**));
                        result->data[result->row_count] = calloc(result->column_count, sizeof(char*));
                        result->data_types = realloc(result->data_types, (result->row_count + 1) * sizeof(int*));
                        result->data_types[result->row_count] = calloc(result->column_count, sizeof(int));

                        for (int c = 0; c < result->column_count; c++) {
                            /* Store the SQLite type */
                            result->data_types[result->row_count][c] = sqlite3_column_type(transform_result->stmt, c);
                            const char *val = (const char*)sqlite3_column_text(transform_result->stmt, c);
                            result->data[result->row_count][c] = val ? strdup(val) : NULL;
                        }
                        result->row_count++;
                    }
                    sqlite3_finalize(transform_result->stmt);
                }

                result->success = true;
                free(transform_result);
                cypher_transform_free_context(ctx);
            }
            break;

        default:
            set_result_error(result, "Unsupported query type");
            return result;
    }
    
    /* If we got here, execution was successful */
    result->success = true;
    
    return result;
}

/* Execute query string */
cypher_result* cypher_executor_execute(cypher_executor *executor, const char *query)
{
    if (!executor || !query) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor or query");
        }
        return result;
    }

    CYPHER_DEBUG("Executing query: %s", query);

#ifdef GRAPHQLITE_PERF_TIMING
    struct timespec t_start, t_parse, t_exec, t_cleanup;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif

    /* Parse query to AST with extended error handling */
    CYPHER_DEBUG("Parsing query: '%s'", query);
    cypher_parse_result *parse_result = parse_cypher_query_ext(query);
    if (!parse_result) {
        CYPHER_DEBUG("Parser returned NULL");
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Internal parser error");
        }
        return result;
    }

    /* Check for parse errors */
    if (!parse_result->ast) {
        CYPHER_DEBUG("Parser error: %s", parse_result->error_message ? parse_result->error_message : "Unknown error");
        cypher_result *result = create_empty_result();
        if (result) {
            /* Use the detailed parser error message */
            set_result_error(result, parse_result->error_message ? parse_result->error_message : "Failed to parse query");
        }
        cypher_parse_result_free(parse_result);
        return result;
    }

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_parse);
#endif

    ast_node *ast = parse_result->ast;

    CYPHER_DEBUG("Parser returned AST with type=%d, data=%p", ast->type, ast->data);

    /* Execute AST */
    cypher_result *result = cypher_executor_execute_ast(executor, ast);

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_exec);
#endif

    /* Clean up parse result (includes AST) - note: don't free AST separately */
    parse_result->ast = NULL;  /* Prevent double-free since execute_ast may have taken ownership */
    cypher_parse_result_free(parse_result);

    /* Clean up AST */
    cypher_parser_free_result(ast);

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_cleanup);
    double parse_ms = (t_parse.tv_sec - t_start.tv_sec) * 1000.0 + (t_parse.tv_nsec - t_start.tv_nsec) / 1000000.0;
    double exec_ms = (t_exec.tv_sec - t_parse.tv_sec) * 1000.0 + (t_exec.tv_nsec - t_parse.tv_nsec) / 1000000.0;
    double cleanup_ms = (t_cleanup.tv_sec - t_exec.tv_sec) * 1000.0 + (t_cleanup.tv_nsec - t_exec.tv_nsec) / 1000000.0;
    CYPHER_DEBUG("TIMING: parse=%.2fms, exec=%.2fms, cleanup=%.2fms", parse_ms, exec_ms, cleanup_ms);
#endif

    return result;
}

/* Execute Cypher query with parameters */
cypher_result* cypher_executor_execute_params(cypher_executor *executor, const char *query, const char *params_json)
{
    if (!executor) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor");
        }
        return result;
    }

    /* Set params for this execution */
    executor->params_json = params_json;

    /* Execute the query */
    cypher_result *result = cypher_executor_execute(executor, query);

    /* Clear params */
    executor->params_json = NULL;

    return result;
}

/* Execute AST with parameters */
cypher_result* cypher_executor_execute_ast_params(cypher_executor *executor, ast_node *ast, const char *params_json)
{
    if (!executor) {
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Invalid executor");
        }
        return result;
    }

    /* Set params for this execution */
    executor->params_json = params_json;

    /* Execute the AST */
    cypher_result *result = cypher_executor_execute_ast(executor, ast);

    /* Clear params */
    executor->params_json = NULL;

    return result;
}

/* Free result */
void cypher_result_free(cypher_result *result)
{
    if (!result) {
        return;
    }
    
    free(result->error_message);
    
    if (result->column_names) {
        for (int i = 0; i < result->column_count; i++) {
            free(result->column_names[i]);
        }
        free(result->column_names);
    }
    
    if (result->data) {
        for (int row = 0; row < result->row_count; row++) {
            if (result->data[row]) {
                for (int col = 0; col < result->column_count; col++) {
                    free(result->data[row][col]);
                }
                free(result->data[row]);
            }
        }
        free(result->data);
    }

    if (result->data_types) {
        for (int row = 0; row < result->row_count; row++) {
            free(result->data_types[row]);
        }
        free(result->data_types);
    }

    if (result->agtype_data) {
        for (int row = 0; row < result->row_count; row++) {
            if (result->agtype_data[row]) {
                for (int col = 0; col < result->column_count; col++) {
                    agtype_value_free(result->agtype_data[row][col]);
                }
                free(result->agtype_data[row]);
            }
        }
        free(result->agtype_data);
    }
    
    free(result);
}

/* Print result */
void cypher_result_print(cypher_result *result)
{
    if (!result) {
        printf("NULL result\n");
        return;
    }
    
    if (!result->success) {
        printf("Query failed: %s\n", result->error_message ? result->error_message : "Unknown error");
        return;
    }
    
    /* Print statistics for modification queries */
    if (result->nodes_created > 0 || result->nodes_deleted > 0 || result->relationships_created > 0 || result->relationships_deleted > 0 || result->properties_set > 0) {
        printf("Query executed successfully - nodes created: %d, relationships created: %d, nodes deleted: %d, relationships deleted: %d\n", 
               result->nodes_created, result->relationships_created, result->nodes_deleted, result->relationships_deleted);
    }
    
    /* Print result data */
    if (result->row_count > 0 && result->column_count > 0) {
        /* Print column headers */
        for (int col = 0; col < result->column_count; col++) {
            printf("%-15s", result->column_names[col]);
        }
        printf("\n");
        
        /* Print separator */
        for (int col = 0; col < result->column_count; col++) {
            printf("%-15s", "---------------");
        }
        printf("\n");
        
        /* Print data rows */
        for (int row = 0; row < result->row_count; row++) {
            for (int col = 0; col < result->column_count; col++) {
                printf("%-15s", result->data[row][col]);
            }
            printf("\n");
        }
    }
}

/* Utility functions */
bool cypher_executor_is_ready(cypher_executor *executor)
{
    return executor && executor->schema_initialized;
}

const char* cypher_executor_get_last_error(cypher_executor *executor)
{
    UNUSED_PARAMETER(executor);
    return "Not implemented";
}