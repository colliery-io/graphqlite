/* GraphQLite Property System Module */
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "property.h"
#include "graphqlite.h"
#include "ast.h"

int extract_property_from_ast(cypher_ast_node_t *value_node, const char **prop_value_str, 
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

int64_t get_or_create_property_key_id(sqlite3 *db, const char *key) {
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

int insert_node_property(sqlite3 *db, int64_t node_id, int64_t key_id, 
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

int insert_edge_property(sqlite3 *db, int64_t edge_id, int64_t key_id, 
                        const char *value, graphqlite_value_type_t type) {
    const char *table_name;
    switch (type) {
        case GRAPHQLITE_TYPE_INTEGER:
            table_name = "edge_props_int";
            break;
        case GRAPHQLITE_TYPE_FLOAT:
            table_name = "edge_props_real";
            break;
        case GRAPHQLITE_TYPE_TEXT:
            table_name = "edge_props_text";
            break;
        case GRAPHQLITE_TYPE_BOOLEAN:
            table_name = "edge_props_bool";
            break;
        default:
            return GRAPHQLITE_INVALID;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), 
        "INSERT INTO %s (edge_id, key_id, value) VALUES (?, ?, ?)", table_name);
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return GRAPHQLITE_ERROR;
    
    sqlite3_bind_int64(stmt, 1, edge_id);
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