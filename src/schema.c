/* GraphQLite Schema Management Module */
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "graphqlite.h"

int create_schema(sqlite3 *db) {
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