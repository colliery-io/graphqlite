/*
 * SET Clause Execution
 * Handles MATCH+SET query execution and property/label updates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"
#include "transform/transform_variables.h"

/* Execute SET items from a list (used for ON CREATE/ON MATCH) */
int execute_set_items(cypher_executor *executor, ast_list *items, variable_map *var_map, cypher_result *result)
{
    if (!executor || !items || !var_map || !result) {
        return -1;
    }

    /* Create a temporary cypher_set to reuse execute_set_operations */
    cypher_set temp_set;
    temp_set.base.type = AST_NODE_SET;
    temp_set.items = items;

    return execute_set_operations(executor, &temp_set, var_map, result);
}

/* Execute MATCH+SET query combination */
int execute_match_set_query(cypher_executor *executor, cypher_match *match, cypher_set *set, cypher_result *result)
{
    if (!executor || !match || !set || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing MATCH+SET query");

    /* Transform MATCH clause to get bound variables */
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

    /* Finalize to assemble unified builder content into sql_buffer */
    if (finalize_sql_generation(ctx) < 0) {
        set_result_error(result, "Failed to finalize SQL generation");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Add SELECT to get matched node and edge IDs */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
        /* Replace SELECT * with specific node/edge ID selection */
        char *after_star = select_pos + strlen("SELECT *");
        char *temp = strdup(after_star);

        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';

        /* Select all node and edge variables found in the MATCH */
        bool first = true;
        int var_count = transform_var_count(ctx->var_ctx);
        for (int i = 0; i < var_count; i++) {
            transform_var *var = transform_var_at(ctx->var_ctx, i);
            if (var && var->kind == VAR_KIND_NODE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", var->table_alias, var->name);
                first = false;
            } else if (var && var->kind == VAR_KIND_EDGE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", var->table_alias, var->name);
                first = false;
            }
        }

        append_sql(ctx, " %s", temp);
        free(temp);
    }

    CYPHER_DEBUG("Generated MATCH SQL: %s", ctx->sql_buffer);

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

    /* Process each matched row and apply SET operations */
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
        int var_count2 = transform_var_count(ctx->var_ctx);
        for (int i = 0; i < var_count2; i++) {
            transform_var *var = transform_var_at(ctx->var_ctx, i);
            if (var && var->kind == VAR_KIND_NODE) {
                int64_t node_id = sqlite3_column_int64(stmt, col);
                set_variable_node_id(var_map, var->name, (int)node_id);
                printf("DEBUG - MATCH returned node_id=%lld for variable '%s'\n", (long long)node_id, var->name);
                CYPHER_DEBUG("Bound variable '%s' to node %lld", var->name, (long long)node_id);
                col++;
            } else if (var && var->kind == VAR_KIND_EDGE) {
                int64_t edge_id = sqlite3_column_int64(stmt, col);
                set_variable_edge_id(var_map, var->name, (int)edge_id);
                printf("DEBUG - MATCH returned edge_id=%lld for variable '%s'\n", (long long)edge_id, var->name);
                CYPHER_DEBUG("Bound variable '%s' to edge %lld", var->name, (long long)edge_id);
                col++;
            }
        }

        /* Execute SET operations for this matched row */
        if (execute_set_operations(executor, set, var_map, result) < 0) {
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

/* Execute standalone SET clause */
int execute_set_clause(cypher_executor *executor, cypher_set *set, cypher_result *result)
{
    if (!executor || !set || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing standalone SET clause");

    /* For standalone SET, we don't have any bound variables
     * This would typically be an error in real Cypher, but for
     * testing purposes we'll allow it */

    set_result_error(result, "SET clause requires MATCH to bind variables");
    return -1;
}

/* Execute SET operations with variable bindings */
int execute_set_operations(cypher_executor *executor, cypher_set *set, variable_map *var_map, cypher_result *result)
{
    if (!executor || !set || !var_map || !result) {
        return -1;
    }

    CYPHER_DEBUG("Executing SET operations");

    /* Process each SET item */
    for (int i = 0; i < set->items->count; i++) {
        cypher_set_item *item = (cypher_set_item*)set->items->items[i];

        if (!item->property) {
            set_result_error(result, "Invalid SET item");
            return -1;
        }

        /* Handle label assignment (SET n:Label) */
        if (item->property->type == AST_NODE_LABEL_EXPR) {
            cypher_label_expr *label_expr = (cypher_label_expr*)item->property;

            if (!label_expr->expr || label_expr->expr->type != AST_NODE_IDENTIFIER) {
                set_result_error(result, "SET label must be on a variable");
                return -1;
            }

            cypher_identifier *var_id = (cypher_identifier*)label_expr->expr;

            /* Get the node ID for this variable */
            int node_id = get_variable_node_id(var_map, var_id->name);
            if (node_id < 0) {
                char error[256];
                snprintf(error, sizeof(error), "Unbound variable in SET label: %s", var_id->name);
                set_result_error(result, error);
                return -1;
            }

            /* Add the label to the node */
            if (cypher_schema_add_node_label(executor->schema_mgr, node_id, label_expr->label_name) == 0) {
                result->properties_set++; /* We use properties_set counter for labels too */
                CYPHER_DEBUG("Added label '%s' to node %d", label_expr->label_name, node_id);
            } else {
                char error[512];
                snprintf(error, sizeof(error), "Failed to add label '%s' to node %d", label_expr->label_name, node_id);
                set_result_error(result, error);
                return -1;
            }
            continue;
        }

        /* Handle property assignment (SET n.prop = value) */
        if (item->property->type != AST_NODE_PROPERTY) {
            set_result_error(result, "SET target must be a property or label");
            return -1;
        }

        cypher_property *prop = (cypher_property*)item->property;
        if (!prop->expr || prop->expr->type != AST_NODE_IDENTIFIER) {
            set_result_error(result, "SET property must be on a variable");
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
                snprintf(error, sizeof(error), "Unbound edge variable in SET: %s", var_id->name);
                set_result_error(result, error);
                return -1;
            }
        } else {
            entity_id = get_variable_node_id(var_map, var_id->name);
            if (entity_id < 0) {
                char error[256];
                snprintf(error, sizeof(error), "Unbound variable in SET: %s", var_id->name);
                set_result_error(result, error);
                return -1;
            }
        }

        /* Evaluate the value expression */
        property_type prop_type = PROP_TYPE_TEXT;
        const void *prop_value = NULL;
        static char set_str_buf[4096];
        static int64_t set_int_buf;
        static double set_real_buf;
        static int set_bool_buf;

        if (!item->expr) {
            set_result_error(result, "SET value is missing");
            return -1;
        }

        if (item->expr->type == AST_NODE_LITERAL) {
            cypher_literal *lit = (cypher_literal*)item->expr;

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
                case LITERAL_NULL:
                    /* Skip null properties for now */
                    continue;
            }
        } else if (item->expr->type == AST_NODE_PARAMETER && executor->params_json) {
            /* Handle parameter substitution */
            cypher_parameter *param = (cypher_parameter*)item->expr;

            int rc = get_param_value(executor->params_json, param->name, &prop_type, set_str_buf, sizeof(set_str_buf));
            if (rc == -2) {
                /* null parameter - skip */
                continue;
            } else if (rc == 0) {
                if (prop_type == PROP_TYPE_TEXT) {
                    prop_value = set_str_buf;
                } else if (prop_type == PROP_TYPE_INTEGER) {
                    set_int_buf = *(int64_t*)set_str_buf;
                    prop_value = &set_int_buf;
                } else if (prop_type == PROP_TYPE_REAL) {
                    set_real_buf = *(double*)set_str_buf;
                    prop_value = &set_real_buf;
                } else if (prop_type == PROP_TYPE_BOOLEAN) {
                    set_bool_buf = *(int*)set_str_buf;
                    prop_value = &set_bool_buf;
                }
            } else {
                char error[256];
                snprintf(error, sizeof(error), "Parameter '%s' not found in params_json", param->name);
                set_result_error(result, error);
                return -1;
            }
        } else {
            set_result_error(result, "SET value must be a literal or parameter");
            return -1;
        }

        /* Set the property on the node or edge */
        if (prop_value) {
            if (is_edge) {
                if (cypher_schema_set_edge_property(executor->schema_mgr, entity_id, prop->property_name, prop_type, prop_value) == 0) {
                    result->properties_set++;
                    CYPHER_DEBUG("Set property '%s' = value on edge %d", prop->property_name, entity_id);
                } else {
                    char error[512];
                    snprintf(error, sizeof(error), "Failed to set property '%s' on edge %d", prop->property_name, entity_id);
                    set_result_error(result, error);
                    return -1;
                }
            } else {
                if (cypher_schema_set_node_property(executor->schema_mgr, entity_id, prop->property_name, prop_type, prop_value) == 0) {
                    result->properties_set++;
                    CYPHER_DEBUG("Set property '%s' = value on node %d", prop->property_name, entity_id);
                } else {
                    char error[512];
                    snprintf(error, sizeof(error), "Failed to set property '%s' on node %d", prop->property_name, entity_id);
                    set_result_error(result, error);
                    return -1;
                }
            }
        }
    }

    return 0;
}
