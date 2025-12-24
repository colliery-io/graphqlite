/*
 * Cypher Execution Engine
 * Orchestrates parser, transformer, and schema manager for end-to-end query execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "executor/cypher_executor.h"
#include "executor/graph_algorithms.h"
#include "parser/cypher_debug.h"

/* Helper to get label string from a label literal node */
static const char* get_label_string(ast_node *label_node)
{
    if (!label_node || label_node->type != AST_NODE_LITERAL) return NULL;
    cypher_literal *lit = (cypher_literal*)label_node;
    if (lit->literal_type != LITERAL_STRING) return NULL;
    return lit->value.string;
}

/* Helper to check if a node pattern has any labels */
static bool has_labels(cypher_node_pattern *node)
{
    return node && node->labels && node->labels->count > 0;
}

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

/* Forward declarations */
static int execute_match_return_query(cypher_executor *executor, cypher_match *match, cypher_return *return_clause, cypher_result *result);
static int execute_match_create_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_result *result);
static int execute_match_create_return_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_return *return_clause, cypher_result *result);
static int execute_match_set_query(cypher_executor *executor, cypher_match *match, cypher_set *set, cypher_result *result);
static int execute_match_delete_query(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause, cypher_result *result);
static int delete_edge_by_id(cypher_executor *executor, int64_t edge_id);
static int delete_node_by_id(cypher_executor *executor, int64_t node_id, bool detach);
static int execute_set_clause(cypher_executor *executor, cypher_set *set, cypher_result *result);
static int build_query_results(cypher_executor *executor, sqlite3_stmt *stmt, cypher_return *return_clause, cypher_result *result, cypher_transform_context *ctx);
static agtype_value* create_property_agtype_value(const char* value);
static agtype_value* build_path_from_ids(cypher_executor *executor, cypher_transform_context *ctx, const char *path_name, const char *json_ids);

/* Create empty result */
static cypher_result* create_empty_result(void)
{
    cypher_result *result = calloc(1, sizeof(cypher_result));
    if (!result) {
        return NULL;
    }
    
    result->success = false;
    result->error_message = NULL;
    result->row_count = 0;
    result->column_count = 0;
    result->column_names = NULL;
    result->data = NULL;
    result->data_types = NULL;
    result->agtype_data = NULL;
    result->use_agtype = false;
    result->nodes_created = 0;
    result->nodes_deleted = 0;
    result->relationships_created = 0;
    result->relationships_deleted = 0;
    result->properties_set = 0;
    
    return result;
}

/* Set error message in result */
static void set_result_error(cypher_result *result, const char *error_msg)
{
    if (!result || !error_msg) {
        return;
    }
    
    result->success = false;
    result->error_message = strdup(error_msg);
}

/* Simple variable to node ID mapping structure */
typedef struct {
    char *variable;
    int node_id;
} variable_mapping;

typedef struct {
    variable_mapping *mappings;
    int count;
    int capacity;
} variable_map;

/* Forward declaration that needs variable_map type */
static int execute_set_operations(cypher_executor *executor, cypher_set *set, variable_map *var_map, cypher_result *result);

/* Create variable map */
static variable_map* create_variable_map(void)
{
    variable_map *map = calloc(1, sizeof(variable_map));
    if (!map) return NULL;
    
    map->capacity = 16;
    map->mappings = calloc(map->capacity, sizeof(variable_mapping));
    if (!map->mappings) {
        free(map);
        return NULL;
    }
    
    return map;
}

/* Free variable map */
static void free_variable_map(variable_map *map)
{
    if (!map) return;
    
    for (int i = 0; i < map->count; i++) {
        free(map->mappings[i].variable);
    }
    free(map->mappings);
    free(map);
}

/* Get node ID for variable, return -1 if not found */
static int get_variable_node_id(variable_map *map, const char *variable)
{
    if (!map || !variable) return -1;
    
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            return map->mappings[i].node_id;
        }
    }
    return -1;
}

/* Set variable to node ID mapping */
static int set_variable_node_id(variable_map *map, const char *variable, int node_id)
{
    if (!map || !variable) return -1;
    
    /* Check if variable already exists */
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->mappings[i].variable, variable) == 0) {
            map->mappings[i].node_id = node_id;
            return 0;
        }
    }
    
    /* Add new mapping */
    if (map->count >= map->capacity) {
        /* Expand capacity */
        map->capacity *= 2;
        map->mappings = realloc(map->mappings, map->capacity * sizeof(variable_mapping));
        if (!map->mappings) return -1;
    }
    
    map->mappings[map->count].variable = strdup(variable);
    map->mappings[map->count].node_id = node_id;
    map->count++;
    
    return 0;
}

/* Helper function to execute a single path pattern with variable tracking */
static int execute_path_pattern_with_variables(cypher_executor *executor, cypher_path *path, 
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
                                    default:
                                        continue; /* Skip unsupported types */
                                }
                                
                                /* Set the property on the edge */
                                if (cypher_schema_set_edge_property(executor->schema_mgr, edge_id, pair->key, prop_type, prop_value) < 0) {
                                    set_result_error(result, "Failed to set edge property");
                                    return -1;
                                }
                                
                                result->properties_set++;
                                CYPHER_DEBUG("Added edge property: %s", pair->key);
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
static int execute_create_clause(cypher_executor *executor, cypher_create *create, cypher_result *result)
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

/* Find a node by label and properties, returns node_id or -1 if not found */
static int find_node_by_pattern(cypher_executor *executor, cypher_node_pattern *node_pattern)
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

/* Execute SET items from a list (used for ON CREATE/ON MATCH) */
static int execute_set_items(cypher_executor *executor, ast_list *items, variable_map *var_map, cypher_result *result)
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

/* Execute MERGE clause */
static int execute_merge_clause(cypher_executor *executor, cypher_merge *merge, cypher_result *result)
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

                /* For now, just skip relationship handling - focus on node MERGE first */
                /* TODO: Implement relationship MERGE */
                CYPHER_DEBUG("Relationship MERGE not yet fully implemented");
            }
        }
    }

    CYPHER_DEBUG("MERGE complete: %d nodes matched, %d nodes created", nodes_matched, nodes_created_in_merge);

    free_variable_map(var_map);
    return 0;
}

/* Execute MATCH clause by running generated SQL */
static int execute_match_clause(cypher_executor *executor, cypher_match *match, cypher_result *result)
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
static int execute_match_return_query(cypher_executor *executor, cypher_match *match, cypher_return *return_clause, cypher_result *result)
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

    /*
     * Check if we should enable sql_builder for optimized property JOINs.
     * Only enable for simple node-only patterns without relationships,
     * as the MATCH transformation for relationships writes directly to sql_buffer
     * and doesn't work well with the sql_builder pattern.
     */
    bool has_relationships = false;
    for (int i = 0; i < match->pattern->count; i++) {
        ast_node *pattern = match->pattern->items[i];
        if (pattern->type == AST_NODE_PATH) {
            cypher_path *path = (cypher_path *)pattern;
            if (path->elements && path->elements->count > 1) {
                has_relationships = true;
                break;
            }
        }
    }

    if (!has_relationships) {
        if (init_sql_builder(ctx) < 0) {
            set_result_error(result, "Failed to initialize SQL builder");
            cypher_transform_free_context(ctx);
            return -1;
        }
    }

    /* Transform MATCH clause to generate FROM/WHERE */
    if (transform_match_clause(ctx, match) < 0) {
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }

    /* Finalize SQL generation before RETURN (assembles FROM + JOINs + WHERE) */
    /* Only needed if sql_builder was initialized */
    if (!has_relationships) {
        if (finalize_sql_generation(ctx) < 0) {
            set_result_error(result, "Failed to finalize SQL generation");
            cypher_transform_free_context(ctx);
            return -1;
        }
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
static int build_query_results(cypher_executor *executor, sqlite3_stmt *stmt, cypher_return *return_clause, cypher_result *result, cypher_transform_context *ctx)
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
        
        for (int col = 0; col < column_count; col++) {
            const char *value = (const char*)sqlite3_column_text(stmt, col);
            if (value) {
                result->data[current_row][col] = strdup(value);
                
                /* Create agtype value for graph entities */
                if (has_agtype_values) {
                    cypher_return_item *item = (cypher_return_item*)return_clause->items->items[col];
                    if (item->expr && item->expr->type == AST_NODE_IDENTIFIER) {
                        cypher_identifier *ident = (cypher_identifier*)item->expr;
                        
                        /* Check if this is a path variable */
                        if (ctx && is_path_variable(ctx, ident->name)) {
                            CYPHER_DEBUG("Executor: Processing path variable '%s' with value: %s", ident->name, value);
                            /* Parse the JSON array of element IDs and build path object */
                            result->agtype_data[current_row][col] = build_path_from_ids(executor, ctx, ident->name, value);
                        } else if (ctx && is_edge_variable(ctx, ident->name)) {
                            /* Parse the edge ID from the SQL result */
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
                        } else {
                            /* This is a node variable */
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
                    } else if (item->expr && item->expr->type == AST_NODE_PROPERTY) {
                        /* Property access - try to detect the original data type */
                        result->agtype_data[current_row][col] = create_property_agtype_value(value);
                    } else {
                        /* For other non-entity columns, create string agtype values */
                        result->agtype_data[current_row][col] = agtype_value_create_string(value);
                    }
                }
            } else {
                result->data[current_row][col] = strdup("NULL");
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
static agtype_value* create_property_agtype_value(const char* value)
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
static agtype_value* build_path_from_ids(cypher_executor *executor, cypher_transform_context *ctx, const char *path_name, const char *json_ids)
{
    CYPHER_DEBUG("build_path_from_ids called: path_name='%s', json_ids='%s'", 
                 path_name ? path_name : "NULL", json_ids ? json_ids : "NULL");
    
    if (!executor || !ctx || !path_name || !json_ids) {
        CYPHER_DEBUG("build_path_from_ids: Missing required parameters");
        return agtype_value_create_null();
    }
    
    /* Get path variable metadata */
    path_variable *path_var = get_path_variable(ctx, path_name);
    if (!path_var || !path_var->elements) {
        CYPHER_DEBUG("build_path_from_ids: Failed to get path variable metadata for '%s'", path_name);
        return agtype_value_create_null();
    }
    CYPHER_DEBUG("build_path_from_ids: Found path metadata with %d elements", path_var->elements->count);
    
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
    
    if (id_count != path_var->elements->count) {
        /* Mismatch between expected elements and actual IDs */
        CYPHER_DEBUG("build_path_from_ids: Mismatch - expected %d elements, got %d IDs", 
                     path_var->elements->count, id_count);
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
                ast_node *element = path_var->elements->items[elem_index];
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
        ast_node *element = path_var->elements->items[elem_index];
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
static int execute_match_create_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_result *result)
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
        for (int i = 0; i < ctx->variable_count; i++) {
            if (ctx->variables[i].type == VAR_TYPE_NODE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", ctx->variables[i].table_alias, ctx->variables[i].name);
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
        for (int i = 0; i < ctx->variable_count; i++) {
            if (ctx->variables[i].type == VAR_TYPE_NODE) {
                int64_t node_id = sqlite3_column_int64(stmt, col);
                set_variable_node_id(var_map, ctx->variables[i].name, (int)node_id);
                CYPHER_DEBUG("Bound variable '%s' to existing node %lld", ctx->variables[i].name, (long long)node_id);
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
static int execute_match_create_return_query(cypher_executor *executor, cypher_match *match, cypher_create *create, cypher_return *return_clause, cypher_result *result)
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

/* Execute MATCH+SET query combination */
static int execute_match_set_query(cypher_executor *executor, cypher_match *match, cypher_set *set, cypher_result *result)
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
        printf("DEBUG - Transform MATCH failed: %s\n", ctx->error_message ? ctx->error_message : "No error message");
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }
    
    if (ctx->has_error) {
        printf("DEBUG - Transform context has error: %s\n", ctx->error_message ? ctx->error_message : "No error message");
    }
    
    /* Add SELECT to get matched node IDs */
    char *select_pos = strstr(ctx->sql_buffer, "SELECT *");
    if (select_pos) {
        /* Replace SELECT * with specific node ID selection */
        char *after_star = select_pos + strlen("SELECT *");
        char *temp = strdup(after_star);
        
        ctx->sql_size = select_pos + strlen("SELECT ") - ctx->sql_buffer;
        ctx->sql_buffer[ctx->sql_size] = '\0';
        
        /* Select all node variables found in the MATCH */
        bool first = true;
        for (int i = 0; i < ctx->variable_count; i++) {
            if (ctx->variables[i].type == VAR_TYPE_NODE) {
                if (!first) append_sql(ctx, ", ");
                append_sql(ctx, "%s.id AS %s_id", ctx->variables[i].table_alias, ctx->variables[i].name);
                first = false;
            }
        }
        
        append_sql(ctx, " %s", temp);
        free(temp);
    }
    
    CYPHER_DEBUG("Generated MATCH SQL: %s", ctx->sql_buffer);
    printf("\nDEBUG - Generated MATCH SQL for SET (length %zu/%zu): %s\n", ctx->sql_size, ctx->sql_capacity, ctx->sql_buffer);
    
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
        
        /* Bind variables to matched node IDs */
        int col = 0;
        for (int i = 0; i < ctx->variable_count; i++) {
            if (ctx->variables[i].type == VAR_TYPE_NODE) {
                int64_t node_id = sqlite3_column_int64(stmt, col);
                set_variable_node_id(var_map, ctx->variables[i].name, (int)node_id);
                printf("DEBUG - MATCH returned node_id=%lld for variable '%s'\n", (long long)node_id, ctx->variables[i].name);
                CYPHER_DEBUG("Bound variable '%s' to node %lld", ctx->variables[i].name, (long long)node_id);
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
static int execute_set_clause(cypher_executor *executor, cypher_set *set, cypher_result *result)
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
static int execute_set_operations(cypher_executor *executor, cypher_set *set, variable_map *var_map, cypher_result *result)
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
        
        /* Get the node ID for this variable */
        int node_id = get_variable_node_id(var_map, var_id->name);
        if (node_id < 0) {
            char error[256];
            snprintf(error, sizeof(error), "Unbound variable in SET: %s", var_id->name);
            set_result_error(result, error);
            return -1;
        }
        
        /* Evaluate the value expression */
        if (!item->expr || item->expr->type != AST_NODE_LITERAL) {
            set_result_error(result, "SET value must be a literal");
            return -1;
        }
        
        cypher_literal *lit = (cypher_literal*)item->expr;
        
        /* Determine property type and value */
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
            case LITERAL_NULL:
                /* Skip null properties for now */
                continue;
        }
        
        /* Set the property on the node */
        if (prop_value) {
            if (cypher_schema_set_node_property(executor->schema_mgr, node_id, prop->property_name, prop_type, prop_value) == 0) {
                result->properties_set++;
                CYPHER_DEBUG("Set property '%s' = value on node %d", prop->property_name, node_id);
            } else {
                char error[512];
                snprintf(error, sizeof(error), "Failed to set property '%s' on node %d", prop->property_name, node_id);
                set_result_error(result, error);
                return -1;
            }
        }
    }
    
    return 0;
}

/* Execute MATCH+DELETE query combination */
static int execute_match_delete_query(cypher_executor *executor, cypher_match *match, cypher_delete *delete_clause, cypher_result *result)
{
    if (!executor || !match || !delete_clause || !result) {
        return -1;
    }
    
    CYPHER_DEBUG("Executing MATCH+DELETE query");
    
    /* Step 1: Execute MATCH to find entities to delete */
    cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
    if (!ctx) {
        set_result_error(result, "Failed to create transform context");
        return -1;
    }
    
    /* Transform MATCH clause to build SELECT query */
    if (transform_match_clause(ctx, match) < 0) {
        set_result_error(result, "Failed to transform MATCH clause");
        cypher_transform_free_context(ctx);
        return -1;
    }
    
    /* Following AGE's approach: execute MATCH first to get a result set, 
     * then iterate through each row and delete the specified entities */
    
    /* Create a synthetic RETURN clause for the variables to delete */
    cypher_return *synthetic_return = calloc(1, sizeof(cypher_return));
    if (!synthetic_return) {
        set_result_error(result, "Failed to allocate memory for DELETE processing");
        cypher_transform_free_context(ctx);
        return -1;
    }
    
    synthetic_return->base.type = AST_NODE_RETURN;
    synthetic_return->items = ast_list_create();
    synthetic_return->distinct = false;
    synthetic_return->order_by = NULL;
    synthetic_return->limit = NULL;
    synthetic_return->skip = NULL;
    
    /* Add RETURN items for each variable to delete */
    for (int i = 0; i < delete_clause->items->count; i++) {
        cypher_delete_item *del_item = (cypher_delete_item*)delete_clause->items->items[i];
        if (del_item && del_item->variable) {
            /* Create identifier for the variable */
            cypher_identifier *id = calloc(1, sizeof(cypher_identifier));
            id->base.type = AST_NODE_IDENTIFIER;
            id->name = strdup(del_item->variable);
            
            /* Create return item */
            cypher_return_item *ret_item = calloc(1, sizeof(cypher_return_item));
            ret_item->base.type = AST_NODE_RETURN_ITEM;
            ret_item->expr = (ast_node*)id;
            ret_item->alias = NULL;
            
            ast_list_append(synthetic_return->items, (ast_node*)ret_item);
        }
    }
    
    /* Execute the MATCH query to get entities */
    cypher_result *match_result = create_empty_result();
    if (execute_match_return_query(executor, match, synthetic_return, match_result) < 0) {
        set_result_error(result, "Failed to execute MATCH for DELETE");
        cypher_transform_free_context(ctx);
        cypher_result_free(match_result);
        /* Clean up synthetic return - ast_list_free handles freeing items */
        if (synthetic_return->items) {
            ast_list_free(synthetic_return->items);
        }
        free(synthetic_return);
        return -1;
    }
    
    /* Process each entity found by the MATCH and delete it */
    int deleted_nodes = 0, deleted_edges = 0;
    
    /* Following AGE's process_delete_list pattern */
    for (int i = 0; i < delete_clause->items->count; i++) {
        cypher_delete_item *item = (cypher_delete_item*)delete_clause->items->items[i];
        if (!item || !item->variable) continue;
        
        /* Check if this variable is an edge or node */
        /* bool is_edge = is_edge_variable(ctx, item->variable); -- not needed, we check entity type */
        
        /* For each variable to delete, we need to find its value in the MATCH results */
        /* AGE uses entity_position but we'll find by variable name */
        for (int row = 0; row < match_result->row_count; row++) {
            for (int col = 0; col < match_result->column_count; col++) {
                if (match_result->column_names[col] && 
                    strcmp(match_result->column_names[col], item->variable) == 0) {
                    
                    /* Found the variable's column - get the entity */
                    if (match_result->agtype_data && match_result->agtype_data[row][col]) {
                        agtype_value *entity = match_result->agtype_data[row][col];
                        
                        /* Extract entity following AGE's pattern */
                        
                        if (entity->type == AGTV_VERTEX) {
                            /* For vertex, use the entity structure */
                            int64_t entity_id = entity->val.entity.id;
                            
                            CYPHER_DEBUG("Deleting node '%s' with ID %lld", item->variable, entity_id);
                            
                            int delete_result = delete_node_by_id(executor, entity_id, delete_clause->detach);
                            if (delete_result == 0) {
                                deleted_nodes++;
                            } else {
                                /* Failed to delete node - likely due to constraint violation */
                                set_result_error(result, "Cannot delete node - it still has relationships");
                                cypher_result_free(match_result);
                                cypher_transform_free_context(ctx);
                                
                                /* Clean up synthetic return - ast_list_free handles freeing items */
                                if (synthetic_return->items) {
                                    ast_list_free(synthetic_return->items);
                                }
                                free(synthetic_return);

                                return -1;
                            }
                        } else if (entity->type == AGTV_EDGE) {
                            /* For edge, use the edge structure */
                            int64_t entity_id = entity->val.edge.id;
                            
                            CYPHER_DEBUG("Deleting edge '%s' with ID %lld", item->variable, entity_id);
                            
                            if (delete_edge_by_id(executor, entity_id) == 0) {
                                deleted_edges++;
                            }
                        }
                    }
                }
            }
        }
    }
    
    cypher_result_free(match_result);
    cypher_transform_free_context(ctx);
    
    /* Clean up synthetic return - ast_list_free handles freeing items */
    if (synthetic_return->items) {
        ast_list_free(synthetic_return->items);
    }
    free(synthetic_return);

    /* Set result with deletion counts */
    result->success = true;
    result->nodes_deleted = deleted_nodes;
    result->relationships_deleted = deleted_edges;
    
    return 0;
}

/* Delete an edge by ID */
static int delete_edge_by_id(cypher_executor *executor, int64_t edge_id)
{
    if (!executor || !executor->db) {
        return -1;
    }
    
    CYPHER_DEBUG("Deleting edge with ID %lld", edge_id);
    
    /* Delete edge properties first */
    const char *prop_tables[] = {
        "edge_props_text", "edge_props_int", "edge_props_real", "edge_props_bool"
    };
    
    char sql[256];
    for (int i = 0; i < 4; i++) {
        snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE edge_id = %lld", prop_tables[i], edge_id);
        char *err_msg = NULL;
        int rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("Warning: Failed to delete from %s: %s", prop_tables[i], err_msg ? err_msg : "unknown error");
            if (err_msg) sqlite3_free(err_msg);
        }
    }
    
    /* Delete the edge itself */
    snprintf(sql, sizeof(sql), "DELETE FROM edges WHERE id = %lld", edge_id);
    char *err_msg = NULL;
    int rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to delete edge: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return -1;
    }
    
    return 0;
}

/* Delete a node by ID */
static int delete_node_by_id(cypher_executor *executor, int64_t node_id, bool detach)
{
    if (!executor || !executor->db) {
        return -1;
    }
    
    CYPHER_DEBUG("Deleting node with ID %lld (detach: %s)", node_id, detach ? "true" : "false");
    
    if (detach) {
        /* DETACH DELETE: First delete all connected edges */
        char delete_edges_sql[256];
        snprintf(delete_edges_sql, sizeof(delete_edges_sql), 
                 "DELETE FROM edges WHERE source_id = %lld OR target_id = %lld", node_id, node_id);
        
        char *err_msg = NULL;
        int rc = sqlite3_exec(executor->db, delete_edges_sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("Failed to delete connected edges for node %lld: %s", node_id, err_msg ? err_msg : "unknown error");
            if (err_msg) sqlite3_free(err_msg);
            return -1;
        }
        CYPHER_DEBUG("Deleted all connected edges for node %lld", node_id);
    } else {
        /* Regular DELETE: Check for connected edges (constraint enforcement) */
        char check_sql[256];
        snprintf(check_sql, sizeof(check_sql), "SELECT COUNT(*) FROM edges WHERE source_id = %lld OR target_id = %lld", node_id, node_id);
        
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(executor->db, check_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int edge_count = sqlite3_column_int(stmt, 0);
                if (edge_count > 0) {
                    sqlite3_finalize(stmt);
                    CYPHER_DEBUG("Cannot delete node with ID %lld: has %d connected edges", node_id, edge_count);
                    return -1; /* Node has connected edges */
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    
    /* Delete node properties first */
    const char *prop_tables[] = {
        "node_props_text", "node_props_int", "node_props_real", "node_props_bool"
    };
    
    char sql[256];
    int rc;
    for (int i = 0; i < 4; i++) {
        snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE node_id = %lld", prop_tables[i], node_id);
        char *err_msg = NULL;
        rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            CYPHER_DEBUG("Warning: Failed to delete from %s: %s", prop_tables[i], err_msg ? err_msg : "unknown error");
            if (err_msg) sqlite3_free(err_msg);
        }
    }
    
    /* Delete node labels */
    snprintf(sql, sizeof(sql), "DELETE FROM node_labels WHERE node_id = %lld", node_id);
    char *err_msg = NULL;
    rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Warning: Failed to delete node labels: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
    }
    
    /* Delete the node itself */
    snprintf(sql, sizeof(sql), "DELETE FROM nodes WHERE id = %lld", node_id);
    err_msg = NULL;
    rc = sqlite3_exec(executor->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        CYPHER_DEBUG("Failed to delete node: %s", err_msg ? err_msg : "unknown error");
        if (err_msg) sqlite3_free(err_msg);
        return -1;
    }
    
    return 0;
}

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
                    /* Analyze query structure to determine execution strategy */
                    cypher_match *match_clause = NULL;
                    cypher_return *return_clause = NULL;
                    cypher_create *create_clause = NULL;
                    cypher_merge *merge_clause = NULL;
                    cypher_set *set_clause = NULL;
                    cypher_delete *delete_clause = NULL;
                    cypher_with *with_clause = NULL;
                    cypher_unwind *unwind_clause = NULL;

                    /* Scan for clause types */
                    for (int i = 0; i < query->clauses->count; i++) {
                        ast_node *clause = query->clauses->items[i];
                        switch (clause->type) {
                            case AST_NODE_MATCH:
                                match_clause = (cypher_match*)clause;
                                break;
                            case AST_NODE_RETURN:
                                return_clause = (cypher_return*)clause;
                                break;
                            case AST_NODE_CREATE:
                                create_clause = (cypher_create*)clause;
                                break;
                            case AST_NODE_MERGE:
                                merge_clause = (cypher_merge*)clause;
                                break;
                            case AST_NODE_SET:
                                set_clause = (cypher_set*)clause;
                                break;
                            case AST_NODE_DELETE:
                                delete_clause = (cypher_delete*)clause;
                                break;
                            case AST_NODE_WITH:
                                with_clause = (cypher_with*)clause;
                                break;
                            case AST_NODE_UNWIND:
                                unwind_clause = (cypher_unwind*)clause;
                                break;
                            default:
                                CYPHER_DEBUG("Found clause type: %d", clause->type);
                                break;
                        }
                    }
                    
                    /* Handle EXPLAIN - return generated SQL instead of executing */
                    if (query->explain) {
                        CYPHER_DEBUG("EXPLAIN mode - returning generated SQL");
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

                        /* Return the SQL as a single-row, single-column result */
                        result->column_count = 1;
                        result->row_count = 1;
                        result->data = malloc(sizeof(char**));
                        result->data[0] = malloc(sizeof(char*));
                        result->data[0][0] = strdup(ctx->sql_buffer ? ctx->sql_buffer : "");
                        result->success = true;

                        cypher_transform_free_context(ctx);
                        return result;
                    }

                    /* Execute based on query pattern */
                    if (unwind_clause && return_clause) {
                        /* UNWIND ... RETURN pattern - use generic transform */
                        CYPHER_DEBUG("Executing UNWIND+RETURN query");
                        cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
                        if (!ctx) {
                            set_result_error(result, "Failed to create transform context");
                            return result;
                        }

                        cypher_query_result *transform_result = cypher_transform_query(ctx, query);
                        if (!transform_result) {
                            set_result_error(result, "Failed to transform query");
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        if (transform_result->has_error) {
                            set_result_error(result, transform_result->error_message ? transform_result->error_message : "Transform error");
                            free(transform_result);
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        /* Execute the prepared statement */
                        if (transform_result->stmt) {
                            result->data = NULL;
                            result->row_count = 0;
                            result->column_count = sqlite3_column_count(transform_result->stmt);

                            /* Collect results */
                            while (sqlite3_step(transform_result->stmt) == SQLITE_ROW) {
                                /* Allocate/resize data array */
                                result->data = realloc(result->data, (result->row_count + 1) * sizeof(char**));
                                result->data[result->row_count] = calloc(result->column_count, sizeof(char*));

                                for (int c = 0; c < result->column_count; c++) {
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
                    } else if (with_clause && match_clause && return_clause) {
                        /* MATCH ... WITH ... RETURN pattern - use generic transform */
                        CYPHER_DEBUG("Executing MATCH+WITH+RETURN query");
                        cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
                        if (!ctx) {
                            set_result_error(result, "Failed to create transform context");
                            return result;
                        }

                        cypher_query_result *transform_result = cypher_transform_query(ctx, query);
                        if (!transform_result) {
                            set_result_error(result, "Failed to transform query");
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        if (transform_result->has_error) {
                            set_result_error(result, transform_result->error_message ? transform_result->error_message : "Transform error");
                            free(transform_result);
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        /* Execute the prepared statement */
                        if (transform_result->stmt) {
                            result->data = NULL;
                            result->row_count = 0;
                            result->column_count = sqlite3_column_count(transform_result->stmt);

                            /* Collect results */
                            while (sqlite3_step(transform_result->stmt) == SQLITE_ROW) {
                                /* Allocate/resize data array */
                                result->data = realloc(result->data, (result->row_count + 1) * sizeof(char**));
                                result->data[result->row_count] = calloc(result->column_count, sizeof(char*));

                                for (int c = 0; c < result->column_count; c++) {
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
                    } else if (match_clause && create_clause && return_clause) {
                        /* MATCH ... CREATE ... RETURN pattern */
                        CYPHER_DEBUG("Executing MATCH+CREATE+RETURN query");
                        if (execute_match_create_return_query(executor, match_clause, create_clause, return_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (match_clause && set_clause) {
                        /* MATCH ... SET pattern */
                        CYPHER_DEBUG("Executing MATCH+SET query");
                        if (execute_match_set_query(executor, match_clause, set_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (match_clause && delete_clause) {
                        /* MATCH ... DELETE pattern */
                        CYPHER_DEBUG("Executing MATCH+DELETE query");
                        if (execute_match_delete_query(executor, match_clause, delete_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (match_clause && create_clause) {
                        /* MATCH ... CREATE pattern */
                        CYPHER_DEBUG("Executing MATCH+CREATE query");
                        if (execute_match_create_query(executor, match_clause, create_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (match_clause && return_clause) {
                        /* MATCH ... RETURN pattern */
                        CYPHER_DEBUG("Executing MATCH+RETURN query");
                        if (execute_match_return_query(executor, match_clause, return_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (set_clause) {
                        /* SET pattern (standalone) */
                        CYPHER_DEBUG("Executing SET clause");
                        if (execute_set_clause(executor, set_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (create_clause) {
                        /* CREATE pattern */
                        CYPHER_DEBUG("Executing CREATE clause");
                        if (execute_create_clause(executor, create_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (merge_clause) {
                        /* MERGE pattern */
                        CYPHER_DEBUG("Executing MERGE clause");
                        if (execute_merge_clause(executor, merge_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (match_clause) {
                        /* MATCH without RETURN - not typical but handle it */
                        CYPHER_DEBUG("Executing MATCH without RETURN");
                        if (execute_match_clause(executor, match_clause, result) < 0) {
                            return result; /* Error already set */
                        }
                    } else if (return_clause) {
                        /* Standalone RETURN - useful for expressions and list comprehensions */
                        CYPHER_DEBUG("Executing standalone RETURN query");

                        /* Check for graph algorithm functions - execute in C for performance */
                        graph_algo_params algo_params = detect_graph_algorithm(return_clause);
                        if (algo_params.type != GRAPH_ALGO_NONE) {
                            graph_algo_result *algo_result = NULL;

                            switch (algo_params.type) {
                                case GRAPH_ALGO_PAGERANK:
                                    CYPHER_DEBUG("Executing C-based PageRank");
                                    algo_result = execute_pagerank(executor->db,
                                                                   algo_params.damping,
                                                                   algo_params.iterations,
                                                                   algo_params.top_k);
                                    break;
                                case GRAPH_ALGO_LABEL_PROPAGATION:
                                    CYPHER_DEBUG("Executing C-based Label Propagation");
                                    algo_result = execute_label_propagation(executor->db,
                                                                            algo_params.iterations);
                                    break;
                                default:
                                    break;
                            }

                            if (algo_result) {
                                if (algo_result->success) {
                                    /* Return the JSON result as a single column, single row */
                                    result->column_count = 1;
                                    result->row_count = 1;
                                    result->data = malloc(sizeof(char**));
                                    result->data[0] = malloc(sizeof(char*));
                                    result->data[0][0] = strdup(algo_result->json_result);
                                    result->success = true;
                                } else {
                                    set_result_error(result, algo_result->error_message ?
                                                     algo_result->error_message : "Graph algorithm failed");
                                }
                                graph_algo_result_free(algo_result);
                                return result;
                            }
                        }

                        /* Standard SQL-based execution for non-algorithm queries */
                        cypher_transform_context *ctx = cypher_transform_create_context(executor->db);
                        if (!ctx) {
                            set_result_error(result, "Failed to create transform context");
                            return result;
                        }

                        cypher_query_result *transform_result = cypher_transform_query(ctx, query);
                        if (!transform_result) {
                            set_result_error(result, "Failed to transform query");
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        if (transform_result->has_error) {
                            set_result_error(result, transform_result->error_message ? transform_result->error_message : "Transform error");
                            free(transform_result);
                            cypher_transform_free_context(ctx);
                            return result;
                        }

                        /* Execute the prepared statement */
                        if (transform_result->stmt) {
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
                    } else {
                        set_result_error(result, "Unsupported query pattern");
                        return result;
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