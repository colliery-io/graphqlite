/*
 * MERGE Clause Execution
 * Handles MERGE clause and MATCH+MERGE query execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"
#include "transform/transform_variables.h"

/* Find a node by label and properties, returns node_id or -1 if not found */
int find_node_by_pattern(cypher_executor *executor, cypher_node_pattern *node_pattern)
{
    if (!executor || !node_pattern) {
        return -1;
    }

    /* Build SQL query to find matching node */
    char sql[2048];
    int offset = 0;

    offset += snprintf(sql + offset, sizeof(sql) - offset,
                       "SELECT n.id FROM nodes n");

    /* Add label joins if specified - one join per label */
    if (has_labels(node_pattern)) {
        for (int li = 0; li < node_pattern->labels->count; li++) {
            const char *label = get_label_string(node_pattern->labels->items[li]);
            if (label) {
                offset += snprintf(sql + offset, sizeof(sql) - offset,
                                  " JOIN node_labels nl%d ON n.id = nl%d.node_id AND nl%d.label = '%s'",
                                  li, li, li, label);
            }
        }
    }

    /* Add property joins if specified */
    if (node_pattern->properties && node_pattern->properties->type == AST_NODE_MAP) {
        cypher_map *map = (cypher_map*)node_pattern->properties;
        if (map->pairs) {
            for (int i = 0; i < map->pairs->count; i++) {
                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)pair->value;

                    /* Determine which property table to join */
                    const char *prop_table = "node_props_text";
                    char value_str[256];

                    switch (lit->literal_type) {
                        case LITERAL_STRING:
                            prop_table = "node_props_text";
                            snprintf(value_str, sizeof(value_str), "'%s'", lit->value.string);
                            break;
                        case LITERAL_INTEGER:
                            prop_table = "node_props_int";
                            snprintf(value_str, sizeof(value_str), "%d", lit->value.integer);
                            break;
                        case LITERAL_DECIMAL:
                            prop_table = "node_props_real";
                            snprintf(value_str, sizeof(value_str), "%f", lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            prop_table = "node_props_bool";
                            snprintf(value_str, sizeof(value_str), "%d", lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            continue;
                    }

                    offset += snprintf(sql + offset, sizeof(sql) - offset,
                                      " JOIN %s np%d ON n.id = np%d.node_id"
                                      " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = '%s'"
                                      " AND np%d.value = %s",
                                      prop_table, i, i,
                                      i, i, i, i, pair->key,
                                      i, value_str);
                }
            }
        }
    }

    offset += snprintf(sql + offset, sizeof(sql) - offset, " LIMIT 1");

    CYPHER_DEBUG("MERGE find query: %s", sql);

    /* Execute the query */
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("MERGE find query prepare failed: %s", sqlite3_errmsg(executor->db));
        return -1;
    }

    int node_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        node_id = sqlite3_column_int(stmt, 0);
        CYPHER_DEBUG("Found existing node %d", node_id);
    }

    sqlite3_finalize(stmt);
    return node_id;
}

/* Find an edge by source, target, type and optional properties. Returns edge_id or -1 if not found */
int find_edge_by_pattern(cypher_executor *executor, int source_id, int target_id,
                                 const char *type, cypher_rel_pattern *rel_pattern)
{
    if (!executor || source_id < 0 || target_id < 0) {
        return -1;
    }

    /* Build SQL query to find matching edge */
    char sql[2048];
    int offset = 0;

    offset += snprintf(sql + offset, sizeof(sql) - offset,
                       "SELECT e.id FROM edges e WHERE e.source_id = %d AND e.target_id = %d",
                       source_id, target_id);

    /* Add type filter if specified */
    if (type) {
        offset += snprintf(sql + offset, sizeof(sql) - offset, " AND e.type = '%s'", type);
    }

    /* Add property joins if specified */
    if (rel_pattern && rel_pattern->properties && rel_pattern->properties->type == AST_NODE_MAP) {
        cypher_map *map = (cypher_map*)rel_pattern->properties;
        if (map->pairs) {
            for (int i = 0; i < map->pairs->count; i++) {
                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[i];
                if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                    cypher_literal *lit = (cypher_literal*)pair->value;

                    /* Determine which property table to join */
                    const char *prop_table = "edge_props_text";
                    char value_str[256];

                    switch (lit->literal_type) {
                        case LITERAL_STRING:
                            prop_table = "edge_props_text";
                            snprintf(value_str, sizeof(value_str), "'%s'", lit->value.string);
                            break;
                        case LITERAL_INTEGER:
                            prop_table = "edge_props_int";
                            snprintf(value_str, sizeof(value_str), "%d", lit->value.integer);
                            break;
                        case LITERAL_DECIMAL:
                            prop_table = "edge_props_real";
                            snprintf(value_str, sizeof(value_str), "%f", lit->value.decimal);
                            break;
                        case LITERAL_BOOLEAN:
                            prop_table = "edge_props_bool";
                            snprintf(value_str, sizeof(value_str), "%d", lit->value.boolean ? 1 : 0);
                            break;
                        default:
                            continue;
                    }

                    offset += snprintf(sql + offset, sizeof(sql) - offset,
                                      " AND EXISTS (SELECT 1 FROM %s ep%d"
                                      " JOIN property_keys pk%d ON ep%d.key_id = pk%d.id"
                                      " WHERE ep%d.edge_id = e.id AND pk%d.key = '%s' AND ep%d.value = %s)",
                                      prop_table, i,
                                      i, i, i,
                                      i, i, pair->key, i, value_str);
                }
            }
        }
    }

    offset += snprintf(sql + offset, sizeof(sql) - offset, " LIMIT 1");

    CYPHER_DEBUG("MERGE find edge query: %s", sql);

    /* Execute the query */
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("MERGE find edge query prepare failed: %s", sqlite3_errmsg(executor->db));
        return -1;
    }

    int edge_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        edge_id = sqlite3_column_int(stmt, 0);
        CYPHER_DEBUG("Found existing edge %d", edge_id);
    }

    sqlite3_finalize(stmt);
    return edge_id;
}
/* Execute MERGE clause */
int execute_merge_clause(cypher_executor *executor, cypher_merge *merge, cypher_result *result)
{
    if (!executor || !merge || !result) {
        return -1;
    }

    if (!merge->pattern) {
        set_result_error(result, "No pattern in MERGE clause");
        return -1;
    }

    CYPHER_DEBUG("Executing MERGE clause with %d patterns", merge->pattern->count);

    /* Create variable map to track node variables */
    variable_map *var_map = create_variable_map();
    if (!var_map) {
        set_result_error(result, "Failed to create variable map");
        return -1;
    }

    /* Track which nodes were created vs matched */
    int nodes_matched = 0;
    int nodes_created_in_merge = 0;

    /* Process each path pattern in the MERGE clause */
    for (int p = 0; p < merge->pattern->count; p++) {
        ast_node *pattern = merge->pattern->items[p];

        if (pattern->type != AST_NODE_PATH) {
            CYPHER_DEBUG("Unexpected pattern type in MERGE: %d", pattern->type);
            continue;
        }

        cypher_path *path = (cypher_path*)pattern;
        if (!path->elements) continue;

        int previous_node_id = -1;

        /* Process path elements: node, rel, node, rel, node, ... */
        for (int i = 0; i < path->elements->count; i++) {
            ast_node *element = path->elements->items[i];

            if (element->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *node_pattern = (cypher_node_pattern*)element;
                int node_id = -1;
                bool was_created = false;

                /* Check if this variable already exists in var_map */
                if (node_pattern->variable) {
                    node_id = get_variable_node_id(var_map, node_pattern->variable);
                    if (node_id >= 0) {
                        CYPHER_DEBUG("Reusing existing node %d for variable '%s'", node_id, node_pattern->variable);
                        previous_node_id = node_id;
                        continue;
                    }
                }

                /* Try to find existing node by label + properties */
                node_id = find_node_by_pattern(executor, node_pattern);

                if (node_id >= 0) {
                    /* Node exists - matched */
                    nodes_matched++;
                    CYPHER_DEBUG("MERGE matched existing node %d", node_id);
                } else {
                    /* Node doesn't exist - create it */
                    node_id = cypher_schema_create_node(executor->schema_mgr);
                    if (node_id < 0) {
                        set_result_error(result, "Failed to create node in MERGE");
                        free_variable_map(var_map);
                        return -1;
                    }

                    was_created = true;
                    nodes_created_in_merge++;
                    result->nodes_created++;
                    CYPHER_DEBUG("MERGE created new node %d", node_id);

                    /* Add labels if specified - supports multiple labels */
                    if (has_labels(node_pattern)) {
                        for (int li = 0; li < node_pattern->labels->count; li++) {
                            const char *label = get_label_string(node_pattern->labels->items[li]);
                            if (label) {
                                cypher_schema_add_node_label(executor->schema_mgr, node_id, label);
                            }
                        }
                    }

                    /* Add properties if specified */
                    if (node_pattern->properties && node_pattern->properties->type == AST_NODE_MAP) {
                        cypher_map *map = (cypher_map*)node_pattern->properties;
                        if (map->pairs) {
                            for (int j = 0; j < map->pairs->count; j++) {
                                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                                if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                    cypher_literal *lit = (cypher_literal*)pair->value;
                                    property_type prop_type = PROP_TYPE_TEXT;
                                    const void *prop_value = NULL;

                                    switch (lit->literal_type) {
                                        case LITERAL_STRING:
                                            prop_type = PROP_TYPE_TEXT;
                                            prop_value = lit->value.string;
                                            break;
                                        case LITERAL_INTEGER:
                                            prop_type = PROP_TYPE_INTEGER;
                                            prop_value = &lit->value.integer;
                                            break;
                                        case LITERAL_DECIMAL:
                                            prop_type = PROP_TYPE_REAL;
                                            prop_value = &lit->value.decimal;
                                            break;
                                        case LITERAL_BOOLEAN:
                                            prop_type = PROP_TYPE_BOOLEAN;
                                            prop_value = &lit->value.boolean;
                                            break;
                                        default:
                                            continue;
                                    }

                                    if (prop_value) {
                                        cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, prop_type, prop_value);
                                        result->properties_set++;
                                    }
                                }
                            }
                        }
                    }
                }

                /* Store variable mapping if present */
                if (node_pattern->variable) {
                    set_variable_node_id(var_map, node_pattern->variable, node_id);
                }

                previous_node_id = node_id;

                /* Apply ON CREATE SET if node was created */
                if (was_created && merge->on_create && merge->on_create->count > 0) {
                    CYPHER_DEBUG("Applying ON CREATE SET for node %d", node_id);
                    if (execute_set_items(executor, merge->on_create, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

                /* Apply ON MATCH SET if node was matched */
                if (!was_created && merge->on_match && merge->on_match->count > 0) {
                    CYPHER_DEBUG("Applying ON MATCH SET for node %d", node_id);
                    if (execute_set_items(executor, merge->on_match, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

            } else if (element->type == AST_NODE_REL_PATTERN) {
                /* Handle relationship MERGE - need source and target nodes */
                if (previous_node_id < 0 || i + 1 >= path->elements->count) {
                    set_result_error(result, "Invalid relationship pattern in MERGE");
                    free_variable_map(var_map);
                    return -1;
                }

                /* Get the target node (next element) */
                ast_node *next_element = path->elements->items[i + 1];
                if (next_element->type != AST_NODE_NODE_PATTERN) {
                    set_result_error(result, "Expected node after relationship in MERGE");
                    free_variable_map(var_map);
                    return -1;
                }

                cypher_node_pattern *target_pattern = (cypher_node_pattern*)next_element;
                int target_node_id = -1;
                bool target_was_created = false;

                /* Check if target variable already exists in var_map */
                if (target_pattern->variable) {
                    target_node_id = get_variable_node_id(var_map, target_pattern->variable);
                }

                if (target_node_id < 0) {
                    /* Try to find existing target node by label + properties */
                    target_node_id = find_node_by_pattern(executor, target_pattern);

                    if (target_node_id >= 0) {
                        nodes_matched++;
                        CYPHER_DEBUG("MERGE relationship: matched existing target node %d", target_node_id);
                    } else {
                        /* Create the target node */
                        target_node_id = cypher_schema_create_node(executor->schema_mgr);
                        if (target_node_id < 0) {
                            set_result_error(result, "Failed to create target node in MERGE");
                            free_variable_map(var_map);
                            return -1;
                        }

                        target_was_created = true;
                        nodes_created_in_merge++;
                        result->nodes_created++;
                        CYPHER_DEBUG("MERGE relationship: created target node %d", target_node_id);

                        /* Add labels to target node */
                        if (has_labels(target_pattern)) {
                            for (int li = 0; li < target_pattern->labels->count; li++) {
                                const char *label = get_label_string(target_pattern->labels->items[li]);
                                if (label) {
                                    cypher_schema_add_node_label(executor->schema_mgr, target_node_id, label);
                                }
                            }
                        }

                        /* Add properties to target node */
                        if (target_pattern->properties && target_pattern->properties->type == AST_NODE_MAP) {
                            cypher_map *map = (cypher_map*)target_pattern->properties;
                            if (map->pairs) {
                                for (int j = 0; j < map->pairs->count; j++) {
                                    cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                                    if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                        cypher_literal *lit = (cypher_literal*)pair->value;
                                        property_type prop_type = PROP_TYPE_TEXT;
                                        const void *prop_value = NULL;

                                        switch (lit->literal_type) {
                                            case LITERAL_STRING:
                                                prop_type = PROP_TYPE_TEXT;
                                                prop_value = lit->value.string;
                                                break;
                                            case LITERAL_INTEGER:
                                                prop_type = PROP_TYPE_INTEGER;
                                                prop_value = &lit->value.integer;
                                                break;
                                            case LITERAL_DECIMAL:
                                                prop_type = PROP_TYPE_REAL;
                                                prop_value = &lit->value.decimal;
                                                break;
                                            case LITERAL_BOOLEAN:
                                                prop_type = PROP_TYPE_BOOLEAN;
                                                prop_value = &lit->value.boolean;
                                                break;
                                            default:
                                                continue;
                                        }

                                        if (prop_value) {
                                            cypher_schema_set_node_property(executor->schema_mgr, target_node_id, pair->key, prop_type, prop_value);
                                            result->properties_set++;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    /* Store target variable mapping */
                    if (target_pattern->variable) {
                        set_variable_node_id(var_map, target_pattern->variable, target_node_id);
                    }
                }

                /* Now handle the relationship itself */
                cypher_rel_pattern *rel_pattern = (cypher_rel_pattern*)element;
                const char *rel_type = rel_pattern->type ? rel_pattern->type : "RELATED";

                /* Determine direction */
                int source_id, dest_id;
                if (rel_pattern->left_arrow && !rel_pattern->right_arrow) {
                    /* <-[:TYPE]- (reversed) */
                    source_id = target_node_id;
                    dest_id = previous_node_id;
                } else {
                    /* -[:TYPE]-> or -[:TYPE]- (forward or undirected) */
                    source_id = previous_node_id;
                    dest_id = target_node_id;
                }

                /* Try to find existing edge */
                int edge_id = find_edge_by_pattern(executor, source_id, dest_id, rel_type, rel_pattern);
                bool edge_was_created = false;

                if (edge_id >= 0) {
                    CYPHER_DEBUG("MERGE matched existing edge %d", edge_id);
                } else {
                    /* Create new edge */
                    edge_id = cypher_schema_create_edge(executor->schema_mgr, source_id, dest_id, rel_type);
                    if (edge_id < 0) {
                        set_result_error(result, "Failed to create relationship in MERGE");
                        free_variable_map(var_map);
                        return -1;
                    }

                    edge_was_created = true;
                    result->relationships_created++;
                    CYPHER_DEBUG("MERGE created new edge %d: %d -[:%s]-> %d", edge_id, source_id, rel_type, dest_id);

                    /* Add relationship properties */
                    if (rel_pattern->properties && rel_pattern->properties->type == AST_NODE_MAP) {
                        cypher_map *map = (cypher_map*)rel_pattern->properties;
                        if (map->pairs) {
                            for (int j = 0; j < map->pairs->count; j++) {
                                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                                if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                    cypher_literal *lit = (cypher_literal*)pair->value;
                                    property_type prop_type = PROP_TYPE_TEXT;
                                    const void *prop_value = NULL;

                                    switch (lit->literal_type) {
                                        case LITERAL_STRING:
                                            prop_type = PROP_TYPE_TEXT;
                                            prop_value = lit->value.string;
                                            break;
                                        case LITERAL_INTEGER:
                                            prop_type = PROP_TYPE_INTEGER;
                                            prop_value = &lit->value.integer;
                                            break;
                                        case LITERAL_DECIMAL:
                                            prop_type = PROP_TYPE_REAL;
                                            prop_value = &lit->value.decimal;
                                            break;
                                        case LITERAL_BOOLEAN:
                                            prop_type = PROP_TYPE_BOOLEAN;
                                            prop_value = &lit->value.boolean;
                                            break;
                                        default:
                                            continue;
                                    }

                                    if (prop_value) {
                                        cypher_schema_set_edge_property(executor->schema_mgr, edge_id, pair->key, prop_type, prop_value);
                                        result->properties_set++;
                                    }
                                }
                            }
                        }
                    }
                }

                /* Apply ON CREATE SET if edge was created */
                if (edge_was_created && merge->on_create && merge->on_create->count > 0) {
                    CYPHER_DEBUG("Applying ON CREATE SET for edge %d", edge_id);
                    /* Note: Currently ON CREATE/ON MATCH SET for relationships would need
                       relationship variable support in var_map - for now, handled via node SET */
                }

                /* Apply ON MATCH SET if edge was matched */
                if (!edge_was_created && merge->on_match && merge->on_match->count > 0) {
                    CYPHER_DEBUG("Applying ON MATCH SET for edge %d", edge_id);
                }

                /* Also apply ON CREATE/MATCH for target node if it was created/matched */
                if (target_was_created && merge->on_create && merge->on_create->count > 0) {
                    if (execute_set_items(executor, merge->on_create, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }
                if (!target_was_created && merge->on_match && merge->on_match->count > 0) {
                    if (execute_set_items(executor, merge->on_match, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

                previous_node_id = target_node_id;
                /* Skip the next element since we already processed the target node */
                i++;
            }
        }
    }

    CYPHER_DEBUG("MERGE complete: %d nodes matched, %d nodes created", nodes_matched, nodes_created_in_merge);

    free_variable_map(var_map);
    return 0;
}


/* Execute MATCH+MERGE query combination */
int execute_match_merge_query(cypher_executor *executor, cypher_match *match, cypher_merge *merge, cypher_result *result)
{
    if (!executor || !match || !merge || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing MATCH+MERGE query");

    /* Transform MATCH clause to get bound variables */
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

    /* Finalize to assemble unified builder content into sql_buffer */
    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "Failed to finalize SQL generation");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Replace SELECT * with specific node ID selection */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
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

    CYPHER_DEBUG("Generated MATCH SQL for MERGE: %s", ctx->sql_buffer);

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
                CYPHER_DEBUG("MERGE bound variable '%s' to node %lld", var->name, (long long)node_id);
                col++;
            }
        }
        break; /* For now, just take the first match */
    }

    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);

    /* Now execute the MERGE clause with the bound variables */
    if (!merge->pattern) {
        set_result_error(result, "No pattern in MERGE clause");
        free_variable_map(var_map);
        return -1;
    }

    CYPHER_DEBUG("Executing MERGE clause with %d patterns", merge->pattern->count);

    /* Track which nodes were created vs matched */
    int nodes_matched = 0;
    int nodes_created_in_merge = 0;

    /* Process each path pattern in the MERGE clause */
    for (int p = 0; p < merge->pattern->count; p++) {
        ast_node *pattern = merge->pattern->items[p];

        if (pattern->type != AST_NODE_PATH) {
            CYPHER_DEBUG("Unexpected pattern type in MERGE: %d", pattern->type);
            continue;
        }

        cypher_path *path = (cypher_path*)pattern;
        if (!path->elements) continue;

        int previous_node_id = -1;

        /* Process path elements: node, rel, node, rel, node, ... */
        for (int i = 0; i < path->elements->count; i++) {
            ast_node *element = path->elements->items[i];

            if (element->type == AST_NODE_NODE_PATTERN) {
                cypher_node_pattern *node_pattern = (cypher_node_pattern*)element;
                int node_id = -1;
                bool was_created = false;

                /* Check if this variable already exists in var_map (from MATCH) */
                if (node_pattern->variable) {
                    node_id = get_variable_node_id(var_map, node_pattern->variable);
                    if (node_id >= 0) {
                        CYPHER_DEBUG("Using bound node %d for variable '%s'", node_id, node_pattern->variable);
                        nodes_matched++;
                        previous_node_id = node_id;
                        continue;
                    }
                }

                /* Try to find existing node by label + properties */
                node_id = find_node_by_pattern(executor, node_pattern);

                if (node_id >= 0) {
                    nodes_matched++;
                    CYPHER_DEBUG("MERGE matched existing node %d", node_id);
                } else {
                    /* Node doesn't exist - create it */
                    node_id = cypher_schema_create_node(executor->schema_mgr);
                    if (node_id < 0) {
                        set_result_error(result, "Failed to create node in MERGE");
                        free_variable_map(var_map);
                        return -1;
                    }

                    was_created = true;
                    nodes_created_in_merge++;
                    result->nodes_created++;
                    CYPHER_DEBUG("MERGE created new node %d", node_id);

                    /* Add labels if specified */
                    if (has_labels(node_pattern)) {
                        for (int li = 0; li < node_pattern->labels->count; li++) {
                            const char *label = get_label_string(node_pattern->labels->items[li]);
                            if (label) {
                                cypher_schema_add_node_label(executor->schema_mgr, node_id, label);
                            }
                        }
                    }

                    /* Add properties if specified */
                    if (node_pattern->properties && node_pattern->properties->type == AST_NODE_MAP) {
                        cypher_map *map = (cypher_map*)node_pattern->properties;
                        if (map->pairs) {
                            for (int j = 0; j < map->pairs->count; j++) {
                                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                                if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                    cypher_literal *lit = (cypher_literal*)pair->value;
                                    property_type prop_type = PROP_TYPE_TEXT;
                                    const void *prop_value = NULL;

                                    switch (lit->literal_type) {
                                        case LITERAL_STRING:
                                            prop_type = PROP_TYPE_TEXT;
                                            prop_value = lit->value.string;
                                            break;
                                        case LITERAL_INTEGER:
                                            prop_type = PROP_TYPE_INTEGER;
                                            prop_value = &lit->value.integer;
                                            break;
                                        case LITERAL_DECIMAL:
                                            prop_type = PROP_TYPE_REAL;
                                            prop_value = &lit->value.decimal;
                                            break;
                                        case LITERAL_BOOLEAN:
                                            prop_type = PROP_TYPE_BOOLEAN;
                                            prop_value = &lit->value.boolean;
                                            break;
                                        default:
                                            continue;
                                    }

                                    if (prop_value) {
                                        cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, prop_type, prop_value);
                                        result->properties_set++;
                                    }
                                }
                            }
                        }
                    }
                }

                /* Store variable mapping */
                if (node_pattern->variable) {
                    set_variable_node_id(var_map, node_pattern->variable, node_id);
                }

                previous_node_id = node_id;

                /* Apply ON CREATE SET if node was created */
                if (was_created && merge->on_create && merge->on_create->count > 0) {
                    CYPHER_DEBUG("Applying ON CREATE SET for node %d", node_id);
                    if (execute_set_items(executor, merge->on_create, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

                /* Apply ON MATCH SET if node was matched */
                if (!was_created && merge->on_match && merge->on_match->count > 0) {
                    CYPHER_DEBUG("Applying ON MATCH SET for node %d", node_id);
                    if (execute_set_items(executor, merge->on_match, var_map, result) < 0) {
                        free_variable_map(var_map);
                        return -1;
                    }
                }

            } else if (element->type == AST_NODE_REL_PATTERN) {
                /* Handle relationship in MERGE path */
                if (previous_node_id < 0 || i + 1 >= path->elements->count) {
                    set_result_error(result, "Invalid relationship pattern in MERGE");
                    free_variable_map(var_map);
                    return -1;
                }

                /* Get the target node (next element) */
                ast_node *next_element = path->elements->items[i + 1];
                if (next_element->type != AST_NODE_NODE_PATTERN) {
                    set_result_error(result, "Expected node after relationship in MERGE");
                    free_variable_map(var_map);
                    return -1;
                }

                cypher_node_pattern *target_pattern = (cypher_node_pattern*)next_element;
                int target_node_id = -1;

                /* Check if target variable already exists in var_map (from MATCH) */
                if (target_pattern->variable) {
                    target_node_id = get_variable_node_id(var_map, target_pattern->variable);
                    if (target_node_id >= 0) {
                        CYPHER_DEBUG("Using bound target node %d for variable '%s'", target_node_id, target_pattern->variable);
                    }
                }

                if (target_node_id < 0) {
                    /* Try to find existing target node by label + properties */
                    target_node_id = find_node_by_pattern(executor, target_pattern);

                    if (target_node_id >= 0) {
                        nodes_matched++;
                        CYPHER_DEBUG("MERGE relationship: matched existing target node %d", target_node_id);
                    } else {
                        /* Create the target node */
                        target_node_id = cypher_schema_create_node(executor->schema_mgr);
                        if (target_node_id < 0) {
                            set_result_error(result, "Failed to create target node in MERGE");
                            free_variable_map(var_map);
                            return -1;
                        }

                        nodes_created_in_merge++;
                        result->nodes_created++;
                        CYPHER_DEBUG("MERGE relationship: created target node %d", target_node_id);

                        /* Add labels to target node */
                        if (has_labels(target_pattern)) {
                            for (int li = 0; li < target_pattern->labels->count; li++) {
                                const char *label = get_label_string(target_pattern->labels->items[li]);
                                if (label) {
                                    cypher_schema_add_node_label(executor->schema_mgr, target_node_id, label);
                                }
                            }
                        }

                        /* Add properties to target node */
                        if (target_pattern->properties && target_pattern->properties->type == AST_NODE_MAP) {
                            cypher_map *map = (cypher_map*)target_pattern->properties;
                            if (map->pairs) {
                                for (int j = 0; j < map->pairs->count; j++) {
                                    cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                                    if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                        cypher_literal *lit = (cypher_literal*)pair->value;
                                        property_type prop_type = PROP_TYPE_TEXT;
                                        const void *prop_value = NULL;

                                        switch (lit->literal_type) {
                                            case LITERAL_STRING:
                                                prop_type = PROP_TYPE_TEXT;
                                                prop_value = lit->value.string;
                                                break;
                                            case LITERAL_INTEGER:
                                                prop_type = PROP_TYPE_INTEGER;
                                                prop_value = &lit->value.integer;
                                                break;
                                            case LITERAL_DECIMAL:
                                                prop_type = PROP_TYPE_REAL;
                                                prop_value = &lit->value.decimal;
                                                break;
                                            case LITERAL_BOOLEAN:
                                                prop_type = PROP_TYPE_BOOLEAN;
                                                prop_value = &lit->value.boolean;
                                                break;
                                            default:
                                                continue;
                                        }

                                        if (prop_value) {
                                            cypher_schema_set_node_property(executor->schema_mgr, target_node_id, pair->key, prop_type, prop_value);
                                            result->properties_set++;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    /* Store target variable mapping */
                    if (target_pattern->variable) {
                        set_variable_node_id(var_map, target_pattern->variable, target_node_id);
                    }
                }

                /* Handle the relationship itself */
                cypher_rel_pattern *rel_pattern = (cypher_rel_pattern*)element;
                const char *rel_type = rel_pattern->type ? rel_pattern->type : "RELATED";

                /* Determine direction */
                int source_id, dest_id;
                if (rel_pattern->left_arrow && !rel_pattern->right_arrow) {
                    source_id = target_node_id;
                    dest_id = previous_node_id;
                } else {
                    source_id = previous_node_id;
                    dest_id = target_node_id;
                }

                /* Try to find existing edge */
                int edge_id = find_edge_by_pattern(executor, source_id, dest_id, rel_type, rel_pattern);

                if (edge_id >= 0) {
                    CYPHER_DEBUG("MERGE matched existing edge %d", edge_id);
                } else {
                    /* Create new edge */
                    edge_id = cypher_schema_create_edge(executor->schema_mgr, source_id, dest_id, rel_type);
                    if (edge_id < 0) {
                        set_result_error(result, "Failed to create relationship in MERGE");
                        free_variable_map(var_map);
                        return -1;
                    }

                    result->relationships_created++;
                    CYPHER_DEBUG("MERGE created new edge %d: %d -[:%s]-> %d", edge_id, source_id, rel_type, dest_id);

                    /* Add relationship properties */
                    if (rel_pattern->properties && rel_pattern->properties->type == AST_NODE_MAP) {
                        cypher_map *map = (cypher_map*)rel_pattern->properties;
                        if (map->pairs) {
                            for (int j = 0; j < map->pairs->count; j++) {
                                cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                                if (pair->key && pair->value && pair->value->type == AST_NODE_LITERAL) {
                                    cypher_literal *lit = (cypher_literal*)pair->value;
                                    property_type prop_type = PROP_TYPE_TEXT;
                                    const void *prop_value = NULL;

                                    switch (lit->literal_type) {
                                        case LITERAL_STRING:
                                            prop_type = PROP_TYPE_TEXT;
                                            prop_value = lit->value.string;
                                            break;
                                        case LITERAL_INTEGER:
                                            prop_type = PROP_TYPE_INTEGER;
                                            prop_value = &lit->value.integer;
                                            break;
                                        case LITERAL_DECIMAL:
                                            prop_type = PROP_TYPE_REAL;
                                            prop_value = &lit->value.decimal;
                                            break;
                                        case LITERAL_BOOLEAN:
                                            prop_type = PROP_TYPE_BOOLEAN;
                                            prop_value = &lit->value.boolean;
                                            break;
                                        default:
                                            continue;
                                    }

                                    if (prop_value) {
                                        cypher_schema_set_edge_property(executor->schema_mgr, edge_id, pair->key, prop_type, prop_value);
                                        result->properties_set++;
                                    }
                                }
                            }
                        }
                    }
                }

                previous_node_id = target_node_id;
                /* Skip the next element since we already processed the target node */
                i++;
            }
        }
    }

    CYPHER_DEBUG("MERGE complete: %d nodes matched, %d nodes created", nodes_matched, nodes_created_in_merge);

    free_variable_map(var_map);
    return 0;
}

