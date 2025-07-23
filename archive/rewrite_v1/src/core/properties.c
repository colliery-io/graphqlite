#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Property Management Implementation
// ============================================================================

int graphqlite_set_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value) {
    if (!db || !key || !value || entity_id <= 0) {
        return SQLITE_ERROR;
    }
    
    // Validate property value
    if (!validate_property_value(value)) {
        return SQLITE_ERROR;
    }
    
    // Intern property key
    int key_id = intern_property_key(db->key_cache, key);
    if (key_id < 0) {
        return SQLITE_ERROR;
    }
    
    // Select appropriate prepared statement based on type and entity
    sqlite3_stmt *stmt;
    statement_type_t stmt_type;
    
    if (entity_type == ENTITY_NODE) {
        switch (value->type) {
            case PROP_INT:
                stmt_type = STMT_SET_NODE_PROP_INT;
                break;
            case PROP_TEXT:
                stmt_type = STMT_SET_NODE_PROP_TEXT;
                break;
            case PROP_REAL:
                stmt_type = STMT_SET_NODE_PROP_REAL;
                break;
            case PROP_BOOL:
                stmt_type = STMT_SET_NODE_PROP_BOOL;
                break;
            case PROP_NULL:
                // Delete property if setting to NULL
                return graphqlite_delete_property(db, entity_type, entity_id, key);
            default:
                return SQLITE_ERROR;
        }
    } else if (entity_type == ENTITY_EDGE) {
        switch (value->type) {
            case PROP_INT:
                stmt_type = STMT_SET_EDGE_PROP_INT;
                break;
            case PROP_TEXT:
                stmt_type = STMT_SET_EDGE_PROP_TEXT;
                break;
            case PROP_REAL:
                stmt_type = STMT_SET_EDGE_PROP_REAL;
                break;
            case PROP_BOOL:
                stmt_type = STMT_SET_EDGE_PROP_BOOL;
                break;
            case PROP_NULL:
                return graphqlite_delete_property(db, entity_type, entity_id, key);
            default:
                return SQLITE_ERROR;
        }
    } else {
        return SQLITE_ERROR;
    }
    
    stmt = get_prepared_statement(db, stmt_type);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    // Bind parameters
    sqlite3_bind_int64(stmt, 1, entity_id);
    sqlite3_bind_int(stmt, 2, key_id);
    
    switch (value->type) {
        case PROP_INT:
            sqlite3_bind_int64(stmt, 3, value->value.int_val);
            break;
        case PROP_TEXT:
            sqlite3_bind_text(stmt, 3, value->value.text_val, -1, SQLITE_STATIC);
            break;
        case PROP_REAL:
            sqlite3_bind_double(stmt, 3, value->value.real_val);
            break;
        case PROP_BOOL:
            sqlite3_bind_int(stmt, 3, value->value.bool_val ? 1 : 0);
            break;
        default:
            sqlite3_reset(stmt);
            return SQLITE_ERROR;
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

int graphqlite_get_property(graphqlite_db_t *db, entity_type_t entity_type,
                           int64_t entity_id, const char *key, property_value_t *value) {
    if (!db || !key || !value || entity_id <= 0) {
        return SQLITE_ERROR;
    }
    
    // Initialize value
    memset(value, 0, sizeof(property_value_t));
    
    // Lookup property key
    int key_id = lookup_property_key(db->key_cache, key);
    if (key_id < 0) {
        value->type = PROP_NULL;
        return SQLITE_OK; // Property doesn't exist, return NULL
    }
    
    // Try each property type to find the value
    property_type_t types[] = {PROP_INT, PROP_TEXT, PROP_REAL, PROP_BOOL};
    statement_type_t stmt_types_node[] = {
        STMT_GET_NODE_PROP_INT, STMT_GET_NODE_PROP_TEXT, 
        STMT_GET_NODE_PROP_REAL, STMT_GET_NODE_PROP_BOOL
    };
    statement_type_t stmt_types_edge[] = {
        STMT_GET_EDGE_PROP_INT, STMT_GET_EDGE_PROP_TEXT,
        STMT_GET_EDGE_PROP_REAL, STMT_GET_EDGE_PROP_BOOL
    };
    
    statement_type_t *stmt_types = (entity_type == ENTITY_NODE) ? 
                                  stmt_types_node : stmt_types_edge;
    
    for (int i = 0; i < 4; i++) {
        sqlite3_stmt *stmt = get_prepared_statement(db, stmt_types[i]);
        if (!stmt) {
            continue;
        }
        
        sqlite3_bind_int64(stmt, 1, entity_id);
        sqlite3_bind_int(stmt, 2, key_id);
        
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            value->type = types[i];
            
            switch (types[i]) {
                case PROP_INT:
                    value->value.int_val = sqlite3_column_int64(stmt, 0);
                    break;
                case PROP_TEXT: {
                    const char *text = (const char*)sqlite3_column_text(stmt, 0);
                    value->value.text_val = strdup(text ? text : "");
                    break;
                }
                case PROP_REAL:
                    value->value.real_val = sqlite3_column_double(stmt, 0);
                    break;
                case PROP_BOOL:
                    value->value.bool_val = sqlite3_column_int(stmt, 0);
                    break;
            }
            
            sqlite3_reset(stmt);
            return SQLITE_OK;
        }
        
        sqlite3_reset(stmt);
    }
    
    // Property not found
    value->type = PROP_NULL;
    return SQLITE_OK;
}

int graphqlite_delete_property(graphqlite_db_t *db, entity_type_t entity_type,
                              int64_t entity_id, const char *key) {
    if (!db || !key || entity_id <= 0) {
        return SQLITE_ERROR;
    }
    
    // Lookup property key
    int key_id = lookup_property_key(db->key_cache, key);
    if (key_id < 0) {
        return SQLITE_OK; // Property doesn't exist, nothing to delete
    }
    
    // Delete from all property type tables
    statement_type_t stmt_types_node[] = {
        STMT_DEL_NODE_PROP_INT, STMT_DEL_NODE_PROP_TEXT,
        STMT_DEL_NODE_PROP_REAL, STMT_DEL_NODE_PROP_BOOL
    };
    statement_type_t stmt_types_edge[] = {
        STMT_DEL_EDGE_PROP_INT, STMT_DEL_EDGE_PROP_TEXT,
        STMT_DEL_EDGE_PROP_REAL, STMT_DEL_EDGE_PROP_BOOL
    };
    
    statement_type_t *stmt_types = (entity_type == ENTITY_NODE) ?
                                  stmt_types_node : stmt_types_edge;
    
    int deleted = 0;
    for (int i = 0; i < 4; i++) {
        sqlite3_stmt *stmt = get_prepared_statement(db, stmt_types[i]);
        if (!stmt) {
            continue;
        }
        
        sqlite3_bind_int64(stmt, 1, entity_id);
        sqlite3_bind_int(stmt, 2, key_id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        
        if (rc == SQLITE_DONE && sqlite3_changes(db->sqlite_db) > 0) {
            deleted = 1;
        }
    }
    
    return deleted ? SQLITE_OK : SQLITE_ERROR;
}

// ============================================================================
// Batch Property Operations
// ============================================================================

int graphqlite_set_properties(graphqlite_db_t *db, entity_type_t entity_type,
                             int64_t entity_id, property_set_t *properties) {
    if (!db || !properties || entity_id <= 0) {
        return SQLITE_ERROR;
    }
    
    // Use transaction for batch operation
    bool started_transaction = false;
    if (!graphqlite_in_transaction(db)) {
        int rc = graphqlite_begin_transaction(db);
        if (rc != SQLITE_OK) {
            return rc;
        }
        started_transaction = true;
    }
    
    for (size_t i = 0; i < properties->count; i++) {
        property_pair_t *prop = &properties->properties[i];
        int rc = graphqlite_set_property(db, entity_type, entity_id, 
                                        prop->key, &prop->value);
        if (rc != SQLITE_OK) {
            if (started_transaction) {
                graphqlite_rollback_transaction(db);
            }
            return rc;
        }
    }
    
    if (started_transaction) {
        return graphqlite_commit_transaction(db);
    }
    
    return SQLITE_OK;
}

property_set_t* graphqlite_get_all_properties(graphqlite_db_t *db, entity_type_t entity_type,
                                             int64_t entity_id) {
    if (!db || entity_id <= 0) {
        return NULL;
    }
    
    property_set_t *prop_set = create_property_set();
    if (!prop_set) {
        return NULL;
    }
    
    // Query each property type table
    const char *table_names[] = {"int", "text", "real", "bool"};
    property_type_t types[] = {PROP_INT, PROP_TEXT, PROP_REAL, PROP_BOOL};
    
    const char *entity_table_prefix = (entity_type == ENTITY_NODE) ? "node_props" : "edge_props";
    
    for (int i = 0; i < 4; i++) {
        char sql[512];
        snprintf(sql, sizeof(sql),
                "SELECT pk.key, p.value FROM %s_%s p "
                "JOIN property_keys pk ON p.key_id = pk.id "
                "WHERE p.%s_id = ?",
                entity_table_prefix, table_names[i],
                (entity_type == ENTITY_NODE) ? "node" : "edge");
        
        sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
        if (!stmt) {
            continue;
        }
        
        sqlite3_bind_int64(stmt, 1, entity_id);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *key = (const char*)sqlite3_column_text(stmt, 0);
            
            property_value_t value;
            value.type = types[i];
            
            switch (types[i]) {
                case PROP_INT:
                    value.value.int_val = sqlite3_column_int64(stmt, 1);
                    break;
                case PROP_TEXT: {
                    const char *text = (const char*)sqlite3_column_text(stmt, 1);
                    value.value.text_val = strdup(text ? text : "");
                    break;
                }
                case PROP_REAL:
                    value.value.real_val = sqlite3_column_double(stmt, 1);
                    break;
                case PROP_BOOL:
                    value.value.bool_val = sqlite3_column_int(stmt, 1);
                    break;
            }
            
            if (add_property_to_set(prop_set, key, &value) != 0) {
                // Cleanup on failure
                if (types[i] == PROP_TEXT) {
                    free(value.value.text_val);
                }
                free_property_set(prop_set);
                sqlite3_reset(stmt);
                return NULL;
            }
        }
        
        sqlite3_reset(stmt);
    }
    
    return prop_set;
}

// ============================================================================
// Property Query Operations
// ============================================================================

int64_t* graphqlite_find_entities_by_property(graphqlite_db_t *db, entity_type_t entity_type,
                                             const char *key, property_value_t *value,
                                             size_t *count) {
    if (!db || !key || !value || !count) {
        return NULL;
    }
    
    *count = 0;
    
    // Lookup property key
    int key_id = lookup_property_key(db->key_cache, key);
    if (key_id < 0) {
        return NULL; // Key doesn't exist
    }
    
    // Build query based on property type and entity type
    const char *table_prefix = (entity_type == ENTITY_NODE) ? "node_props" : "edge_props";
    const char *entity_column = (entity_type == ENTITY_NODE) ? "node_id" : "edge_id";
    const char *type_suffix;
    
    switch (value->type) {
        case PROP_INT:
            type_suffix = "int";
            break;
        case PROP_TEXT:
            type_suffix = "text";
            break;
        case PROP_REAL:
            type_suffix = "real";
            break;
        case PROP_BOOL:
            type_suffix = "bool";
            break;
        default:
            return NULL;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql),
            "SELECT %s FROM %s_%s WHERE key_id = ? AND value = ?",
            entity_column, table_prefix, type_suffix);
    
    sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, key_id);
    
    switch (value->type) {
        case PROP_INT:
            sqlite3_bind_int64(stmt, 2, value->value.int_val);
            break;
        case PROP_TEXT:
            sqlite3_bind_text(stmt, 2, value->value.text_val, -1, SQLITE_STATIC);
            break;
        case PROP_REAL:
            sqlite3_bind_double(stmt, 2, value->value.real_val);
            break;
        case PROP_BOOL:
            sqlite3_bind_int(stmt, 2, value->value.bool_val ? 1 : 0);
            break;
    }
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    sqlite3_reset(stmt);
    
    if (result_count == 0) {
        return NULL;
    }
    
    // Allocate result array
    int64_t *results = malloc(result_count * sizeof(int64_t));
    if (!results) {
        return NULL;
    }
    
    // Second pass: collect results
    sqlite3_bind_int(stmt, 1, key_id);
    switch (value->type) {
        case PROP_INT:
            sqlite3_bind_int64(stmt, 2, value->value.int_val);
            break;
        case PROP_TEXT:
            sqlite3_bind_text(stmt, 2, value->value.text_val, -1, SQLITE_STATIC);
            break;
        case PROP_REAL:
            sqlite3_bind_double(stmt, 2, value->value.real_val);
            break;
        case PROP_BOOL:
            sqlite3_bind_int(stmt, 2, value->value.bool_val ? 1 : 0);
            break;
    }
    
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < result_count) {
        results[index++] = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_reset(stmt);
    
    *count = result_count;
    return results;
}

// ============================================================================
// Convenience Functions for Common Types
// ============================================================================

int graphqlite_set_int_property(graphqlite_db_t *db, entity_type_t entity_type,
                               int64_t entity_id, const char *key, int64_t value) {
    property_value_t prop = {.type = PROP_INT, .value = {.int_val = value}};
    return graphqlite_set_property(db, entity_type, entity_id, key, &prop);
}

int graphqlite_set_text_property(graphqlite_db_t *db, entity_type_t entity_type,
                                int64_t entity_id, const char *key, const char *value) {
    if (!value) {
        return SQLITE_ERROR;
    }
    property_value_t prop = {.type = PROP_TEXT, .value = {.text_val = (char*)value}};
    return graphqlite_set_property(db, entity_type, entity_id, key, &prop);
}

int graphqlite_set_real_property(graphqlite_db_t *db, entity_type_t entity_type,
                                int64_t entity_id, const char *key, double value) {
    property_value_t prop = {.type = PROP_REAL, .value = {.real_val = value}};
    return graphqlite_set_property(db, entity_type, entity_id, key, &prop);
}

int graphqlite_set_bool_property(graphqlite_db_t *db, entity_type_t entity_type,
                                int64_t entity_id, const char *key, bool value) {
    property_value_t prop = {.type = PROP_BOOL, .value = {.bool_val = value ? 1 : 0}};
    return graphqlite_set_property(db, entity_type, entity_id, key, &prop);
}

// ============================================================================
// Property Value Cleanup
// ============================================================================

void free_property_value(property_value_t *value) {
    if (!value) {
        return;
    }
    
    if (value->type == PROP_TEXT && value->value.text_val) {
        free(value->value.text_val);
        value->value.text_val = NULL;
    }
    
    value->type = PROP_NULL;
}