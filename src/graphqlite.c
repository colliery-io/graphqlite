/* GraphQLite - OpenCypher extension for SQLite */
#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

#include "graphqlite.h"
#include "ast.h"
#include "schema.h"
#include "property.h"
#include "expression.h"
#include "serialization.h"
#include "query_create.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declaration for parser function
extern cypher_ast_node_t* parse_cypher_query(const char *query);




// ============================================================================
// Query Execution - Forward Declarations
// ============================================================================

static graphqlite_result_t* execute_match_node(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *return_stmt);
static graphqlite_result_t* execute_match_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *return_stmt);
static graphqlite_result_t* execute_match_node_with_where(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt);
static graphqlite_result_t* execute_match_relationship_with_where(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt);

// ============================================================================
// Query Execution
// ============================================================================


static graphqlite_result_t* execute_match_statement(sqlite3 *db, cypher_ast_node_t *match_stmt, cypher_ast_node_t *return_stmt) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract pattern from MATCH statement (can be node or relationship)
    cypher_ast_node_t *pattern = match_stmt->data.match_stmt.node_pattern;
    cypher_ast_node_t *where_clause = match_stmt->data.match_stmt.where_clause;
    
    if (!pattern) {
        graphqlite_result_set_error(result, "Missing pattern in MATCH statement");
        return result;
    }
    
    graphqlite_result_free(result);  // Free the temporary result
    
    if (pattern->type == AST_NODE_PATTERN) {
        // Handle node matching (existing logic)
        return execute_match_node_with_where(db, pattern, where_clause, return_stmt);
    } else if (pattern->type == AST_RELATIONSHIP_PATTERN) {
        // Handle relationship matching (new logic)
        return execute_match_relationship_with_where(db, pattern, where_clause, return_stmt);
    } else {
        result = graphqlite_result_create();
        if (result) {
            graphqlite_result_set_error(result, "Invalid pattern in MATCH statement");
        }
        return result;
    }
}

static graphqlite_result_t* execute_match_node(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *return_stmt) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Build SELECT query based on pattern
    const char *label = NULL;
    const char *prop_key = NULL;
    const char *prop_value_str = NULL;
    double prop_value_num = 0.0;
    int prop_value_int = 0;
    graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
    
    if (node_pattern->data.node_pattern.label && node_pattern->data.node_pattern.label->type == AST_LABEL) {
        label = node_pattern->data.node_pattern.label->data.label.name;
    }
    
    if (node_pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = node_pattern->data.node_pattern.properties;
        
        // Properties should always be a list now
        if (props->type == AST_PROPERTY_LIST && props->data.property_list.count > 0) {
            // For MATCH, we only use the first property for now
            // TODO: Support multiple property filters in MATCH
            cypher_ast_node_t *prop = props->data.property_list.properties[0];
            prop_key = prop->data.property.key;
            extract_property_from_ast(prop->data.property.value, 
                                     &prop_value_str, &prop_value_num, 
                                     &prop_value_int, &prop_type);
        }
    }
    
    // Build query using new schema with JOINs
    char query[1024];
    const char *prop_table = NULL;
    
    // Determine which property table to query based on type
    switch (prop_type) {
        case GRAPHQLITE_TYPE_TEXT:
            prop_table = "node_props_text";
            break;
        case GRAPHQLITE_TYPE_INTEGER:
            prop_table = "node_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            prop_table = "node_props_real";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            prop_table = "node_props_bool";
            break;
        default:
            prop_table = NULL;
            break;
    }
    
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        // Match by label AND property
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, pk.key, %s.value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "JOIN %s ON n.id = %s.node_id "
            "JOIN property_keys pk ON %s.key_id = pk.id "
            "WHERE nl.label = ? AND pk.key = ? AND %s.value = ?",
            prop_table, prop_table, prop_table, prop_table, prop_table);
    } else if (label) {
        // Match by label only
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "WHERE nl.label = ?");
    } else {
        // Match all nodes
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare SELECT statement");
        return result;
    }
    
    // Bind parameters
    int param_idx = 1;
    if (label) {
        sqlite3_bind_text(stmt, param_idx++, label, -1, SQLITE_STATIC);
    }
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        sqlite3_bind_text(stmt, param_idx++, prop_key, -1, SQLITE_STATIC);
        
        // Bind value based on type
        switch (prop_type) {
            case GRAPHQLITE_TYPE_TEXT:
                sqlite3_bind_text(stmt, param_idx++, prop_value_str, -1, SQLITE_STATIC);
                break;
            case GRAPHQLITE_TYPE_INTEGER:
            case GRAPHQLITE_TYPE_BOOLEAN:
                sqlite3_bind_int(stmt, param_idx++, prop_value_int);
                break;
            case GRAPHQLITE_TYPE_FLOAT:
                sqlite3_bind_double(stmt, param_idx++, prop_value_num);
                break;
            default:
                break;
        }
    }
    
    // Parse which variable to return from RETURN statement
    const char *return_variable = "node"; // default
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    // Set up single result column for entity
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        graphqlite_result_add_row(result);
        size_t row_idx = result->row_count - 1;
        
        // Serialize complete node entity
        graphqlite_value_t value;
        value.type = GRAPHQLITE_TYPE_TEXT;
        int64_t node_id = sqlite3_column_int64(stmt, 0);
        value.data.text = serialize_node_entity(db, node_id);
        
        graphqlite_result_set_value(result, row_idx, 0, &value);
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_match_node_with_where(sqlite3 *db, cypher_ast_node_t *node_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt) {
    // If no WHERE clause, fall back to original function
    if (!where_clause) {
        return execute_match_node(db, node_pattern, return_stmt);
    }
    
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Build SELECT query based on pattern (same as original function)
    const char *label = NULL;
    const char *prop_key = NULL;
    const char *prop_value_str = NULL;
    double prop_value_num = 0.0;
    int prop_value_int = 0;
    graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
    
    if (node_pattern->data.node_pattern.label && node_pattern->data.node_pattern.label->type == AST_LABEL) {
        label = node_pattern->data.node_pattern.label->data.label.name;
    }
    
    if (node_pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = node_pattern->data.node_pattern.properties;
        
        if (props->type == AST_PROPERTY_LIST && props->data.property_list.count > 0) {
            cypher_ast_node_t *prop = props->data.property_list.properties[0];
            prop_key = prop->data.property.key;
            extract_property_from_ast(prop->data.property.value, 
                                     &prop_value_str, &prop_value_num, 
                                     &prop_value_int, &prop_type);
        }
    }
    
    // Build query using same logic as original function
    char query[1024];
    const char *prop_table = NULL;
    
    switch (prop_type) {
        case GRAPHQLITE_TYPE_TEXT:
            prop_table = "node_props_text";
            break;
        case GRAPHQLITE_TYPE_INTEGER:
            prop_table = "node_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            prop_table = "node_props_real";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            prop_table = "node_props_bool";
            break;
        default:
            prop_table = NULL;
            break;
    }
    
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, pk.key, %s.value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "JOIN %s ON n.id = %s.node_id "
            "JOIN property_keys pk ON %s.key_id = pk.id "
            "WHERE nl.label = ? AND pk.key = ? AND %s.value = ?",
            prop_table, prop_table, prop_table, prop_table, prop_table);
    } else if (label) {
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id "
            "WHERE nl.label = ?");
    } else {
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT n.id, nl.label, 'NULL' as key, 'NULL' as value "
            "FROM nodes n "
            "JOIN node_labels nl ON n.id = nl.node_id");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare SELECT statement");
        return result;
    }
    
    // Bind parameters (same as original function)
    int param_idx = 1;
    if (label) {
        sqlite3_bind_text(stmt, param_idx++, label, -1, SQLITE_STATIC);
    }
    if (prop_key && prop_type != GRAPHQLITE_TYPE_NULL && prop_table) {
        sqlite3_bind_text(stmt, param_idx++, prop_key, -1, SQLITE_STATIC);
        
        switch (prop_type) {
            case GRAPHQLITE_TYPE_TEXT:
                sqlite3_bind_text(stmt, param_idx++, prop_value_str, -1, SQLITE_STATIC);
                break;
            case GRAPHQLITE_TYPE_INTEGER:
            case GRAPHQLITE_TYPE_BOOLEAN:
                sqlite3_bind_int(stmt, param_idx++, prop_value_int);
                break;
            case GRAPHQLITE_TYPE_FLOAT:
                sqlite3_bind_double(stmt, param_idx++, prop_value_num);
                break;
            default:
                break;
        }
    }
    
    // Parse return variable
    const char *return_variable = "node"; // default
    const char *node_variable = "n"; // default node variable
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    // Extract node variable from pattern for binding
    if (node_pattern->data.node_pattern.variable && node_pattern->data.node_pattern.variable->type == AST_VARIABLE) {
        node_variable = node_pattern->data.node_pattern.variable->data.variable.name;
    }
    
    // Set up result column
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch and filter results with WHERE clause
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t node_id = sqlite3_column_int64(stmt, 0);
        
        // Create variable binding for WHERE evaluation
        variable_binding_t binding;
        binding.variable_name = (char*)node_variable;
        binding.node_id = node_id;
        binding.edge_id = -1;
        binding.is_edge = 0;
        
        // Evaluate WHERE clause
        eval_result_t *where_result = evaluate_expression(db, where_clause->data.where_clause.expression, &binding, 1);
        
        // Check if WHERE condition is satisfied
        int include_result = 0;
        if (where_result && where_result->type == GRAPHQLITE_TYPE_BOOLEAN && where_result->data.boolean) {
            include_result = 1;
        }
        
        free_eval_result(where_result);
        
        if (include_result) {
            graphqlite_result_add_row(result);
            size_t row_idx = result->row_count - 1;
            
            // Serialize complete node entity
            graphqlite_value_t value;
            value.type = GRAPHQLITE_TYPE_TEXT;
            value.data.text = serialize_node_entity(db, node_id);
            
            graphqlite_result_set_value(result, row_idx, 0, &value);
        }
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}


static graphqlite_result_t* execute_match_relationship(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *return_stmt) {
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
    
    // Extract patterns for matching
    const char *left_label = NULL;
    const char *right_label = NULL;
    const char *edge_type = NULL;
    
    // Get left node label
    if (left_node->data.node_pattern.label && left_node->data.node_pattern.label->type == AST_LABEL) {
        left_label = left_node->data.node_pattern.label->data.label.name;
    }
    
    // Get right node label  
    if (right_node->data.node_pattern.label && right_node->data.node_pattern.label->type == AST_LABEL) {
        right_label = right_node->data.node_pattern.label->data.label.name;
    }
    
    // Get edge type
    if (edge->data.edge_pattern.label && edge->data.edge_pattern.label->type == AST_LABEL) {
        edge_type = edge->data.edge_pattern.label->data.label.name;
    }
    
    // Build query to match relationships
    char query[2048];
    if (direction == 1) {  // Right direction ->
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.source_id = ln.id "
            "JOIN nodes rn ON e.target_id = rn.id "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else if (direction == -1) {  // Left direction <-
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.target_id = ln.id "  // Reversed
            "JOIN nodes rn ON e.source_id = rn.id "  // Reversed
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else {  // Undirected (direction == 0)
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON (e.source_id = ln.id OR e.target_id = ln.id) "
            "JOIN nodes rn ON (e.source_id = rn.id OR e.target_id = rn.id) "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE ln.id != rn.id %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare relationship SELECT statement");
        return result;
    }
    
    // Bind parameters
    int param_idx = 1;
    if (left_label) {
        sqlite3_bind_text(stmt, param_idx++, left_label, -1, SQLITE_STATIC);
    }
    if (edge_type) {
        sqlite3_bind_text(stmt, param_idx++, edge_type, -1, SQLITE_STATIC);
    }
    if (right_label) {
        sqlite3_bind_text(stmt, param_idx++, right_label, -1, SQLITE_STATIC);
    }
    
    // TODO: Handle edge properties in WHERE clause
    
    // Parse which variable to return from RETURN statement
    const char *return_variable = NULL;
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    if (!return_variable) {
        graphqlite_result_set_error(result, "Cannot determine which variable to return");
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Extract variable names from relationship pattern to map RETURN variable
    const char *left_var = NULL, *edge_var = NULL, *right_var = NULL;
    
    if (left_node && left_node->data.node_pattern.variable) {
        left_var = left_node->data.node_pattern.variable->data.variable.name;
    }
    if (edge && edge->data.edge_pattern.variable) {
        edge_var = edge->data.edge_pattern.variable->data.variable.name;
    }
    if (right_node && right_node->data.node_pattern.variable) {
        right_var = right_node->data.node_pattern.variable->data.variable.name;
    }
    
    // Determine which part of the pattern to return
    enum return_type { RETURN_LEFT_NODE, RETURN_EDGE, RETURN_RIGHT_NODE } return_type;
    if (left_var && strcmp(return_variable, left_var) == 0) {
        return_type = RETURN_LEFT_NODE;
    } else if (edge_var && strcmp(return_variable, edge_var) == 0) {
        return_type = RETURN_EDGE;
    } else if (right_var && strcmp(return_variable, right_var) == 0) {
        return_type = RETURN_RIGHT_NODE;
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Variable '%s' not found in pattern", return_variable);
        graphqlite_result_set_error(result, error_msg);
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Set up single result column for entity
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        graphqlite_result_add_row(result);
        size_t row_idx = result->row_count - 1;
        
        // Get entity ID based on what we're returning and serialize complete entity
        graphqlite_value_t value;
        value.type = GRAPHQLITE_TYPE_TEXT;
        
        if (return_type == RETURN_LEFT_NODE) {
            int64_t node_id = sqlite3_column_int64(stmt, 0);
            value.data.text = serialize_node_entity(db, node_id);
        } else if (return_type == RETURN_EDGE) {
            int64_t edge_id = sqlite3_column_int64(stmt, 2);
            value.data.text = serialize_relationship_entity(db, edge_id);
        } else { // RETURN_RIGHT_NODE
            int64_t node_id = sqlite3_column_int64(stmt, 4);
            value.data.text = serialize_node_entity(db, node_id);
        }
        
        graphqlite_result_set_value(result, row_idx, 0, &value);
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute relationship SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_match_relationship_with_where(sqlite3 *db, cypher_ast_node_t *rel_pattern, cypher_ast_node_t *where_clause, cypher_ast_node_t *return_stmt) {
    // If no WHERE clause, fall back to original function
    if (!where_clause) {
        return execute_match_relationship(db, rel_pattern, return_stmt);
    }
    
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract nodes and edge from relationship pattern (same as original)
    cypher_ast_node_t *left_node = rel_pattern->data.relationship_pattern.left_node;
    cypher_ast_node_t *edge = rel_pattern->data.relationship_pattern.edge;
    cypher_ast_node_t *right_node = rel_pattern->data.relationship_pattern.right_node;
    int direction = rel_pattern->data.relationship_pattern.direction;
    
    if (!left_node || !edge || !right_node) {
        graphqlite_result_set_error(result, "Invalid relationship pattern");
        return result;
    }
    
    // Extract patterns for matching (same as original)
    const char *left_label = NULL;
    const char *right_label = NULL;
    const char *edge_type = NULL;
    
    if (left_node->data.node_pattern.label && left_node->data.node_pattern.label->type == AST_LABEL) {
        left_label = left_node->data.node_pattern.label->data.label.name;
    }
    
    if (right_node->data.node_pattern.label && right_node->data.node_pattern.label->type == AST_LABEL) {
        right_label = right_node->data.node_pattern.label->data.label.name;
    }
    
    if (edge->data.edge_pattern.label && edge->data.edge_pattern.label->type == AST_LABEL) {
        edge_type = edge->data.edge_pattern.label->data.label.name;
    }
    
    // Build query to match relationships (same as original)
    char query[2048];
    if (direction == 1) {  // Right direction ->
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.source_id = ln.id "
            "JOIN nodes rn ON e.target_id = rn.id "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else if (direction == -1) {  // Left direction <-
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON e.target_id = ln.id "  // Reversed
            "JOIN nodes rn ON e.source_id = rn.id "  // Reversed
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE 1=1 %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    } else {  // Undirected (direction == 0)
        snprintf(query, sizeof(query), 
            "SELECT DISTINCT "
            "  ln.id as left_id, ll.label as left_label, "
            "  e.id as edge_id, e.type as edge_type, "
            "  rn.id as right_id, rl.label as right_label "
            "FROM edges e "
            "JOIN nodes ln ON (e.source_id = ln.id OR e.target_id = ln.id) "
            "JOIN nodes rn ON (e.source_id = rn.id OR e.target_id = rn.id) "
            "JOIN node_labels ll ON ln.id = ll.node_id "
            "JOIN node_labels rl ON rn.id = rl.node_id "
            "WHERE ln.id != rn.id %s %s %s",
            left_label ? "AND ll.label = ?" : "",
            edge_type ? "AND e.type = ?" : "",
            right_label ? "AND rl.label = ?" : "");
    }
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare relationship SELECT statement");
        return result;
    }
    
    // Bind parameters (same as original)
    int param_idx = 1;
    if (left_label) {
        sqlite3_bind_text(stmt, param_idx++, left_label, -1, SQLITE_STATIC);
    }
    if (edge_type) {
        sqlite3_bind_text(stmt, param_idx++, edge_type, -1, SQLITE_STATIC);
    }
    if (right_label) {
        sqlite3_bind_text(stmt, param_idx++, right_label, -1, SQLITE_STATIC);
    }
    
    // Parse which variable to return
    const char *return_variable = NULL;
    if (return_stmt && return_stmt->type == AST_RETURN_STATEMENT) {
        cypher_ast_node_t *var = return_stmt->data.return_stmt.variable;
        if (var && var->type == AST_VARIABLE) {
            return_variable = var->data.variable.name;
        }
    }
    
    if (!return_variable) {
        graphqlite_result_set_error(result, "Cannot determine which variable to return");
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Extract variable names from relationship pattern
    const char *left_var = NULL, *edge_var = NULL, *right_var = NULL;
    
    if (left_node && left_node->data.node_pattern.variable) {
        left_var = left_node->data.node_pattern.variable->data.variable.name;
    }
    if (edge && edge->data.edge_pattern.variable) {
        edge_var = edge->data.edge_pattern.variable->data.variable.name;
    }
    if (right_node && right_node->data.node_pattern.variable) {
        right_var = right_node->data.node_pattern.variable->data.variable.name;
    }
    
    // Determine which part of the pattern to return
    enum return_type { RETURN_LEFT_NODE, RETURN_EDGE, RETURN_RIGHT_NODE } return_type;
    if (left_var && strcmp(return_variable, left_var) == 0) {
        return_type = RETURN_LEFT_NODE;
    } else if (edge_var && strcmp(return_variable, edge_var) == 0) {
        return_type = RETURN_EDGE;
    } else if (right_var && strcmp(return_variable, right_var) == 0) {
        return_type = RETURN_RIGHT_NODE;
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Variable '%s' not found in pattern", return_variable);
        graphqlite_result_set_error(result, error_msg);
        sqlite3_finalize(stmt);
        return result;
    }
    
    // Set up result column
    graphqlite_result_add_column(result, return_variable, GRAPHQLITE_TYPE_TEXT);
    
    // Fetch and filter results with WHERE clause
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int64_t left_id = sqlite3_column_int64(stmt, 0);
        int64_t edge_id = sqlite3_column_int64(stmt, 2);
        int64_t right_id = sqlite3_column_int64(stmt, 4);
        
        // Create variable bindings for WHERE evaluation
        variable_binding_t bindings[3];
        int binding_count = 0;
        
        if (left_var) {
            bindings[binding_count].variable_name = (char*)left_var;
            bindings[binding_count].node_id = left_id;
            bindings[binding_count].edge_id = -1;
            bindings[binding_count].is_edge = 0;
            binding_count++;
        }
        
        if (edge_var) {
            bindings[binding_count].variable_name = (char*)edge_var;
            bindings[binding_count].node_id = -1;
            bindings[binding_count].edge_id = edge_id;
            bindings[binding_count].is_edge = 1;
            binding_count++;
        }
        
        if (right_var) {
            bindings[binding_count].variable_name = (char*)right_var;
            bindings[binding_count].node_id = right_id;
            bindings[binding_count].edge_id = -1;
            bindings[binding_count].is_edge = 0;
            binding_count++;
        }
        
        // Evaluate WHERE clause
        eval_result_t *where_result = evaluate_expression(db, where_clause->data.where_clause.expression, bindings, binding_count);
        
        // Check if WHERE condition is satisfied
        int include_result = 0;
        if (where_result && where_result->type == GRAPHQLITE_TYPE_BOOLEAN && where_result->data.boolean) {
            include_result = 1;
        }
        
        free_eval_result(where_result);
        
        if (include_result) {
            graphqlite_result_add_row(result);
            size_t row_idx = result->row_count - 1;
            
            // Get entity ID based on what we're returning and serialize complete entity
            graphqlite_value_t value;
            value.type = GRAPHQLITE_TYPE_TEXT;
            
            if (return_type == RETURN_LEFT_NODE) {
                value.data.text = serialize_node_entity(db, left_id);
            } else if (return_type == RETURN_EDGE) {
                value.data.text = serialize_relationship_entity(db, edge_id);
            } else { // RETURN_RIGHT_NODE
                value.data.text = serialize_node_entity(db, right_id);
            }
            
            graphqlite_result_set_value(result, row_idx, 0, &value);
        }
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute relationship SELECT statement");
        return result;
    }
    
    result->result_code = GRAPHQLITE_OK;
    return result;
}

static graphqlite_result_t* execute_query(sqlite3 *db, cypher_ast_node_t *ast) {
    if (!ast) {
        graphqlite_result_t *result = graphqlite_result_create();
        if (result) {
            graphqlite_result_set_error(result, "Invalid AST");
        }
        return result;
    }
    
    switch (ast->type) {
        case AST_CREATE_STATEMENT:
            return execute_create_statement(db, ast);
            
        case AST_COMPOUND_STATEMENT: {
            // MATCH + RETURN combination
            cypher_ast_node_t *match_stmt = ast->data.compound_stmt.match_stmt;
            cypher_ast_node_t *return_stmt = ast->data.compound_stmt.return_stmt;
            return execute_match_statement(db, match_stmt, return_stmt);
        }
        
        default: {
            graphqlite_result_t *result = graphqlite_result_create();
            if (result) {
                graphqlite_result_set_error(result, "Unsupported statement type");
            }
            return result;
        }
    }
}

// ============================================================================
// SQLite Function Implementation
// ============================================================================

void graphqlite_cypher_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc != 1) {
        sqlite3_result_error(context, "cypher() function requires exactly one argument", -1);
        return;
    }
    
    const char *query = (const char*)sqlite3_value_text(argv[0]);
    if (!query) {
        sqlite3_result_error(context, "Query string cannot be NULL", -1);
        return;
    }
    
    // Get database connection
    sqlite3 *db = sqlite3_context_db_handle(context);
    
    // Parse the query
    cypher_ast_node_t *ast = parse_cypher_query(query);
    if (!ast) {
        sqlite3_result_error(context, "Failed to parse OpenCypher query", -1);
        return;
    }
    
    // Execute the query
    graphqlite_result_t *result = execute_query(db, ast);
    
    // Clean up AST
    ast_free(ast);
    
    if (!result) {
        sqlite3_result_error(context, "Query execution failed", -1);
        return;
    }
    
    if (result->result_code != GRAPHQLITE_OK) {
        sqlite3_result_error(context, result->error_message ? result->error_message : "Unknown error", -1);
        graphqlite_result_free(result);
        return;
    }
    
    // Return actual result data
    if (result->row_count > 0 && result->column_count > 0) {
        // For single value results, return the value directly
        if (result->column_count == 1) {
            graphqlite_value_t *value = &result->rows[0].values[0];
            
            switch (value->type) {
                case GRAPHQLITE_TYPE_INTEGER:
                    sqlite3_result_int64(context, value->data.integer);
                    break;
                case GRAPHQLITE_TYPE_TEXT:
                    if (value->data.text) {
                        sqlite3_result_text(context, value->data.text, -1, SQLITE_TRANSIENT);
                    } else {
                        sqlite3_result_null(context);
                    }
                    break;
                case GRAPHQLITE_TYPE_FLOAT:
                    sqlite3_result_double(context, value->data.float_val);
                    break;
                case GRAPHQLITE_TYPE_BOOLEAN:
                    sqlite3_result_int(context, value->data.boolean ? 1 : 0);
                    break;
                default:
                    sqlite3_result_null(context);
                    break;
            }
        } else {
            // For multiple columns, return JSON-like format
            char result_str[2048] = "{";
            
            for (size_t col = 0; col < result->column_count; col++) {
                char col_str[512];
                graphqlite_value_t *value = &result->rows[0].values[col];
                
                if (col > 0) {
                    strncat(result_str, ", ", sizeof(result_str) - strlen(result_str) - 1);
                }
                
                switch (value->type) {
                    case GRAPHQLITE_TYPE_INTEGER:
                        snprintf(col_str, sizeof(col_str), "\"%s\": %lld", 
                                result->columns[col].name, (long long)value->data.integer);
                        break;
                    case GRAPHQLITE_TYPE_TEXT:
                        snprintf(col_str, sizeof(col_str), "\"%s\": \"%s\"", 
                                result->columns[col].name, 
                                value->data.text ? value->data.text : "null");
                        break;
                    case GRAPHQLITE_TYPE_FLOAT:
                        snprintf(col_str, sizeof(col_str), "\"%s\": %g", 
                                result->columns[col].name, value->data.float_val);
                        break;
                    case GRAPHQLITE_TYPE_BOOLEAN:
                        snprintf(col_str, sizeof(col_str), "\"%s\": %s", 
                                result->columns[col].name, value->data.boolean ? "true" : "false");
                        break;
                    default:
                        snprintf(col_str, sizeof(col_str), "\"%s\": null", result->columns[col].name);
                        break;
                }
                
                strncat(result_str, col_str, sizeof(result_str) - strlen(result_str) - 1);
            }
            
            strncat(result_str, "}", sizeof(result_str) - strlen(result_str) - 1);
            sqlite3_result_text(context, result_str, -1, SQLITE_TRANSIENT);
        }
    } else {
        // For successful operations with no result data (like CREATE without RETURN)
        if (result->result_code == GRAPHQLITE_OK) {
            sqlite3_result_text(context, "Query executed successfully", -1, SQLITE_STATIC);
        } else {
            sqlite3_result_null(context);
        }
    }
    
    graphqlite_result_free(result);
}

// ============================================================================
// Extension Entry Point
// ============================================================================

// Simple test function - using direct calls like working simple_test.c
static void simple_test_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_text(context, "GraphQLite extension loaded successfully!", -1, SQLITE_STATIC);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_graphqlite_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  
  /* Register the graphqlite_test function */
  sqlite3_create_function(db, "graphqlite_test", 0, SQLITE_UTF8, 0,
                         simple_test_func, 0, 0);
  
  /* Register the main cypher() function */
  sqlite3_create_function(db, "cypher", 1, SQLITE_UTF8, 0,
                         graphqlite_cypher_func, 0, 0);
  
  /* Create schema during initialization */
  create_schema(db);
  
  return rc;
}