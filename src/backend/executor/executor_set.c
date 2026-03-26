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

/* Evaluate a function call by transforming to SQL and executing via SQLite.
 * Returns 0 on success, -1 on error, -2 for NULL result. */
static int evaluate_function_call_via_sqlite(
    cypher_executor *executor,
    cypher_function_call *func_call,
    property_type *out_type,
    void *out_value,
    size_t value_size)
{
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) return -1;

    append_sql(ctx, "SELECT ");
    if (transform_expression(ctx, (ast_node*)func_call) < 0) {
        cypher_transform_free_context(ctx);
        return -1;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(executor->db, ctx->sql_buffer, -1, &stmt, NULL);
    cypher_transform_free_context(ctx);
    if (rc != SQLITE_OK) return -1;

    if (executor->params_json) {
        if (bind_params_from_json(stmt, executor->params_json) < 0) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int col_type = sqlite3_column_type(stmt, 0);
    switch (col_type) {
        case SQLITE_INTEGER:
            *out_type = PROP_TYPE_INTEGER;
            *(int64_t*)out_value = sqlite3_column_int64(stmt, 0);
            break;
        case SQLITE_FLOAT:
            *out_type = PROP_TYPE_REAL;
            *(double*)out_value = sqlite3_column_double(stmt, 0);
            break;
        case SQLITE_TEXT: {
            const char *text = (const char*)sqlite3_column_text(stmt, 0);
            if (text && (*text == '{' || *text == '[')) {
                *out_type = PROP_TYPE_JSON;
            } else {
                *out_type = PROP_TYPE_TEXT;
            }
            if (text) {
                strncpy((char*)out_value, text, value_size - 1);
                ((char*)out_value)[value_size - 1] = '\0';
            } else {
                ((char*)out_value)[0] = '\0';
            }
            break;
        }
        case SQLITE_NULL:
            sqlite3_finalize(stmt);
            return -2;
        default:
            sqlite3_finalize(stmt);
            return -1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* Parse a JSON object string and set each key-value pair as a property.
 * Follows the same hand-rolled JSON parsing pattern as get_param_value(). */
static int set_properties_from_json_object(
    cypher_executor *executor,
    int entity_id,
    bool is_edge,
    const char *json_str,
    cypher_result *result)
{
    const char *p = json_str;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    if (*p != '{') {
        set_result_error(result, "Bulk SET parameter must be a JSON object");
        return -1;
    }
    p++;

    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }

        /* Parse key */
        if (*p != '"') {
            set_result_error(result, "Invalid JSON in bulk SET parameter");
            return -1;
        }
        p++;
        const char *key_start = p;
        while (*p && *p != '"') p++;
        if (!*p) {
            set_result_error(result, "Unterminated string in bulk SET parameter");
            return -1;
        }
        size_t key_len = p - key_start;
        char key[256];
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        memcpy(key, key_start, key_len);
        key[key_len] = '\0';
        p++; /* closing quote */

        while (*p && *p != ':') p++;
        if (!*p) break;
        p++; /* colon */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        /* Parse value */
        property_type pt;
        char str_buf[4096];
        const void *pv = NULL;
        int64_t json_int_buf;
        double json_real_buf;
        int json_bool_buf;

        if (*p == '"') {
            /* String */
            p++;
            size_t i = 0;
            while (*p && *p != '"' && i < sizeof(str_buf) - 1) {
                if (*p == '\\' && *(p+1)) {
                    p++;
                    switch (*p) {
                        case 'n': str_buf[i++] = '\n'; break;
                        case 't': str_buf[i++] = '\t'; break;
                        case 'r': str_buf[i++] = '\r'; break;
                        case '"': str_buf[i++] = '"'; break;
                        case '\\': str_buf[i++] = '\\'; break;
                        default: str_buf[i++] = *p; break;
                    }
                } else {
                    str_buf[i++] = *p;
                }
                p++;
            }
            str_buf[i] = '\0';
            /* Skip past closing quote, even if buffer was full */
            while (*p && *p != '"') {
                if (*p == '\\' && *(p+1)) p++;
                p++;
            }
            if (*p == '"') p++;
            pt = PROP_TYPE_TEXT;
            pv = str_buf;
        } else if (*p == 't') {
            pt = PROP_TYPE_BOOLEAN;
            json_bool_buf = 1;
            pv = &json_bool_buf;
            while (*p && *p != ',' && *p != '}') p++;
        } else if (*p == 'f') {
            pt = PROP_TYPE_BOOLEAN;
            json_bool_buf = 0;
            pv = &json_bool_buf;
            while (*p && *p != ',' && *p != '}') p++;
        } else if (*p == 'n') {
            /* null — skip this property */
            p += 4;
            continue;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            const char *num_start = p;
            bool is_float = false;
            if (*p == '-') p++;
            while (*p >= '0' && *p <= '9') p++;
            if (*p == '.') { is_float = true; p++; while (*p >= '0' && *p <= '9') p++; }
            if (*p == 'e' || *p == 'E') { is_float = true; p++; if (*p == '+' || *p == '-') p++; while (*p >= '0' && *p <= '9') p++; }
            if (is_float) {
                pt = PROP_TYPE_REAL;
                json_real_buf = strtod(num_start, NULL);
                pv = &json_real_buf;
            } else {
                pt = PROP_TYPE_INTEGER;
                json_int_buf = strtoll(num_start, NULL, 10);
                pv = &json_int_buf;
            }
        } else if (*p == '{' || *p == '[') {
            /* Nested object/array — store as JSON */
            const char *json_start = p;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') depth--;
                else if (*p == '"') {
                    p++;
                    while (*p && *p != '"') {
                        if (*p == '\\' && *(p+1)) p++;
                        p++;
                    }
                }
                if (*p) p++;
            }
            size_t json_len = p - json_start;
            if (json_len >= sizeof(str_buf)) json_len = sizeof(str_buf) - 1;
            memcpy(str_buf, json_start, json_len);
            str_buf[json_len] = '\0';
            pt = PROP_TYPE_JSON;
            pv = str_buf;
        } else {
            /* Skip unknown value types */
            while (*p && *p != ',' && *p != '}') p++;
            continue;
        }

        int rc;
        if (is_edge) {
            rc = cypher_schema_set_edge_property(executor->schema_mgr, entity_id, key, pt, pv);
        } else {
            rc = cypher_schema_set_node_property(executor->schema_mgr, entity_id, key, pt, pv);
        }
        if (rc == 0) {
            result->properties_set++;
        }
    }

    return 0;
}

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
                CYPHER_DEBUG("Bound variable '%s' to node %lld", var->name, (long long)node_id);
                col++;
            } else if (var && var->kind == VAR_KIND_EDGE) {
                int64_t edge_id = sqlite3_column_int64(stmt, col);
                set_variable_edge_id(var_map, var->name, (int)edge_id);
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

        /* Handle bulk SET (SET n = {map} or SET n += {map}) */
        if (item->property->type == AST_NODE_IDENTIFIER) {
            cypher_identifier *var_id = (cypher_identifier*)item->property;

            if (!item->expr) {
                set_result_error(result, "Bulk SET requires a value expression");
                return -1;
            }

            /* Resolve the map expression — map literal or parameter */
            cypher_map *map = NULL;
            char *resolved_json = NULL;
            static char param_json_buf[4096];

            if (item->expr->type == AST_NODE_MAP) {
                map = (cypher_map*)item->expr;
            } else if (item->expr->type == AST_NODE_PARAMETER && executor->params_json) {
                cypher_parameter *param = (cypher_parameter*)item->expr;
                property_type pt;
                int rc = get_param_value(executor->params_json, param->name,
                                         &pt, param_json_buf, sizeof(param_json_buf));
                if (rc == -2) {
                    /* null parameter — fall through with resolved_json = NULL */
                } else if (rc < 0) {
                    char error[256];
                    snprintf(error, sizeof(error), "Parameter '%s' not found for bulk SET", param->name);
                    set_result_error(result, error);
                    return -1;
                } else if (pt != PROP_TYPE_JSON) {
                    set_result_error(result, "Bulk SET parameter must be a JSON object");
                    return -1;
                } else {
                    resolved_json = param_json_buf;
                }
            } else {
                set_result_error(result, "Bulk SET value must be a map literal or parameter");
                return -1;
            }

            /* Determine if node or edge */
            bool is_edge = is_variable_edge(var_map, var_id->name);
            int entity_id;

            if (is_edge) {
                entity_id = get_variable_edge_id(var_map, var_id->name);
                if (entity_id < 0) {
                    char error[256];
                    snprintf(error, sizeof(error), "Unbound edge variable in bulk SET: %s", var_id->name);
                    set_result_error(result, error);
                    return -1;
                }
            } else {
                entity_id = get_variable_node_id(var_map, var_id->name);
                if (entity_id < 0) {
                    char error[256];
                    snprintf(error, sizeof(error), "Unbound variable in bulk SET: %s", var_id->name);
                    set_result_error(result, error);
                    return -1;
                }
            }

            /* For replace mode (=), delete all existing properties first */
            if (!item->is_merge) {
                if (is_edge) {
                    cypher_schema_delete_all_edge_properties(executor->schema_mgr, entity_id);
                } else {
                    cypher_schema_delete_all_node_properties(executor->schema_mgr, entity_id);
                }
            }

            /* Set each property from the map */
            if (map && map->pairs) {
                for (int j = 0; j < map->pairs->count; j++) {
                    cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                    if (!pair || !pair->key || !pair->value) continue;

                    property_type pt = PROP_TYPE_TEXT;
                    const void *pv = NULL;
                    static char bulk_str_buf[4096];

                    if (pair->value->type == AST_NODE_LITERAL) {
                        cypher_literal *lit = (cypher_literal*)pair->value;
                        switch (lit->literal_type) {
                            case LITERAL_STRING:
                                pt = PROP_TYPE_TEXT;
                                pv = lit->value.string;
                                break;
                            case LITERAL_INTEGER:
                                pt = PROP_TYPE_INTEGER;
                                pv = &lit->value.integer;
                                break;
                            case LITERAL_DECIMAL:
                                pt = PROP_TYPE_REAL;
                                pv = &lit->value.decimal;
                                break;
                            case LITERAL_BOOLEAN:
                                pt = PROP_TYPE_BOOLEAN;
                                pv = &lit->value.boolean;
                                break;
                            case LITERAL_NULL:
                                continue; /* Skip null values */
                        }
                    } else if (pair->value->type == AST_NODE_MAP || pair->value->type == AST_NODE_LIST) {
                        char *json_str = serialize_ast_to_json(pair->value);
                        if (!json_str) {
                            set_result_error(result, "Failed to serialize map/list value in bulk SET");
                            return -1;
                        }
                        pt = PROP_TYPE_JSON;
                        /* Store in static buffer for schema call */
                        strncpy(bulk_str_buf, json_str, sizeof(bulk_str_buf) - 1);
                        bulk_str_buf[sizeof(bulk_str_buf) - 1] = '\0';
                        free(json_str);
                        pv = bulk_str_buf;
                    } else {
                        continue; /* Skip unsupported value types */
                    }

                    int rc;
                    if (is_edge) {
                        rc = cypher_schema_set_edge_property(executor->schema_mgr, entity_id, pair->key, pt, pv);
                    } else {
                        rc = cypher_schema_set_node_property(executor->schema_mgr, entity_id, pair->key, pt, pv);
                    }
                    if (rc == 0) {
                        result->properties_set++;
                    }
                }
            }

            /* Set properties from resolved JSON parameter */
            if (resolved_json) {
                if (set_properties_from_json_object(executor, entity_id, is_edge,
                                                     resolved_json, result) < 0) {
                    return -1;
                }
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
        } else if (item->expr->type == AST_NODE_MAP || item->expr->type == AST_NODE_LIST) {
            /* Map or list literal - serialize to JSON and store as JSON type */
            char *json_str = serialize_ast_to_json(item->expr);
            if (!json_str) {
                set_result_error(result, "Failed to serialize map/list to JSON");
                return -1;
            }
            prop_type = PROP_TYPE_JSON;
            prop_value = json_str;

            /* Set property and free json_str immediately */
            if (is_edge) {
                if (cypher_schema_set_edge_property(executor->schema_mgr, entity_id, prop->property_name, prop_type, prop_value) == 0) {
                    result->properties_set++;
                    CYPHER_DEBUG("Set JSON property '%s' on edge %d", prop->property_name, entity_id);
                } else {
                    free(json_str);
                    char error[512];
                    snprintf(error, sizeof(error), "Failed to set JSON property '%s' on edge %d", prop->property_name, entity_id);
                    set_result_error(result, error);
                    return -1;
                }
            } else {
                if (cypher_schema_set_node_property(executor->schema_mgr, entity_id, prop->property_name, prop_type, prop_value) == 0) {
                    result->properties_set++;
                    CYPHER_DEBUG("Set JSON property '%s' on node %d", prop->property_name, entity_id);
                } else {
                    free(json_str);
                    char error[512];
                    snprintf(error, sizeof(error), "Failed to set JSON property '%s' on node %d", prop->property_name, entity_id);
                    set_result_error(result, error);
                    return -1;
                }
            }
            free(json_str);
            continue;
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
                } else if (prop_type == PROP_TYPE_JSON) {
                    prop_value = set_str_buf;
                }
            } else {
                char error[256];
                snprintf(error, sizeof(error), "Parameter '%s' not found in params_json", param->name);
                set_result_error(result, error);
                return -1;
            }
        } else if (item->expr->type == AST_NODE_FUNCTION_CALL) {
            cypher_function_call *func = (cypher_function_call*)item->expr;
            int rc = evaluate_function_call_via_sqlite(
                executor, func, &prop_type, set_str_buf, sizeof(set_str_buf));
            if (rc == -2) {
                /* NULL result — skip property */
                continue;
            } else if (rc < 0) {
                char error[256];
                snprintf(error, sizeof(error), "Failed to evaluate function '%s' in SET",
                         func->function_name ? func->function_name : "unknown");
                set_result_error(result, error);
                return -1;
            }
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
            set_result_error(result, "SET value must be a literal, map, list, parameter, or function call");
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
