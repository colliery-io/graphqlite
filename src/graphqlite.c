/* GraphQLite - OpenCypher extension for SQLite */
#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

#include "graphqlite.h"
#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declaration for parser function
extern cypher_ast_node_t* parse_cypher_query(const char *query);

// ============================================================================
// Schema Management
// ============================================================================

static int create_schema(sqlite3 *db) {
    int rc = SQLITE_OK;
    char *err_msg = NULL;
    
    // Check if schema already exists (idempotent initialization)
    sqlite3_stmt *check_stmt;
    rc = sqlite3_prepare_v2(db, 
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='nodes'",
        -1, &check_stmt, NULL);
    if (rc == SQLITE_OK && sqlite3_step(check_stmt) == SQLITE_ROW) {
        sqlite3_finalize(check_stmt);
        return GRAPHQLITE_OK;  // Schema already exists
    }
    sqlite3_finalize(check_stmt);
    
    // Disable foreign keys during schema creation
    sqlite3_exec(db, "PRAGMA foreign_keys = OFF", NULL, NULL, NULL);
    
    // Begin transaction for atomic schema creation
    rc = sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        return GRAPHQLITE_ERROR;
    }
    
    // Schema SQL ordered by dependencies (parent tables first)
    const char *schema_sql[] = {
        // Core tables without foreign keys first
        "CREATE TABLE nodes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT"
        ")",
        
        "CREATE TABLE property_keys ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  key TEXT UNIQUE NOT NULL"
        ")",
        
        // Tables with foreign keys after their dependencies
        "CREATE TABLE edges ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  type TEXT NOT NULL"
        ")",
        
        "CREATE TABLE node_labels ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  label TEXT NOT NULL,"
        "  PRIMARY KEY (node_id, label)"
        ")",
        
        // Node property tables (typed EAV)
        "CREATE TABLE node_props_int ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL,"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        "CREATE TABLE node_props_text ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value TEXT NOT NULL,"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        "CREATE TABLE node_props_real ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value REAL NOT NULL,"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        "CREATE TABLE node_props_bool ("
        "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
        "  PRIMARY KEY (node_id, key_id)"
        ")",
        
        // Edge property tables (typed EAV)
        "CREATE TABLE edge_props_int ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL,"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        "CREATE TABLE edge_props_text ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value TEXT NOT NULL,"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        "CREATE TABLE edge_props_real ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value REAL NOT NULL,"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        "CREATE TABLE edge_props_bool ("
        "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
        "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
        "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
        "  PRIMARY KEY (edge_id, key_id)"
        ")",
        
        NULL  // Sentinel
    };
    
    // Create tables
    for (int i = 0; schema_sql[i] != NULL; i++) {
        rc = sqlite3_exec(db, schema_sql[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) {
                sqlite3_free(err_msg);
            }
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
            return GRAPHQLITE_ERROR;
        }
    }
    
    // Performance indexes
    const char *index_sql[] = {
        // Core indexes for performance
        "CREATE INDEX idx_edges_source ON edges(source_id, type)",
        "CREATE INDEX idx_edges_target ON edges(target_id, type)",
        "CREATE INDEX idx_edges_type ON edges(type)",
        
        // Property indexes (property-first for efficient queries)
        "CREATE INDEX idx_node_props_int_key_value ON node_props_int(key_id, value, node_id)",
        "CREATE INDEX idx_node_props_text_key_value ON node_props_text(key_id, value, node_id)",
        "CREATE INDEX idx_node_props_real_key_value ON node_props_real(key_id, value, node_id)",
        "CREATE INDEX idx_node_props_bool_key_value ON node_props_bool(key_id, value, node_id)",
        
        "CREATE INDEX idx_edge_props_int_key_value ON edge_props_int(key_id, value, edge_id)",
        "CREATE INDEX idx_edge_props_text_key_value ON edge_props_text(key_id, value, edge_id)",
        "CREATE INDEX idx_edge_props_real_key_value ON edge_props_real(key_id, value, edge_id)",
        "CREATE INDEX idx_edge_props_bool_key_value ON edge_props_bool(key_id, value, edge_id)",
        
        // Label indexes
        "CREATE INDEX idx_node_labels_label ON node_labels(label, node_id)",
        
        // Property key index
        "CREATE INDEX idx_property_keys_key ON property_keys(key)",
        
        NULL  // Sentinel
    };
    
    // Create indexes
    for (int i = 0; index_sql[i] != NULL; i++) {
        rc = sqlite3_exec(db, index_sql[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) {
                sqlite3_free(err_msg);
            }
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
            return GRAPHQLITE_ERROR;
        }
    }
    
    // Commit transaction
    rc = sqlite3_exec(db, "COMMIT", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
        return GRAPHQLITE_ERROR;
    }
    
    // Re-enable foreign keys
    sqlite3_exec(db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    
    return GRAPHQLITE_OK;
}

// ============================================================================
// Helper Functions for AST Property Extraction
// ============================================================================

// Extract property value and type from AST literal node
static int extract_property_from_ast(cypher_ast_node_t *value_node, const char **prop_value_str, 
                                     double *prop_value_num, int *prop_value_int, 
                                     graphqlite_value_type_t *prop_type) {
    if (!value_node || !prop_type) return 0;
    
    switch (value_node->type) {
        case AST_STRING_LITERAL:
            if (prop_value_str) {
                *prop_value_str = value_node->data.string_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_TEXT;
            return 1;
            
        case AST_INTEGER_LITERAL:
            if (prop_value_int) {
                *prop_value_int = value_node->data.integer_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_INTEGER;
            return 1;
            
        case AST_FLOAT_LITERAL:
            if (prop_value_num) {
                *prop_value_num = value_node->data.float_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_FLOAT;
            return 1;
            
        case AST_BOOLEAN_LITERAL:
            if (prop_value_int) {
                *prop_value_int = value_node->data.boolean_literal.value;
            }
            *prop_type = GRAPHQLITE_TYPE_BOOLEAN;
            return 1;
            
        default:
            *prop_type = GRAPHQLITE_TYPE_NULL;
            return 0;
    }
}

// ============================================================================
// Helper Functions for Typed EAV Schema
// ============================================================================

// Get or create property key ID (property key interning)
static int64_t get_or_create_property_key_id(sqlite3 *db, const char *key) {
    if (!key) return -1;
    
    // First, try to find existing key
    const char *select_sql = "SELECT id FROM property_keys WHERE key = ?";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    
    int64_t key_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        key_id = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    
    if (key_id != -1) {
        return key_id;  // Found existing key
    }
    
    // Key doesn't exist, create it
    const char *insert_sql = "INSERT INTO property_keys (key) VALUES (?)";
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) return -1;
    
    return sqlite3_last_insert_rowid(db);
}

// Insert property based on type
static int insert_node_property(sqlite3 *db, int64_t node_id, int64_t key_id, 
                               const char *value, graphqlite_value_type_t type) {
    const char *table_name;
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER:
            table_name = "node_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            table_name = "node_props_real";
            break;
        case GRAPHQLITE_TYPE_TEXT:
            table_name = "node_props_text";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            table_name = "node_props_bool";
            break;
        default:
            return GRAPHQLITE_INVALID;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO %s (node_id, key_id, value) VALUES (?, ?, ?)", table_name);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return GRAPHQLITE_ERROR;
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_int64(stmt, 2, key_id);
    
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER: {
            int64_t int_val = atoll(value);
            sqlite3_bind_int64(stmt, 3, int_val);
            break;
        }
        case GRAPHQLITE_TYPE_FLOAT: {
            double float_val = atof(value);
            sqlite3_bind_double(stmt, 3, float_val);
            break;
        }
        case GRAPHQLITE_TYPE_TEXT:
            sqlite3_bind_text(stmt, 3, value, -1, SQLITE_STATIC);
            break;
        case GRAPHQLITE_TYPE_BOOLEAN: {
            int bool_val = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
            sqlite3_bind_int(stmt, 3, bool_val);
            break;
        }
        default:
            sqlite3_finalize(stmt);
            return GRAPHQLITE_INVALID;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? GRAPHQLITE_OK : GRAPHQLITE_ERROR;
}

// ============================================================================
// Query Execution
// ============================================================================

static graphqlite_result_t* execute_create_statement(sqlite3 *db, cypher_ast_node_t *ast) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract node pattern from CREATE statement
    cypher_ast_node_t *pattern = ast->data.create_stmt.node_pattern;
    if (!pattern || pattern->type != AST_NODE_PATTERN) {
        graphqlite_result_set_error(result, "Invalid node pattern in CREATE statement");
        return result;
    }
    
    // Extract label
    const char *label = NULL;
    if (pattern->data.node_pattern.label && pattern->data.node_pattern.label->type == AST_LABEL) {
        label = pattern->data.node_pattern.label->data.label.name;
    }
    
    if (!label) {
        graphqlite_result_set_error(result, "Node must have a label");
        return result;
    }
    
    // Step 1: Insert node into nodes table
    const char *insert_node_sql = "INSERT INTO nodes DEFAULT VALUES";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, insert_node_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare node INSERT statement");
        return result;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert node");
        return result;
    }
    
    int64_t node_id = sqlite3_last_insert_rowid(db);
    
    // Step 2: Insert label into node_labels table
    const char *insert_label_sql = "INSERT INTO node_labels (node_id, label) VALUES (?, ?)";
    rc = sqlite3_prepare_v2(db, insert_label_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        graphqlite_result_set_error(result, "Failed to prepare label INSERT statement");
        return result;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to insert node label");
        return result;
    }
    
    // Step 3: Insert properties (if any) into appropriate typed tables
    if (pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = pattern->data.node_pattern.properties;
        
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
                        return result;
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

static graphqlite_result_t* execute_match_statement(sqlite3 *db, cypher_ast_node_t *match_stmt, cypher_ast_node_t *return_stmt) {
    graphqlite_result_t *result = graphqlite_result_create();
    if (!result) return NULL;
    
    // Extract node pattern from MATCH statement
    cypher_ast_node_t *pattern = match_stmt->data.match_stmt.node_pattern;
    if (!pattern || pattern->type != AST_NODE_PATTERN) {
        graphqlite_result_set_error(result, "Invalid node pattern in MATCH statement");
        return result;
    }
    
    // Build SELECT query based on pattern
    const char *label = NULL;
    const char *prop_key = NULL;
    const char *prop_value_str = NULL;
    double prop_value_num = 0.0;
    int prop_value_int = 0;
    graphqlite_value_type_t prop_type = GRAPHQLITE_TYPE_NULL;
    
    if (pattern->data.node_pattern.label && pattern->data.node_pattern.label->type == AST_LABEL) {
        label = pattern->data.node_pattern.label->data.label.name;
    }
    
    if (pattern->data.node_pattern.properties) {
        cypher_ast_node_t *props = pattern->data.node_pattern.properties;
        
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
    
    // Set up result columns
    graphqlite_result_add_column(result, "id", GRAPHQLITE_TYPE_INTEGER);
    graphqlite_result_add_column(result, "label", GRAPHQLITE_TYPE_TEXT);
    graphqlite_result_add_column(result, "property_key", GRAPHQLITE_TYPE_TEXT);
    graphqlite_result_add_column(result, "property_value", GRAPHQLITE_TYPE_TEXT);
    
    // Fetch results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        graphqlite_result_add_row(result);
        size_t row_idx = result->row_count - 1;
        
        // Set values
        graphqlite_value_t value;
        
        // ID
        value.type = GRAPHQLITE_TYPE_INTEGER;
        value.data.integer = sqlite3_column_int64(stmt, 0);
        graphqlite_result_set_value(result, row_idx, 0, &value);
        
        // Label
        value.type = GRAPHQLITE_TYPE_TEXT;
        const char *text = (const char*)sqlite3_column_text(stmt, 1);
        value.data.text = text ? strdup(text) : NULL;
        graphqlite_result_set_value(result, row_idx, 1, &value);
        
        // Property key
        value.type = GRAPHQLITE_TYPE_TEXT;
        text = (const char*)sqlite3_column_text(stmt, 2);
        value.data.text = text ? strdup(text) : NULL;
        graphqlite_result_set_value(result, row_idx, 2, &value);
        
        // Property value
        value.type = GRAPHQLITE_TYPE_TEXT;
        text = (const char*)sqlite3_column_text(stmt, 3);
        value.data.text = text ? strdup(text) : NULL;
        graphqlite_result_set_value(result, row_idx, 3, &value);
    }
    
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        graphqlite_result_set_error(result, "Failed to execute SELECT statement");
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
    
    // For now, just return success message
    // TODO: Format results properly for SQLite return
    if (result->row_count > 0) {
        char success_msg[128];
        snprintf(success_msg, sizeof(success_msg), "Query executed successfully, %zu rows returned", result->row_count);
        sqlite3_result_text(context, success_msg, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_result_text(context, "Query executed successfully", -1, SQLITE_STATIC);
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