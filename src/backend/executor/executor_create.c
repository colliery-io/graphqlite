/*
 * CREATE Clause Execution
 * Handles CREATE clause and path pattern execution with variable tracking
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/executor_internal.h"
#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

/* Helper function to execute a single path pattern with variable tracking */
int execute_path_pattern_with_variables(cypher_executor *executor, cypher_path *path,
                                             cypher_result *result, variable_map *var_map)
{
    if (!path || !path->elements) {
        return -1;
    }

    CYPHER_DEBUG("Executing path with %d elements", path->elements->count);

    int previous_node_id = -1;

    /* Process path elements: node, rel, node, rel, node, ... */
    for (int i = 0; i < path->elements->count; i++) {
        ast_node *element = path->elements->items[i];

        if (element->type == AST_NODE_NODE_PATTERN) {
            cypher_node_pattern *node_pattern = (cypher_node_pattern*)element;
            int node_id = -1;

            /* Check if this variable already exists */
            if (node_pattern->variable && var_map) {
                node_id = get_variable_node_id(var_map, node_pattern->variable);
                if (node_id >= 0) {
                    CYPHER_DEBUG("Reusing existing node %d for variable '%s'", node_id, node_pattern->variable);
                }
            }

            /* Create new node if not found */
            if (node_id < 0) {
                node_id = cypher_schema_create_node(executor->schema_mgr);
                if (node_id < 0) {
                    set_result_error(result, "Failed to create node");
                    return -1;
                }

                result->nodes_created++;
                CYPHER_DEBUG("Created new node %d", node_id);

                /* Store variable mapping if present */
                if (node_pattern->variable && var_map) {
                    set_variable_node_id(var_map, node_pattern->variable, node_id);
                    CYPHER_DEBUG("Mapped variable '%s' to node %d", node_pattern->variable, node_id);
                }

                /* Handle node labels and properties for new nodes - supports multiple labels */
                if (has_labels(node_pattern)) {
                    for (int li = 0; li < node_pattern->labels->count; li++) {
                        const char *label = get_label_string(node_pattern->labels->items[li]);
                        if (label && cypher_schema_add_node_label(executor->schema_mgr, node_id, label) == 0) {
                            CYPHER_DEBUG("Added label '%s' to node %d", label, node_id);
                        }
                    }
                }

                /* Process node properties if present */
                if (node_pattern->properties && node_pattern->properties->type == AST_NODE_MAP) {
                    cypher_map *map = (cypher_map*)node_pattern->properties;
                    if (map->pairs) {
                        for (int j = 0; j < map->pairs->count; j++) {
                            cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                            if (pair->key && pair->value) {
                                /* Determine property type and value */
                                property_type prop_type = PROP_TYPE_TEXT;
                                const void *prop_value = NULL;

                                if (pair->value->type == AST_NODE_LITERAL) {
                                    cypher_literal *lit = (cypher_literal*)pair->value;
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
                                } else if (pair->value->type == AST_NODE_MAP || pair->value->type == AST_NODE_LIST) {
                                    /* Map or list literal - serialize to JSON and store as JSON type */
                                    char *json_str = serialize_ast_to_json(pair->value);
                                    if (json_str) {
                                        if (cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, PROP_TYPE_JSON, json_str) == 0) {
                                            result->properties_set++;
                                            CYPHER_DEBUG("Set JSON property '%s' on node %d", pair->key, node_id);
                                        }
                                        free(json_str);
                                    }
                                    continue;
                                } else if (pair->value->type == AST_NODE_PARAMETER && executor->params_json) {
                                    /* Handle parameter substitution */
                                    cypher_parameter *param = (cypher_parameter*)pair->value;
                                    property_value pv;
                                    property_value_init(&pv);
                                    static int64_t int_buf;
                                    static double real_buf;
                                    static int bool_buf;

                                    int rc = get_param_value(executor->params_json, param->name, &prop_type, &pv);
                                    if (rc == -2) {
                                        /* null parameter - skip */
                                        property_value_free(&pv);
                                        continue;
                                    } else if (rc == 0) {
                                        /* Set prop_value based on type returned */
                                        if (prop_type == PROP_TYPE_TEXT) {
                                            prop_value = pv.as_str;
                                        } else if (prop_type == PROP_TYPE_INTEGER) {
                                            int_buf = pv.as_int;
                                            prop_value = &int_buf;
                                        } else if (prop_type == PROP_TYPE_REAL) {
                                            real_buf = pv.as_real;
                                            prop_value = &real_buf;
                                        } else if (prop_type == PROP_TYPE_BOOLEAN) {
                                            bool_buf = pv.as_bool;
                                            prop_value = &bool_buf;
                                        } else if (prop_type == PROP_TYPE_JSON) {
                                            prop_value = pv.as_str;
                                        }
                                    } else {
                                        CYPHER_DEBUG("Parameter '%s' not found in params_json", param->name);
                                        property_value_free(&pv);
                                        continue;
                                    }
                                } else if (pair->value->type == AST_NODE_IDENTIFIER && g_foreach_ctx) {
                                    /* Check if this is a foreach variable reference */
                                    cypher_identifier *id = (cypher_identifier*)pair->value;
                                    foreach_binding *binding = get_foreach_binding(g_foreach_ctx, id->name);
                                    if (binding) {
                                        switch (binding->literal_type) {
                                            case LITERAL_STRING:
                                                prop_type = PROP_TYPE_TEXT;
                                                prop_value = binding->value.string;
                                                break;
                                            case LITERAL_INTEGER:
                                                prop_type = PROP_TYPE_INTEGER;
                                                prop_value = &binding->value.integer;
                                                break;
                                            case LITERAL_DECIMAL:
                                                prop_type = PROP_TYPE_REAL;
                                                prop_value = &binding->value.decimal;
                                                break;
                                            case LITERAL_BOOLEAN:
                                                prop_type = PROP_TYPE_BOOLEAN;
                                                prop_value = &binding->value.boolean;
                                                break;
                                            default:
                                                continue;
                                        }
                                    }
                                } else if (pair->value->type == AST_NODE_FUNCTION_CALL) {
                                    cypher_function_call *func = (cypher_function_call*)pair->value;
                                    property_value func_pv;
                                    property_value_init(&func_pv);
                                    int rc = evaluate_function_call_via_sqlite(executor, func, &prop_type, &func_pv);
                                    if (rc == -2) {
                                        property_value_free(&func_pv);
                                        continue;
                                    } else if (rc == 0) {
                                        static int64_t func_int_buf;
                                        static double func_real_buf;
                                        static int func_bool_buf;
                                        if (prop_type == PROP_TYPE_TEXT || prop_type == PROP_TYPE_JSON) {
                                            if (cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, prop_type, func_pv.as_str) == 0) {
                                                result->properties_set++;
                                            }
                                            property_value_free(&func_pv);
                                            continue;
                                        } else if (prop_type == PROP_TYPE_INTEGER) {
                                            func_int_buf = func_pv.as_int;
                                            prop_value = &func_int_buf;
                                        } else if (prop_type == PROP_TYPE_REAL) {
                                            func_real_buf = func_pv.as_real;
                                            prop_value = &func_real_buf;
                                        } else if (prop_type == PROP_TYPE_BOOLEAN) {
                                            func_bool_buf = func_pv.as_bool;
                                            prop_value = &func_bool_buf;
                                        }
                                    }
                                    property_value_free(&func_pv);
                                }

                                if (prop_value) {
                                    if (cypher_schema_set_node_property(executor->schema_mgr, node_id, pair->key, prop_type, prop_value) == 0) {
                                        result->properties_set++;
                                        CYPHER_DEBUG("Set property '%s' on node %d", pair->key, node_id);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            previous_node_id = node_id;

        } else if (element->type == AST_NODE_REL_PATTERN) {
            /* Create a relationship - need the next node */
            if (i + 1 >= path->elements->count) {
                set_result_error(result, "Incomplete relationship pattern");
                return -1;
            }

            ast_node *next_element = path->elements->items[i + 1];
            if (next_element->type != AST_NODE_NODE_PATTERN) {
                set_result_error(result, "Expected node after relationship");
                return -1;
            }

            /* Handle the target node (check for existing variable) */
            cypher_node_pattern *target_pattern = (cypher_node_pattern*)next_element;
            int target_node_id = -1;

            /* Check if target variable already exists */
            if (target_pattern->variable && var_map) {
                target_node_id = get_variable_node_id(var_map, target_pattern->variable);
                if (target_node_id >= 0) {
                    CYPHER_DEBUG("Reusing existing target node %d for variable '%s'", target_node_id, target_pattern->variable);
                }
            }

            /* Create target node if not found */
            if (target_node_id < 0) {
                target_node_id = cypher_schema_create_node(executor->schema_mgr);
                if (target_node_id < 0) {
                    set_result_error(result, "Failed to create target node");
                    return -1;
                }

                result->nodes_created++;
                CYPHER_DEBUG("Created new target node %d", target_node_id);

                /* Store target variable mapping if present */
                if (target_pattern->variable && var_map) {
                    set_variable_node_id(var_map, target_pattern->variable, target_node_id);
                    CYPHER_DEBUG("Mapped target variable '%s' to node %d", target_pattern->variable, target_node_id);
                }

                /* Handle target node labels and properties for new nodes - supports multiple labels */
                if (has_labels(target_pattern)) {
                    for (int li = 0; li < target_pattern->labels->count; li++) {
                        const char *label = get_label_string(target_pattern->labels->items[li]);
                        if (label && cypher_schema_add_node_label(executor->schema_mgr, target_node_id, label) == 0) {
                            CYPHER_DEBUG("Added label '%s' to target node %d", label, target_node_id);
                        }
                    }
                }

                /* Process target node properties if present */
                if (target_pattern->properties && target_pattern->properties->type == AST_NODE_MAP) {
                    cypher_map *map = (cypher_map*)target_pattern->properties;
                    if (map->pairs) {
                        for (int j = 0; j < map->pairs->count; j++) {
                            cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                            if (pair->key && pair->value) {
                                /* Determine property type and value */
                                property_type prop_type = PROP_TYPE_TEXT;
                                const void *prop_value = NULL;

                                if (pair->value->type == AST_NODE_LITERAL) {
                                    cypher_literal *lit = (cypher_literal*)pair->value;
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
                                } else if (pair->value->type == AST_NODE_MAP || pair->value->type == AST_NODE_LIST) {
                                    /* Map or list literal - serialize to JSON and store as JSON type */
                                    char *json_str = serialize_ast_to_json(pair->value);
                                    if (json_str) {
                                        if (cypher_schema_set_node_property(executor->schema_mgr, target_node_id, pair->key, PROP_TYPE_JSON, json_str) == 0) {
                                            result->properties_set++;
                                            CYPHER_DEBUG("Set JSON property '%s' on target node %d", pair->key, target_node_id);
                                        }
                                        free(json_str);
                                    }
                                    continue;
                                } else if (pair->value->type == AST_NODE_PARAMETER && executor->params_json) {
                                    /* Handle parameter substitution */
                                    cypher_parameter *param = (cypher_parameter*)pair->value;
                                    property_value pv2;
                                    property_value_init(&pv2);
                                    static int64_t int_buf2;
                                    static double real_buf2;
                                    static int bool_buf2;

                                    int rc = get_param_value(executor->params_json, param->name, &prop_type, &pv2);
                                    if (rc == -2) {
                                        property_value_free(&pv2);
                                        continue;
                                    } else if (rc == 0) {
                                        if (prop_type == PROP_TYPE_TEXT) {
                                            prop_value = pv2.as_str;
                                        } else if (prop_type == PROP_TYPE_INTEGER) {
                                            int_buf2 = pv2.as_int;
                                            prop_value = &int_buf2;
                                        } else if (prop_type == PROP_TYPE_REAL) {
                                            real_buf2 = pv2.as_real;
                                            prop_value = &real_buf2;
                                        } else if (prop_type == PROP_TYPE_BOOLEAN) {
                                            bool_buf2 = pv2.as_bool;
                                            prop_value = &bool_buf2;
                                        } else if (prop_type == PROP_TYPE_JSON) {
                                            prop_value = pv2.as_str;
                                        }
                                    } else {
                                        CYPHER_DEBUG("Parameter '%s' not found in params_json", param->name);
                                        property_value_free(&pv2);
                                        continue;
                                    }
                                } else if (pair->value->type == AST_NODE_FUNCTION_CALL) {
                                    cypher_function_call *func = (cypher_function_call*)pair->value;
                                    property_value func_pv2;
                                    property_value_init(&func_pv2);
                                    int rc = evaluate_function_call_via_sqlite(executor, func, &prop_type, &func_pv2);
                                    if (rc == -2) {
                                        property_value_free(&func_pv2);
                                        continue;
                                    } else if (rc == 0) {
                                        static int64_t func_int_buf2;
                                        static double func_real_buf2;
                                        static int func_bool_buf2;
                                        if (prop_type == PROP_TYPE_TEXT || prop_type == PROP_TYPE_JSON) {
                                            if (cypher_schema_set_node_property(executor->schema_mgr, target_node_id, pair->key, prop_type, func_pv2.as_str) == 0) {
                                                result->properties_set++;
                                            }
                                            property_value_free(&func_pv2);
                                            continue;
                                        } else if (prop_type == PROP_TYPE_INTEGER) {
                                            func_int_buf2 = func_pv2.as_int;
                                            prop_value = &func_int_buf2;
                                        } else if (prop_type == PROP_TYPE_REAL) {
                                            func_real_buf2 = func_pv2.as_real;
                                            prop_value = &func_real_buf2;
                                        } else if (prop_type == PROP_TYPE_BOOLEAN) {
                                            func_bool_buf2 = func_pv2.as_bool;
                                            prop_value = &func_bool_buf2;
                                        }
                                    }
                                    property_value_free(&func_pv2);
                                }

                                if (prop_value) {
                                    if (cypher_schema_set_node_property(executor->schema_mgr, target_node_id, pair->key, prop_type, prop_value) == 0) {
                                        result->properties_set++;
                                        CYPHER_DEBUG("Set property '%s' on target node %d", pair->key, target_node_id);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /* Create the relationship */
            cypher_rel_pattern *rel_pattern = (cypher_rel_pattern*)element;
            const char *rel_type = rel_pattern->type ? rel_pattern->type : "RELATED";

            int source_id, target_id;
            if (rel_pattern->left_arrow && !rel_pattern->right_arrow) {
                /* <-[:TYPE]- (reversed) */
                source_id = target_node_id;
                target_id = previous_node_id;
            } else {
                /* -[:TYPE]-> or -[:TYPE]- (forward or undirected, treat as forward) */
                source_id = previous_node_id;
                target_id = target_node_id;
            }

            int edge_id = cypher_schema_create_edge(executor->schema_mgr, source_id, target_id, rel_type);
            if (edge_id < 0) {
                set_result_error(result, "Failed to create relationship");
                return -1;
            }

            /* Process relationship properties if present */
            if (rel_pattern->properties && rel_pattern->properties->type == AST_NODE_MAP) {
                cypher_map *map = (cypher_map*)rel_pattern->properties;
                if (map->pairs) {
                    for (int j = 0; j < map->pairs->count; j++) {
                        cypher_map_pair *pair = (cypher_map_pair*)map->pairs->items[j];
                        if (pair->key && pair->value) {
                            /* Determine property type and value */
                            property_type prop_type = PROP_TYPE_TEXT;
                            const void *prop_value = NULL;

                            if (pair->value->type == AST_NODE_MAP || pair->value->type == AST_NODE_LIST) {
                                /* Map or list literal - serialize to JSON and store as JSON type */
                                char *json_str = serialize_ast_to_json(pair->value);
                                if (json_str) {
                                    if (cypher_schema_set_edge_property(executor->schema_mgr, edge_id, pair->key, PROP_TYPE_JSON, json_str) == 0) {
                                        result->properties_set++;
                                        CYPHER_DEBUG("Set JSON edge property '%s' on edge %d", pair->key, edge_id);
                                    }
                                    free(json_str);
                                }
                            } else if (pair->value->type == AST_NODE_LITERAL) {
                                cypher_literal *lit = (cypher_literal*)pair->value;
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
                                        continue; /* Skip unsupported types */
                                }
                            } else if (pair->value->type == AST_NODE_PARAMETER && executor->params_json) {
                                cypher_parameter *param = (cypher_parameter*)pair->value;
                                property_value pv;
                                property_value_init(&pv);
                                static int64_t rel_int_buf;
                                static double rel_real_buf;
                                static int rel_bool_buf;

                                int rc = get_param_value(executor->params_json, param->name, &prop_type, &pv);
                                if (rc == -2) {
                                    property_value_free(&pv);
                                    continue;
                                } else if (rc == 0) {
                                    if (prop_type == PROP_TYPE_TEXT) {
                                        prop_value = pv.as_str;
                                    } else if (prop_type == PROP_TYPE_INTEGER) {
                                        rel_int_buf = pv.as_int;
                                        prop_value = &rel_int_buf;
                                    } else if (prop_type == PROP_TYPE_REAL) {
                                        rel_real_buf = pv.as_real;
                                        prop_value = &rel_real_buf;
                                    } else if (prop_type == PROP_TYPE_BOOLEAN) {
                                        rel_bool_buf = pv.as_bool;
                                        prop_value = &rel_bool_buf;
                                    } else if (prop_type == PROP_TYPE_JSON) {
                                        prop_value = pv.as_str;
                                    }
                                } else {
                                    CYPHER_DEBUG("Parameter '%s' not found in params_json (rel)", param->name);
                                    property_value_free(&pv);
                                    continue;
                                }
                            }

                            if (prop_value) {
                                if (cypher_schema_set_edge_property(executor->schema_mgr, edge_id, pair->key, prop_type, prop_value) < 0) {
                                    set_result_error(result, "Failed to set edge property");
                                    return -1;
                                }
                                result->properties_set++;
                                CYPHER_DEBUG("Set edge property: %s", pair->key);
                            }
                        }
                    }
                }
            }

            result->relationships_created++;
            previous_node_id = target_node_id;

            CYPHER_DEBUG("Created relationship %d: %d -[:%s]-> %d",
                        edge_id, source_id, rel_type, target_id);

            /* Skip the next element since we already processed it */
            i++;
        }
    }

    return 0;
}

/* Execute CREATE clause with full relationship support */
int execute_create_clause(cypher_executor *executor, cypher_create *create, cypher_result *result)
{
    if (!executor || !create || !result) {
        return -1;
    }

    if (!create->pattern) {
        set_result_error(result, "No pattern in CREATE clause");
        return -1;
    }

    CYPHER_DEBUG("Executing CREATE clause with %d patterns", create->pattern->count);

    /* Create variable map to track node variables across patterns */
    variable_map *var_map = create_variable_map();
    if (!var_map) {
        set_result_error(result, "Failed to create variable map");
        return -1;
    }

    /* Process each path pattern in the CREATE clause */
    for (int i = 0; i < create->pattern->count; i++) {
        ast_node *pattern = create->pattern->items[i];

        if (pattern->type == AST_NODE_PATH) {
            if (execute_path_pattern_with_variables(executor, (cypher_path*)pattern, result, var_map) < 0) {
                free_variable_map(var_map);
                return -1; /* Error already set */
            }
        } else {
            CYPHER_DEBUG("Unexpected pattern type in CREATE: %d", pattern->type);
        }
    }

    /* Clean up variable map */
    free_variable_map(var_map);

    return 0;
}

/* Execute CREATE clause, returning the variable map with created node IDs.
 * Caller is responsible for freeing the returned variable_map. */
int execute_create_clause_with_varmap(cypher_executor *executor, cypher_create *create,
                                      cypher_result *result, variable_map **out_var_map)
{
    if (!executor || !create || !result || !out_var_map) {
        return -1;
    }

    if (!create->pattern) {
        set_result_error(result, "No pattern in CREATE clause");
        return -1;
    }

    CYPHER_DEBUG("Executing CREATE clause (with varmap) with %d patterns", create->pattern->count);

    variable_map *var_map = create_variable_map();
    if (!var_map) {
        set_result_error(result, "Failed to create variable map");
        return -1;
    }

    for (int i = 0; i < create->pattern->count; i++) {
        ast_node *pattern = create->pattern->items[i];
        if (pattern->type == AST_NODE_PATH) {
            if (execute_path_pattern_with_variables(executor, (cypher_path*)pattern, result, var_map) < 0) {
                free_variable_map(var_map);
                return -1;
            }
        }
    }

    *out_var_map = var_map;
    return 0;
}
