/* GraphQLite Query Create Module */
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "query_create.h"
#include "property.h"
#include "graphqlite.h"
#include "ast.h"

graphqlite_result_t* execute_create_statement(sqlite3 *db, cypher_ast_node_t *ast) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract pattern from CREATE statement (can be node or relationship)
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    if (!pattern) {
        graphqlite_result_set_error(result, "Missing pattern in CREATE statement");
        return result;
    }
    
    if (pattern->type == AST_NODE_PATTERN) {
        // Handle node creation (existing logic)
        return execute_create_node(db, pattern);
    } else if (pattern->type == AST_RELATIONSHIP_PATTERN) {
        // Handle relationship creation (new logic)
        return execute_create_relationship(db, pattern);
    } else {
        graphqlite_result_set_error(result, "Invalid pattern in CREATE statement");
        return result;
    }
}

int64_t create_node_with_properties(sqlite3 *db, cypher_ast_node_t *node_pattern, graphqlite_result_t *result) {
    // Extract label
    const char *label = NULL;
    if (node_pattern->data.node_pattern.label && node_pattern->data.node_pattern.label->type == AST_LABEL) {
        label = node_pattern->data.node_pattern.label->data.label.name;
    }
    
    if (!label) {
        graphqlite_result_set_error(result, "Node must have a label");
        return -1;
    }
    
    // Step 1: Insert node into nodes table
    const char *insert_node_sql = "INSERT INTO nodes DEFAULT VALUES";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, insert_node_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare node INSERT statement");
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert node");
        return -1;
    }
    
    int64_t node_id = sqlite3_last_insert_rowid(db);
    
    // Step 2: Insert label into node_labels table
    const char *insert_label_sql = "INSERT INTO node_labels (node_id, label) VALUES (?, ?)";
    rc = sqlite3_prepare_v2(db, insert_label_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare label INSERT statement");
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert node label");
        return -1;
    }
    
    // Step 3: Insert properties (if any) into appropriate typed tables
    if (node_pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = node_pattern->data.node_pattern.properties;
        
        // Properties should always be a list now
        if (props->type == AST_PROPERTY_LIST) {
            for (int i = 0; i < props->data.property_list.count; i++) {
                cypher_ast_node_t *prop = props->data.property_list.properties[i];
                const char *prop_key = prop->data.property.key;
                const char *prop_value_str = NULL;
                double prop_value_num = 0.0;
                int prop_value_int = 0;
                graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
                
                // Extract property value and type from AST
                int extracted = extract_property_from_ast(prop->data.property.value, 
                                                         &prop_value_str, &prop_value_num, 
                                                         &prop_value_int, &prop_type);
                
                if (prop_key && extracted && prop_type != GRAPHQLITE_TYPE_NULL) {
                    // Get or create property key ID
                    int64_t key_id = get_or_create_property_key_id(db, prop_key);
                    if (key_id == -1) {
                        graphqlite_result_set_error(result, "Failed to get property key ID");
                        return -1;
                    }
                    
                    // Insert property with correct value based on type
                    const char *value_to_insert = NULL;
                    char int_str[32], float_str[32];
                    
                    switch (prop_type) {
                        case GRAPHQLITE_TYPE_TEXT:
                            value_to_insert = prop_value_str;
                            break;
                        case GRAPHQLITE_TYPE_INTEGER:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        case GRAPHQLITE_TYPE_FLOAT:
                            snprintf(float_str, sizeof(float_str), "%.15g", prop_value_num);
                            value_to_insert = float_str;
                            break;
                        case GRAPHQLITE_TYPE_BOOLEAN:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        default:
                            break;
                    }
                    
                    if (value_to_insert) {
                        int prop_result = insert_node_property(db, node_id, key_id, value_to_insert, prop_type);
                        if (prop_result != GRAPHQLITE_OK) {
                            graphqlite_result_set_error(result, "Failed to insert node property");
                            return -1;
                        }
                    }
                }
            }
        }
    }
    
    return node_id;
}

graphqlite_result_t* execute_create_node(sqlite3 *db, cypher_ast_node_t *node_pattern) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    int64_t node_id = create_node_with_properties(db, node_pattern, result);
    if (node_id == -1) {
        return result;  // Error already set in result
    }
    
    // Return success result (no data for CREATE)
    result->result_code = GRAPHQLITE_OK;
    return result;
}

graphqlite_result_t* execute_create_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract nodes and edge from relationship pattern
    cypher_ast_node_t *left_node = rel_pattern->data.relationship_pattern.left_node;
    cypher_ast_node_t *edge = rel_pattern->data.relationship_pattern.edge;
    cypher_ast_node_t *right_node = rel_pattern->data.relationship_pattern.right_node;
    int direction = rel_pattern->data.relationship_pattern.direction;
    
    if (!left_node || !edge || !right_node) {
        graphqlite_result_set_error(result, "Invalid relationship pattern");
        return result;
    }
    
    // Create left node
    int64_t left_id = create_node_with_properties(db, left_node, result);
    if (left_id == -1) {
        return result;  // Error already set
    }
    
    // Create right node  
    int64_t right_id = create_node_with_properties(db, right_node, result);
    if (right_id == -1) {
        return result;  // Error already set
    }
    
    // Extract edge type
    const char *edge_type = NULL;
    if (edge->data.edge_pattern.label && edge->data.edge_pattern.label->type == AST_LABEL) {
        edge_type = edge->data.edge_pattern.label->data.label.name;
    }
    
    if (!edge_type) {
        graphqlite_result_set_error(result, "Edge must have a type");
        return result;
    }
    
    // Determine source and target based on direction
    int64_t source_id = (direction == -1) ? right_id : left_id;
    int64_t target_id = (direction == -1) ? left_id : right_id;
    
    // Insert edge into edges table
    const char *insert_edge_sql = "INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, insert_edge_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare edge INSERT statement");
        return result;
    }
    
    sqlite3_bind_int64(stmt, 1, source_id);
    sqlite3_bind_int64(stmt, 2, target_id);
    sqlite3_bind_text(stmt, 3, edge_type, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert edge");
        return result;
    }
    
    int64_t edge_id = sqlite3_last_insert_rowid(db);
    
    // Insert edge properties (if any)
    if (edge->data.edge_pattern.properties) {
        cypher_ast_node_t *props = edge->data.edge_pattern.properties;
        
        if (props->type == AST_PROPERTY_LIST) {
            for (int i = 0; i < props->data.property_list.count; i++) {
                cypher_ast_node_t *prop = props->data.property_list.properties[i];
                const char *prop_key = prop->data.property.key;
                const char *prop_value_str = NULL;
                double prop_value_num = 0.0;
                int prop_value_int = 0;
                graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
                
                // Extract property value and type from AST
                int extracted = extract_property_from_ast(prop->data.property.value, 
                                                         &prop_value_str, &prop_value_num, 
                                                         &prop_value_int, &prop_type);
                
                if (prop_key && extracted && prop_type != GRAPHQLITE_TYPE_NULL) {
                    // Get or create property key ID
                    int64_t key_id = get_or_create_property_key_id(db, prop_key);
                    if (key_id == -1) {
                        graphqlite_result_set_error(result, "Failed to get property key ID");
                        return result;
                    }
                    
                    // Insert edge property with correct value based on type
                    const char *value_to_insert = NULL;
                    char int_str[32], float_str[32];
                    
                    switch (prop_type) {
                        case GRAPHQLITE_TYPE_TEXT:
                            value_to_insert = prop_value_str;
                            break;
                        case GRAPHQLITE_TYPE_INTEGER:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        case GRAPHQLITE_TYPE_FLOAT:
                            snprintf(float_str, sizeof(float_str), "%.15g", prop_value_num);
                            value_to_insert = float_str;
                            break;
                        case GRAPHQLITE_TYPE_BOOLEAN:
                            snprintf(int_str, sizeof(int_str), "%d", prop_value_int);
                            value_to_insert = int_str;
                            break;
                        default:
                            break;
                    }
                    
                    if (value_to_insert) {
                        int prop_result = insert_edge_property(db, edge_id, key_id, value_to_insert, prop_type);
                        if (prop_result != GRAPHQLITE_OK) {
                            graphqlite_result_set_error(result, "Failed to insert edge property");
                            return result;
                        }
                    }
                }
            }
        }
    }
    
    // Return success result (no data for CREATE)
    result->result_code = GRAPHQLITE_OK;
    return result;
}