/*
 * MATCH Clause Execution
 * Handles MATCH clause and MATCH+... query combinations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"
#include "transform/transform_variables.h"

/* Functions will be migrated here one by one */

/* Execute MATCH clause by running generated SQL */
int execute_match_clause(cypher_executor *executor, cypher_match *match, cypher_result *result)
{
    if (!executor || !match || !result) {
        return -1;
    }
    
    /* Transform MATCH to SQL */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }
    
    if (transform_match_clause(ctx, match) < 0) {
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }
    
    /* For now, just return success - full SQL execution would be implemented here */
    CYPHER_DEBUG("Generated SQL for MATCH: %s", ctx->sql_buffer);
    
    /* Simulate finding some results */
    result->row_count = 1;
    result->column_count = 1;
    result->column_names = malloc(sizeof(char*));
    result->column_names[0] = strdup("n");
    
    result->data = malloc(sizeof(char**));
    result->data[0] = malloc(sizeof(char*));
    result->data[0][0] = strdup("Node(1)");
    
    cypher_transform_free_context(ctx);
    return 0;
}

/* Execute MATCH+RETURN query combination */
int execute_match_return_query(cypher_executor *executor, cypher_match *match, cypher_return *return_clause, cypher_result *result)
{
    if (!executor || !match || !return_clause || !result) {
        return -1;
    }

#ifdef GRAPHQLITE_PERF_TIMING
    struct timespec t_start, t_transform, t_prepare, t_execute;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif

    CYPHER_DEBUG("Executing MATCH+RETURN query");

    /* Build SQL query from MATCH and RETURN clauses */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }

    /* Transform MATCH clause to generate FROM/WHERE */
    if (transform_match_clause(ctx, match) < 0) {
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Finalize SQL generation before RETURN (assembles FROM + JOINs + WHERE) */
    /* Always call - function checks internally if there's anything to finalize */
    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "Failed to finalize SQL generation");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Transform RETURN clause to generate SELECT projections */
    if (transform_return_clause(ctx, return_clause) < 0) {
        set_result_error(result, "Failed to transform RETURN clause");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Prepend any CTE (Common Table Expression) for variable-length relationships */
    prepend_cte_to_sql(ctx);

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_transform);
#endif

    CYPHER_DEBUG("Generated SQL: %s", ctx->sql_buffer);

    /* Execute the SQL query */
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error[512];
        snprintf(error, sizeof(error), "SQL prepare failed: %s", sqlite3_errmsg(executor->db));
        set_result_error(result, error);
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Bind parameters if provided */
    if (executor->params_json) {
        if (bind_params_from_json(stmt, executor->params_json) < 0) {
            set_result_error(result, "Failed to bind query parameters");
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }
    }

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_prepare);
#endif

    /* Build result from SQL execution */
    if (build_query_results(executor, stmt, return_clause, result, ctx) < 0) {
        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);
        return -1;
    }

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_execute);
    double transform_ms = (t_transform.tv_sec - t_start.tv_sec) * 1000.0 + (t_transform.tv_nsec - t_start.tv_nsec) / 1000000.0;
    double prepare_ms = (t_prepare.tv_sec - t_transform.tv_sec) * 1000.0 + (t_prepare.tv_nsec - t_transform.tv_nsec) / 1000000.0;
    double execute_ms = (t_execute.tv_sec - t_prepare.tv_sec) * 1000.0 + (t_execute.tv_nsec - t_prepare.tv_nsec) / 1000000.0;
    CYPHER_DEBUG("MATCH+RETURN TIMING: transform=%.2fms, prepare=%.2fms, build_results=%.2fms", transform_ms, prepare_ms, execute_ms);
#endif

    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);
    return 0;
}

/* Build query results from executed SQL statement */
int build_query_results(cypher_executor *executor, sqlite3_stmt *stmt, cypher_return *return_clause, cypher_result *result, cypher_transform_context *ctx)
{
#ifdef GRAPHQLITE_PERF_TIMING
    struct timespec t_start, t_count, t_read;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif

    if (!stmt || !return_clause || !result) {
        return -1;
    }

    /* Get column count from return clause */
    int column_count = return_clause->items->count;

    /* Allocate column names array */
    result->column_names = malloc(column_count * sizeof(char*));
    if (!result->column_names) {
        set_result_error(result, "Memory allocation failed for column names");
        return -1;
    }

    /* Determine if we're returning vertices/edges/properties by analyzing return items */
    bool has_agtype_values = false;
    for (int i = 0; i < column_count; i++) {
        cypher_return_item *item = (cypher_return_item*)return_clause->items->items[i];
        if (item->expr && (item->expr->type == AST_NODE_IDENTIFIER || item->expr->type == AST_NODE_PROPERTY)) {
            /* This looks like a node/relationship variable (e.g., RETURN n, r) or property access (e.g., RETURN n.name) */
            has_agtype_values = true;
        }
    }

    /* Set column names from return items */
    for (int i = 0; i < column_count; i++) {
        cypher_return_item *item = (cypher_return_item*)return_clause->items->items[i];
        if (item->alias) {
            /* Use explicit alias if provided */
            result->column_names[i] = strdup(item->alias);
        } else if (item->expr && item->expr->type == AST_NODE_PROPERTY) {
            /* Build full property path from property access (n.age -> "n.age") */
            cypher_property *prop = (cypher_property*)item->expr;
            if (prop->expr && prop->expr->type == AST_NODE_IDENTIFIER) {
                cypher_identifier *id = (cypher_identifier*)prop->expr;
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s.%s", id->name, prop->property_name);
                result->column_names[i] = strdup(full_name);
            } else {
                result->column_names[i] = strdup(prop->property_name);
            }
        } else if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
            /* Use identifier name as column name (n -> "n") */
            cypher_identifier *ident = (cypher_identifier*)item->expr;
            result->column_names[i] = strdup(ident->name);
        } else if (item->expr && item->expr->type == AST_NODE_FUNCTION_CALL) {
            /* Generate function name as column name: funcname(arg1, arg2, ...) */
            cypher_function_call *func = (cypher_function_call*)item->expr;
            if (func->function_name) {
                char func_name[256];
                size_t pos = 0;
                pos += snprintf(func_name + pos, sizeof(func_name) - pos, "%s(", func->function_name);
                if (func->args) {
                    for (int j = 0; j < func->args->count && pos < sizeof(func_name) - 10; j++) {
                        if (j > 0) {
                            pos += snprintf(func_name + pos, sizeof(func_name) - pos, ", ");
                        }
                        ast_node *arg = func->args->items[j];
                        if (arg && arg->type == AST_NODE_IDENTIFIER) {
                            cypher_identifier *arg_id = (cypher_identifier*)arg;
                            pos += snprintf(func_name + pos, sizeof(func_name) - pos, "%s", arg_id->name);
                        } else if (arg && arg->type == AST_NODE_PROPERTY) {
                            cypher_property *arg_prop = (cypher_property*)arg;
                            if (arg_prop->expr && arg_prop->expr->type == AST_NODE_IDENTIFIER) {
                                cypher_identifier *prop_id = (cypher_identifier*)arg_prop->expr;
                                pos += snprintf(func_name + pos, sizeof(func_name) - pos, "%s.%s", prop_id->name, arg_prop->property_name);
                            }
                        } else {
                            pos += snprintf(func_name + pos, sizeof(func_name) - pos, "...");
                        }
                    }
                }
                snprintf(func_name + pos, sizeof(func_name) - pos, ")");
                result->column_names[i] = strdup(func_name);
            } else {
                char default_name[32];
                snprintf(default_name, sizeof(default_name), "column_%d", i);
                result->column_names[i] = strdup(default_name);
            }
        } else {
            /* Generate default column name for complex expressions */
            char default_name[32];
            snprintf(default_name, sizeof(default_name), "column_%d", i);
            result->column_names[i] = strdup(default_name);
        }
    }
    result->column_count = column_count;

    /* Count rows first */
#ifdef GRAPHQLITE_PERF_TIMING
    struct timespec t_first_step;
#endif
    int row_count = 0;
    int first_step_rc = sqlite3_step(stmt);
#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_first_step);
#endif
    if (first_step_rc == SQLITE_ROW) {
        row_count++;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            row_count++;
        }
    }

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_count);
    double first_step_ms = (t_first_step.tv_sec - t_start.tv_sec) * 1000.0 + (t_first_step.tv_nsec - t_start.tv_nsec) / 1000000.0;
    CYPHER_DEBUG("SQL FIRST_STEP TIMING: %.2fms", first_step_ms);
#endif

    if (row_count == 0) {
#ifdef GRAPHQLITE_PERF_TIMING
        double count_ms = (t_count.tv_sec - t_start.tv_sec) * 1000.0 + (t_count.tv_nsec - t_start.tv_nsec) / 1000000.0;
        CYPHER_DEBUG("BUILD_RESULTS TIMING: count_rows=%.2fms (0 rows), read_data=0ms", count_ms);
#endif
        result->row_count = 0;
        result->data = NULL;
        result->success = true;
        return 0;
    }
    
    /* Reset statement for actual data reading */
    sqlite3_reset(stmt);
    
    /* Allocate data arrays */
    result->data = malloc(row_count * sizeof(char**));
    if (!result->data) {
        set_result_error(result, "Memory allocation failed for result data");
        return -1;
    }

    /* Allocate data_types array for type preservation */
    result->data_types = malloc(row_count * sizeof(int*));
    if (!result->data_types) {
        set_result_error(result, "Memory allocation failed for data types");
        return -1;
    }

    /* Allocate agtype data if we have graph entities or property access */
    if (has_agtype_values) {
        result->agtype_data = malloc(row_count * sizeof(agtype_value**));
        if (!result->agtype_data) {
            set_result_error(result, "Memory allocation failed for agtype data");
            return -1;
        }
        result->use_agtype = true;
    }
    
    /* Read actual data */
    int current_row = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && current_row < row_count) {
        result->data[current_row] = malloc(column_count * sizeof(char*));
        if (!result->data[current_row]) {
            set_result_error(result, "Memory allocation failed for row data");
            return -1;
        }
        
        if (has_agtype_values) {
            result->agtype_data[current_row] = malloc(column_count * sizeof(agtype_value*));
            if (!result->agtype_data[current_row]) {
                set_result_error(result, "Memory allocation failed for agtype row data");
                return -1;
            }
        }

        /* Allocate and populate data_types for this row */
        result->data_types[current_row] = malloc(column_count * sizeof(int));
        if (!result->data_types[current_row]) {
            set_result_error(result, "Memory allocation failed for row data types");
            return -1;
        }

        for (int col = 0; col < column_count; col++) {
            /* Store SQLite column type for proper JSON formatting */
            result->data_types[current_row][col] = sqlite3_column_type(stmt, col);
            const char *value = (const char*)sqlite3_column_text(stmt, col);
            if (value) {
                result->data[current_row][col] = strdup(value);
                
                /* Create agtype value for graph entities */
                if (has_agtype_values) {
                    cypher_return_item *item = (cypher_return_item*)return_clause->items->items[col];
                    if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
                        cypher_identifier *ident = (cypher_identifier*)item->expr;
                        
                        /* Check if this is a path variable */
                        if (ctx && transform_var_is_path(ctx->var_ctx, ident->name)) {
                            CYPHER_DEBUG("Executor: Processing path variable '%s' with value: %s", ident->name, value);
                            /* Parse the JSON array of element IDs and build path object */
                            result->agtype_data[current_row][col] = build_path_from_ids(executor, ctx, ident->name, value);
                        } else if (ctx && transform_var_is_edge(ctx->var_ctx, ident->name)) {
                            /* Check if value is already a JSON object (from new RETURN format) */
                            if (value[0] == '{') {
                                /* Parse the JSON object directly */
                                result->agtype_data[current_row][col] = agtype_value_from_edge_json(executor->db, value);
                            } else {
                                /* Legacy path: value is just an edge ID */
                                int64_t edge_id = atoll(value);

                                /* Query the schema to get edge details */
                                char *type = NULL;
                                int64_t source_id = 0, target_id = 0;

                                if (executor && executor->schema_mgr) {
                                    /* Get edge details from edges table */
                                    sqlite3_stmt *edge_stmt;
                                    const char *edge_sql = "SELECT source_id, target_id, type FROM edges WHERE id = ?";
                                    if (sqlite3_prepare_v2(executor->db, edge_sql, -1, &edge_stmt, NULL) == SQLITE_OK) {
                                        sqlite3_bind_int64(edge_stmt, 1, edge_id);
                                        if (sqlite3_step(edge_stmt) == SQLITE_ROW) {
                                            source_id = sqlite3_column_int64(edge_stmt, 0);
                                            target_id = sqlite3_column_int64(edge_stmt, 1);
                                            const char *type_text = (const char*)sqlite3_column_text(edge_stmt, 2);
                                            if (type_text) {
                                                type = strdup(type_text);
                                            }
                                        }
                                        sqlite3_finalize(edge_stmt);
                                    }
                                }

                                /* Create edge agtype value with properties */
                                result->agtype_data[current_row][col] = agtype_value_create_edge_with_properties(executor->db, edge_id, type, source_id, target_id);
                                free(type);
                            }
                        } else if (ctx && transform_var_lookup_node(ctx->var_ctx, ident->name)) {
                            /* This is a node variable */
                            /* Check if value is already a JSON object (from new RETURN format) */
                            if (value[0] == '{') {
                                /* Parse the JSON object directly */
                                result->agtype_data[current_row][col] = agtype_value_from_vertex_json(executor->db, value);
                            } else {
                                /* Legacy path: value is just a node ID */
                                int64_t node_id = atoll(value);

                                /* Query the schema to get node details */
                                char *label = NULL;
                                if (executor && executor->schema_mgr) {
                                    /* Get node label from node_labels table */
                                    sqlite3_stmt *label_stmt;
                                    const char *label_sql = "SELECT label FROM node_labels WHERE node_id = ? LIMIT 1";
                                    if (sqlite3_prepare_v2(executor->db, label_sql, -1, &label_stmt, NULL) == SQLITE_OK) {
                                        sqlite3_bind_int64(label_stmt, 1, node_id);
                                        if (sqlite3_step(label_stmt) == SQLITE_ROW) {
                                            const char *label_text = (const char*)sqlite3_column_text(label_stmt, 0);
                                            if (label_text) {
                                                label = strdup(label_text);
                                            }
                                        }
                                        sqlite3_finalize(label_stmt);
                                    }
                                }

                                /* Create vertex agtype value with properties */
                                result->agtype_data[current_row][col] = agtype_value_create_vertex_with_properties(executor->db, node_id, label);
                                free(label);
                            }
                        } else {
                            /* Not a graph entity - treat as scalar value */
                            result->agtype_data[current_row][col] = create_property_agtype_value(value);
                        }
                    } else if (item->expr && item->expr->type == AST_NODE_PROPERTY) {
                        /* Property access - try to detect the original data type */
                        result->agtype_data[current_row][col] = create_property_agtype_value(value);
                    } else {
                        /* For other non-entity columns, create string agtype values */
                        result->agtype_data[current_row][col] = agtype_value_create_string(value);
                    }
                }
            } else {
                /* Store NULL pointer - extension.c will format as JSON null */
                result->data[current_row][col] = NULL;
                if (has_agtype_values) {
                    result->agtype_data[current_row][col] = agtype_value_create_null();
                }
            }
        }
        current_row++;
    }
    
    result->row_count = row_count;
    result->success = true;

#ifdef GRAPHQLITE_PERF_TIMING
    clock_gettime(CLOCK_MONOTONIC, &t_read);
    double count_ms = (t_count.tv_sec - t_start.tv_sec) * 1000.0 + (t_count.tv_nsec - t_start.tv_nsec) / 1000000.0;
    double read_ms = (t_read.tv_sec - t_count.tv_sec) * 1000.0 + (t_read.tv_nsec - t_count.tv_nsec) / 1000000.0;
    CYPHER_DEBUG("BUILD_RESULTS TIMING: count_rows=%.2fms (%d rows), read_data=%.2fms (agtype: %s)",
                count_ms, row_count, read_ms, has_agtype_values ? "yes" : "no");
#endif

    return 0;
}

/* Create agtype value for property access by detecting data type from string value */
agtype_value* create_property_agtype_value(const char* value)
{
    if (!value) {
        return agtype_value_create_null();
    }
    
    /* Try to detect the data type from the string value */
    
    /* Check for boolean values */
    if (strcmp(value, "true") == 0) {
        return agtype_value_create_bool(true);
    }
    if (strcmp(value, "false") == 0) {
        return agtype_value_create_bool(false);
    }
    
    /* Check for integer values */
    char *endptr;
    errno = 0;
    long long_val = strtoll(value, &endptr, 10);
    if (errno == 0 && *endptr == '\0' && endptr != value) {
        /* Successfully parsed as integer */
        return agtype_value_create_integer((int64_t)long_val);
    }
    
    /* Check for float values */
    errno = 0;
    double double_val = strtod(value, &endptr);
    if (errno == 0 && *endptr == '\0' && endptr != value) {
        /* Successfully parsed as float */
        return agtype_value_create_float(double_val);
    }
    
    /* Default to string */
    return agtype_value_create_string(value);
}

/* Build a path agtype value from JSON array of element IDs */
agtype_value* build_path_from_ids(cypher_executor *executor, cypher_transform_context *ctx, const char *path_name, const char *json_ids)
{
    CYPHER_DEBUG("build_path_from_ids called: path_name='%s', json_ids='%s'", 
                 path_name ? path_name : "NULL", json_ids ? json_ids : "NULL");
    
    if (!executor || !ctx || !path_name || !json_ids) {
        CYPHER_DEBUG("build_path_from_ids: Missing required parameters");
        return agtype_value_create_null();
    }
    
    /* Get path variable metadata */
    transform_var *path_var = transform_var_lookup_path(ctx->var_ctx, path_name);
    if (!path_var || !path_var->path_elements) {
        CYPHER_DEBUG("build_path_from_ids: Failed to get path variable metadata for '%s'", path_name);
        return agtype_value_create_null();
    }
    CYPHER_DEBUG("build_path_from_ids: Found path metadata with %d elements", path_var->path_elements->count);
    
    /* Parse the JSON array of IDs (simple parsing for "[id1,id2,id3]" format) */
    if (json_ids[0] != '[') {
        CYPHER_DEBUG("build_path_from_ids: JSON doesn't start with '[': %s", json_ids);
        return agtype_value_create_null();
    }
    CYPHER_DEBUG("build_path_from_ids: Starting JSON parsing");
    
    /* Count elements in the JSON array */
    int id_count = 0;
    for (const char *p = json_ids + 1; *p && *p != ']'; p++) {
        if (*p == ',' || (*p >= '0' && *p <= '9')) {
            if (*p != ',' && (p == json_ids + 1 || *(p-1) == ',' || *(p-1) == '[')) {
                id_count++;
            }
        }
    }
    
    CYPHER_DEBUG("build_path_from_ids: Counted %d IDs in JSON", id_count);
    
    if (id_count != path_var->path_elements->count) {
        /* Mismatch between expected elements and actual IDs */
        CYPHER_DEBUG("build_path_from_ids: Mismatch - expected %d elements, got %d IDs", 
                     path_var->path_elements->count, id_count);
        return agtype_value_create_null();
    }
    
    /* Extract IDs and create agtype values for each element */
    agtype_value **path_elements = malloc(id_count * sizeof(agtype_value*));
    if (!path_elements) {
        return agtype_value_create_null();
    }
    
    /* Parse IDs from JSON array */
    const char *p = json_ids + 1; /* Skip opening bracket */
    int elem_index = 0;
    char id_buffer[32];
    int id_pos = 0;
    
    CYPHER_DEBUG("build_path_from_ids: Parsing JSON array: %s", json_ids);
    while (*p && *p != ']' && elem_index < id_count) {
        if (*p >= '0' && *p <= '9') {
            if (id_pos < sizeof(id_buffer) - 1) {
                id_buffer[id_pos++] = *p;
            }
        } else if (*p == ',') {
            if (id_pos > 0) {
                id_buffer[id_pos] = '\0';
                int64_t element_id = atoll(id_buffer);
                
                /* Create agtype value based on element type */
                ast_node *element = path_var->path_elements->items[elem_index];
                if (element->type == AST_NODE_NODE_PATTERN) {
                    /* Create vertex */
                    cypher_node_pattern *node = (cypher_node_pattern*)element;
                    const char *first_label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
                    CYPHER_DEBUG("build_path_from_ids: Creating vertex for element %d with ID %lld", elem_index, (long long)element_id);
                    path_elements[elem_index] = agtype_value_create_vertex_with_properties(executor->db, element_id, first_label);
                    CYPHER_DEBUG("build_path_from_ids: Created vertex %p", (void*)path_elements[elem_index]);
                } else if (element->type == AST_NODE_REL_PATTERN) {
                    /* Create edge - need to query for edge details */
                    CYPHER_DEBUG("build_path_from_ids: Creating edge for element %d with ID %lld", elem_index, (long long)element_id);
                    sqlite3_stmt *edge_stmt;
                    const char *edge_sql = "SELECT source_id, target_id, type FROM edges WHERE id = ?";
                    if (sqlite3_prepare_v2(executor->db, edge_sql, -1, &edge_stmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_int64(edge_stmt, 1, element_id);
                        if (sqlite3_step(edge_stmt) == SQLITE_ROW) {
                            int64_t source_id = sqlite3_column_int64(edge_stmt, 0);
                            int64_t target_id = sqlite3_column_int64(edge_stmt, 1);
                            const char *type = (const char*)sqlite3_column_text(edge_stmt, 2);
                            path_elements[elem_index] = agtype_value_create_edge_with_properties(executor->db, element_id, type, source_id, target_id);
                            CYPHER_DEBUG("build_path_from_ids: Created edge %p", (void*)path_elements[elem_index]);
                        } else {
                            CYPHER_DEBUG("build_path_from_ids: No edge found for ID %lld", (long long)element_id);
                            path_elements[elem_index] = agtype_value_create_null();
                        }
                        sqlite3_finalize(edge_stmt);
                    } else {
                        CYPHER_DEBUG("build_path_from_ids: Failed to prepare edge query");
                        path_elements[elem_index] = agtype_value_create_null();
                    }
                } else {
                    CYPHER_DEBUG("build_path_from_ids: Unknown element type at index %d", elem_index);
                    path_elements[elem_index] = agtype_value_create_null();
                }
                
                elem_index++;
                id_pos = 0;
                CYPHER_DEBUG("build_path_from_ids: Finished element %d, moving to next", elem_index - 1);
            }
        }
        p++;
    }
    
    /* Handle the last element if there's still data in the buffer */
    if (id_pos > 0 && elem_index < id_count) {
        id_buffer[id_pos] = '\0';
        int64_t element_id = atoll(id_buffer);
        
        CYPHER_DEBUG("build_path_from_ids: Processing final element %d with ID %lld", elem_index, (long long)element_id);
        
        /* Create agtype value based on element type */
        ast_node *element = path_var->path_elements->items[elem_index];
        if (element->type == AST_NODE_NODE_PATTERN) {
            /* Create vertex */
            cypher_node_pattern *node = (cypher_node_pattern*)element;
            const char *first_label = has_labels(node) ? get_label_string(node->labels->items[0]) : NULL;
            CYPHER_DEBUG("build_path_from_ids: Creating vertex for element %d with ID %lld", elem_index, (long long)element_id);
            path_elements[elem_index] = agtype_value_create_vertex_with_properties(executor->db, element_id, first_label);
            CYPHER_DEBUG("build_path_from_ids: Created vertex %p", (void*)path_elements[elem_index]);
        } else if (element->type == AST_NODE_REL_PATTERN) {
            /* Create edge - need to query for edge details */
            CYPHER_DEBUG("build_path_from_ids: Creating edge for element %d with ID %lld", elem_index, (long long)element_id);
            sqlite3_stmt *edge_stmt;
            const char *edge_sql = "SELECT source_id, target_id, type FROM edges WHERE id = ?";
            if (sqlite3_prepare_v2(executor->db, edge_sql, -1, &edge_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(edge_stmt, 1, element_id);
                if (sqlite3_step(edge_stmt) == SQLITE_ROW) {
                    int64_t source_id = sqlite3_column_int64(edge_stmt, 0);
                    int64_t target_id = sqlite3_column_int64(edge_stmt, 1);
                    const char *type = (const char*)sqlite3_column_text(edge_stmt, 2);
                    path_elements[elem_index] = agtype_value_create_edge_with_properties(executor->db, element_id, type, source_id, target_id);
                    CYPHER_DEBUG("build_path_from_ids: Created edge %p", (void*)path_elements[elem_index]);
                } else {
                    CYPHER_DEBUG("build_path_from_ids: No edge found for ID %lld", (long long)element_id);
                    path_elements[elem_index] = agtype_value_create_null();
                }
                sqlite3_finalize(edge_stmt);
            } else {
                CYPHER_DEBUG("build_path_from_ids: Failed to prepare edge query");
                path_elements[elem_index] = agtype_value_create_null();
            }
        } else {
            CYPHER_DEBUG("build_path_from_ids: Unknown element type at index %d", elem_index);
            path_elements[elem_index] = agtype_value_create_null();
        }
        elem_index++;
    }
    
    CYPHER_DEBUG("build_path_from_ids: Parsed all elements, elem_index = %d, expected = %d", elem_index, id_count);
    
    /* Check if we parsed all elements */
    if (elem_index != id_count) {
        CYPHER_DEBUG("build_path_from_ids: ERROR - only parsed %d elements but expected %d", elem_index, id_count);
        for (int i = 0; i < elem_index; i++) {
            /* Free already allocated elements */
            agtype_value_free(path_elements[i]);
        }
        free(path_elements);
        return agtype_value_create_null();
    }
    
    CYPHER_DEBUG("build_path_from_ids: About to create path agtype value with %d elements", id_count);
    
    /* Create path agtype value */
    agtype_value *path_value = agtype_build_path(path_elements, id_count);
    
    CYPHER_DEBUG("build_path_from_ids: Created path agtype value: %p", (void*)path_value);
    
    /* Cleanup */
    free(path_elements);
    
    CYPHER_DEBUG("build_path_from_ids: Returning path value");
    return path_value ? path_value : agtype_value_create_null();
}


/* Execute MATCH+CREATE query combination */
int execute_match_create_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_result *result)
{
    if (!executor || !match || !create || !result) {
        return -1;
    }
    
    CYPHER_DEBUG("Executing MATCH+CREATE query");
    
    /* First, execute the MATCH to bind variables to existing nodes */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }
    
    /* Transform MATCH clause to generate SQL */
    if (transform_match_clause(ctx, match) < 0) {
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Finalize SQL generation to assemble unified builder content into sql_buffer */
    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "Failed to finalize SQL generation");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Add a simple SELECT to get the matched node IDs */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
        /* Replace SELECT * with specific node ID selection */
        char *after_star = select_pos + strlen("SELECT *");
        char *temp = strdup(after_star);
        
        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';
        
        /* Select all node variables found in the MATCH */
        bool first = true;
        int var_count = transform_var_count(ctx->var_ctx);
        for (int i = 0; i < var_count; i++) {
            transform_var *var = transform_var_at(ctx->var_ctx, i);
            if (var && var->kind == VAR_KIND_NODE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", var->table_alias, var->name);
                first = false;
            }
        }
        
        append_sql(ctx, " %s", temp);
        free(temp);
    }
    
    CYPHER_DEBUG("Generated MATCH SQL: %s", ctx->sql_buffer);

    /* Execute the MATCH query to get existing node IDs */
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char error[512];
        snprintf(error, sizeof(error), "MATCH SQL prepare failed: %s", sqlite3_errmsg(executor->db));
        set_result_error(result, error);
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Bind parameters if provided */
    if (executor->params_json) {
        if (bind_params_from_json(stmt, executor->params_json) < 0) {
            set_result_error(result, "Failed to bind query parameters");
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }
    }

    /* Create variable map to store matched node IDs */
    variable_map *var_map = create_variable_map();
    if (!var_map) {
        set_result_error(result, "Failed to create variable map");
        sqlite3_finalize(stmt);
        cypher_transform_free_context(ctx);
        return -1;
    }
    
    /* Read matched node IDs */
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        int var_count2 = transform_var_count(ctx->var_ctx);
        for (int i = 0; i < var_count2; i++) {
            transform_var *var = transform_var_at(ctx->var_ctx, i);
            if (var && var->kind == VAR_KIND_NODE) {
                int64_t node_id = sqlite3_column_int64(stmt, col);
                set_variable_node_id(var_map, var->name, (int)node_id);
                CYPHER_DEBUG("Bound variable '%s' to existing node %lld", var->name, (long long)node_id);
                col++;
            }
        }
        break; /* For now, just take the first match */
    }
    
    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);
    
    /* Now execute the CREATE clause with the bound variables */
    if (!create->pattern) {
        set_result_error(result, "No pattern in CREATE clause");
        free_variable_map(var_map);
        return -1;
    }
    
    /* Process each path pattern in the CREATE clause */
    for (int i = 0; i < create->pattern->count; i++) {
        ast_node *pattern = create->pattern->items[i];
        
        if (pattern->type == AST_NODE_PATH) {
            if (execute_path_pattern_with_variables(executor, (cypher_path*)pattern, result, var_map) < 0) {
                free_variable_map(var_map);
                return -1;
            }
        }
    }
    
    free_variable_map(var_map);
    return 0;
}


/* Execute MATCH+CREATE+RETURN query combination */
int execute_match_create_return_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_return *return_clause, cypher_result *result)
{
    if (!executor || !match || !create || !return_clause || !result) {
        return -1;
    }
    
    CYPHER_DEBUG("Executing MATCH+CREATE+RETURN query");
    
    /* First execute MATCH+CREATE */
    if (execute_match_create_query(executor, match, create, result) < 0) {
        return -1;
    }
    
    /* Then execute the RETURN clause as a separate MATCH query */
    /* This is a simplified approach - in a full implementation, we'd track created objects */
    return execute_match_return_query(executor, match, return_clause, result);
}
