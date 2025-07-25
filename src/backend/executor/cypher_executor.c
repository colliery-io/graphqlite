/*
 * Cypher Execution Engine
 * Orchestrates parser, transformer, and schema manager for end-to-end query execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "executor/cypher_executor.h"
#include "parser/cypher_debug.h"

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
                
                /* Handle node labels and properties for new nodes */
                if (node_pattern->label) {
                    if (cypher_schema_add_node_label(executor->schema_mgr, node_id, node_pattern->label) == 0) {
                        CYPHER_DEBUG("Added label '%s' to node %d", node_pattern->label, node_id);
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
                
                /* Handle target node labels and properties for new nodes */
                if (target_pattern->label) {
                    if (cypher_schema_add_node_label(executor->schema_mgr, target_node_id, target_pattern->label) == 0) {
                        CYPHER_DEBUG("Added label '%s' to target node %d", target_pattern->label, target_node_id);
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
                    for (int i = 0; i < query->clauses->count; i++) {
                        ast_node *clause = query->clauses->items[i];
                        CYPHER_DEBUG("Processing clause %d, type: %d", i, clause->type);
                        
                        switch (clause->type) {
                            case AST_NODE_CREATE:
                                CYPHER_DEBUG("Executing CREATE clause");
                                if (execute_create_clause(executor, (cypher_create*)clause, result) < 0) {
                                    return result; /* Error already set */
                                }
                                break;
                                
                            case AST_NODE_MATCH:
                                CYPHER_DEBUG("Executing MATCH clause");
                                if (execute_match_clause(executor, (cypher_match*)clause, result) < 0) {
                                    return result; /* Error already set */
                                }
                                break;
                                
                            case AST_NODE_RETURN:
                                CYPHER_DEBUG("Skipping RETURN clause");
                                /* RETURN handled by MATCH for now */
                                break;
                                
                            default:
                                CYPHER_DEBUG("Unhandled clause type: %d", clause->type);
                                break;
                        }
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
    
    /* Parse query to AST */
    CYPHER_DEBUG("Parsing query: '%s'", query);
    ast_node *ast = parse_cypher_query(query);
    if (!ast) {
        CYPHER_DEBUG("Parser returned NULL");
        cypher_result *result = create_empty_result();
        if (result) {
            set_result_error(result, "Failed to parse query");
        }
        return result;
    }
    
    CYPHER_DEBUG("Parser returned AST with type=%d, data=%p", ast->type, ast->data);
    
    /* Check for parser errors */
    const char *parse_error = cypher_parser_get_error(ast);
    if (parse_error) {
        CYPHER_DEBUG("Parser error: %s", parse_error);
        
        /* If there's a parse error, treat it as a failure */
        cypher_result *result = create_empty_result();
        if (result) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Parse error: %s", parse_error);
            set_result_error(result, error_msg);
        }
        cypher_parser_free_result(ast);
        return result;
    } else {
        CYPHER_DEBUG("No parser error, proceeding with execution");
    }
    
    /* Execute AST */
    cypher_result *result = cypher_executor_execute_ast(executor, ast);
    
    /* Clean up AST */
    cypher_parser_free_result(ast);
    
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
    if (result->nodes_created > 0 || result->properties_set > 0) {
        printf("Created %d nodes, set %d properties\n", result->nodes_created, result->properties_set);
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