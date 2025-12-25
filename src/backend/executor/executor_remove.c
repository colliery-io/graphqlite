/*
 * REMOVE Clause Execution
 * Handles MATCH+REMOVE query execution and property/label removal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

/* Execute MATCH+REMOVE query combination */
int execute_match_remove_query(cypher_executor *executor, cypher_match *match, cypher_remove *remove, cypher_result *result)
{
    if (!executor || !match || !remove || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing MATCH+REMOVE query");

    /* Transform MATCH clause to get bound variables */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }

    if (transform_match_clause(ctx, match) < 0) {
        CYPHER_DEBUG("Transform MATCH failed: %s", ctx->error_message ? ctx->error_message : "No error message");
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Replace SELECT * with specific node/edge ID selection */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
        char *after_star = select_pos + strlen("SELECT *");
        char *temp = strdup(after_star);

        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';

        /* Select all node and edge variables found in the MATCH */
        bool first = true;
        for (int i = 0; i < ctx->variable_count; i++) {
            if (ctx->variables[i].type == VAR_TYPE_NODE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", ctx->variables[i].table_alias, ctx->variables[i].name);
                first = false;
            } else if (ctx->variables[i].type == VAR_TYPE_EDGE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", ctx->variables[i].table_alias, ctx->variables[i].name);
                first = false;
            }
        }

        append_sql(ctx, " %s", temp);
        free(temp);
    }

    CYPHER_DEBUG("Generated MATCH SQL for REMOVE: %s", ctx->sql_buffer);

    /* Execute the MATCH query to get node IDs */
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

    /* Process each matched row and apply REMOVE operations */
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        /* Create variable map from MATCH results */
        variable_map *var_map = create_variable_map();
        if (!var_map) {
            set_result_error(result, "Failed to create variable map");
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }

        /* Bind variables to matched node and edge IDs */
        int col = 0;
        for (int i = 0; i < ctx->variable_count; i++) {
            if (ctx->variables[i].type == VAR_TYPE_NODE) {
                int64_t node_id = sqlite3_column_int64(stmt, col);
                set_variable_node_id(var_map, ctx->variables[i].name, (int)node_id);
                CYPHER_DEBUG("Bound variable '%s' to node %lld", ctx->variables[i].name, (long long)node_id);
                col++;
            } else if (ctx->variables[i].type == VAR_TYPE_EDGE) {
                int64_t edge_id = sqlite3_column_int64(stmt, col);
                set_variable_edge_id(var_map, ctx->variables[i].name, (int)edge_id);
                CYPHER_DEBUG("Bound variable '%s' to edge %lld", ctx->variables[i].name, (long long)edge_id);
                col++;
            }
        }

        /* Execute REMOVE operations for this matched row */
        if (execute_remove_operations(executor, remove, var_map, result) < 0) {
            free_variable_map(var_map);
            sqlite3_finalize(stmt);
            cypher_transform_free_context(ctx);
            return -1;
        }

        free_variable_map(var_map);
    }

    sqlite3_finalize(stmt);
    cypher_transform_free_context(ctx);
    return 0;
}

/* Execute REMOVE operations with variable bindings */
int execute_remove_operations(cypher_executor *executor, cypher_remove *remove, variable_map *var_map, cypher_result *result)
{
    if (!executor || !remove || !var_map || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing REMOVE operations with %d items", remove->items->count);

    /* Process each REMOVE item */
    for (int i = 0; i < remove->items->count; i++) {
        cypher_remove_item *item = (cypher_remove_item*)remove->items->items[i];

        if (!item->target) {
            set_result_error(result, "Invalid REMOVE item");
            return -1;
        }

        /* Handle label removal (REMOVE n:Label) */
        if (item->target->type == AST_NODE_LABEL_EXPR) {
            cypher_label_expr *label_expr = (cypher_label_expr*)item->target;

            if (!label_expr->expr || label_expr->expr->type != AST_NODE_IDENTIFIER) {
                set_result_error(result, "REMOVE label must be on a variable");
                return -1;
            }

            cypher_identifier *var_id = (cypher_identifier*)label_expr->expr;

            /* Get the node ID for this variable */
            int node_id = get_variable_node_id(var_map, var_id->name);
            if (node_id < 0) {
                char error[256];
                snprintf(error, sizeof(error), "Unbound variable in REMOVE label: %s", var_id->name);
                set_result_error(result, error);
                return -1;
            }

            /* Remove the label from the node */
            if (cypher_schema_remove_node_label(executor->schema_mgr, node_id, label_expr->label_name) == 0) {
                result->properties_set++; /* Reuse counter for label removals */
                CYPHER_DEBUG("Removed label '%s' from node %d", label_expr->label_name, node_id);
            } else {
                /* Label removal failing is not necessarily an error - it might not exist */
                CYPHER_DEBUG("Label '%s' not found on node %d (or already removed)", label_expr->label_name, node_id);
            }
            continue;
        }

        /* Handle property removal (REMOVE n.prop) */
        if (item->target->type != AST_NODE_PROPERTY) {
            set_result_error(result, "REMOVE target must be a property (variable.property) or label (variable:Label)");
            return -1;
        }

        cypher_property *prop = (cypher_property*)item->target;
        if (!prop->expr || prop->expr->type != AST_NODE_IDENTIFIER) {
            set_result_error(result, "REMOVE property must be on a variable");
            return -1;
        }

        cypher_identifier *var_id = (cypher_identifier*)prop->expr;

        /* Check if this is a node or edge variable */
        bool is_edge = is_variable_edge(var_map, var_id->name);
        int entity_id;

        if (is_edge) {
            entity_id = get_variable_edge_id(var_map, var_id->name);
            if (entity_id < 0) {
                char error[256];
                snprintf(error, sizeof(error), "Unbound edge variable in REMOVE: %s", var_id->name);
                set_result_error(result, error);
                return -1;
            }

            /* Delete the property from the edge */
            if (cypher_schema_delete_edge_property(executor->schema_mgr, entity_id, prop->property_name) == 0) {
                result->properties_set++;
                CYPHER_DEBUG("Removed property '%s' from edge %d", prop->property_name, entity_id);
            } else {
                /* Property removal failing is not necessarily an error - it might not exist */
                CYPHER_DEBUG("Property '%s' not found on edge %d (or already removed)", prop->property_name, entity_id);
            }
        } else {
            entity_id = get_variable_node_id(var_map, var_id->name);
            if (entity_id < 0) {
                char error[256];
                snprintf(error, sizeof(error), "Unbound variable in REMOVE: %s", var_id->name);
                set_result_error(result, error);
                return -1;
            }

            /* Delete the property from the node */
            if (cypher_schema_delete_node_property(executor->schema_mgr, entity_id, prop->property_name) == 0) {
                result->properties_set++;
                CYPHER_DEBUG("Removed property '%s' from node %d", prop->property_name, entity_id);
            } else {
                /* Property removal failing is not necessarily an error - it might not exist */
                CYPHER_DEBUG("Property '%s' not found on node %d (or already removed)", prop->property_name, entity_id);
            }
        }
    }

    return 0;
}
