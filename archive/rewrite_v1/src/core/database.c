#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Database Schema SQL
// ============================================================================

static const char *CREATE_SCHEMA_SQL[] = {
    // Core tables
    "CREATE TABLE IF NOT EXISTS nodes ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT"
    ")",
    
    "CREATE TABLE IF NOT EXISTS edges ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  type TEXT NOT NULL"
    ")",
    
    "CREATE TABLE IF NOT EXISTS property_keys ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  key TEXT UNIQUE NOT NULL"
    ")",
    
    // Node property tables (typed EAV)
    "CREATE TABLE IF NOT EXISTS node_props_int ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL,"
    "  PRIMARY KEY (node_id, key_id)"
    ")",
    
    "CREATE TABLE IF NOT EXISTS node_props_text ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value TEXT NOT NULL,"
    "  PRIMARY KEY (node_id, key_id)"
    ")",
    
    "CREATE TABLE IF NOT EXISTS node_props_real ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value REAL NOT NULL,"
    "  PRIMARY KEY (node_id, key_id)"
    ")",
    
    "CREATE TABLE IF NOT EXISTS node_props_bool ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
    "  PRIMARY KEY (node_id, key_id)"
    ")",
    
    // Edge property tables (typed EAV)
    "CREATE TABLE IF NOT EXISTS edge_props_int ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL,"
    "  PRIMARY KEY (edge_id, key_id)"
    ")",
    
    "CREATE TABLE IF NOT EXISTS edge_props_text ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value TEXT NOT NULL,"
    "  PRIMARY KEY (edge_id, key_id)"
    ")",
    
    "CREATE TABLE IF NOT EXISTS edge_props_real ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value REAL NOT NULL,"
    "  PRIMARY KEY (edge_id, key_id)"
    ")",
    
    "CREATE TABLE IF NOT EXISTS edge_props_bool ("
    "  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,"
    "  key_id INTEGER NOT NULL REFERENCES property_keys(id),"
    "  value INTEGER NOT NULL CHECK (value IN (0, 1)),"
    "  PRIMARY KEY (edge_id, key_id)"
    ")",
    
    // Node labels table
    "CREATE TABLE IF NOT EXISTS node_labels ("
    "  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,"
    "  label TEXT NOT NULL,"
    "  PRIMARY KEY (node_id, label)"
    ")",
    
    NULL  // Sentinel
};

static const char *CREATE_INDEXES_SQL[] = {
    // Core indexes for performance
    "CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id, type)",
    "CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id, type)",
    "CREATE INDEX IF NOT EXISTS idx_edges_type ON edges(type)",
    
    // Property indexes (property-first for efficient queries)
    "CREATE INDEX IF NOT EXISTS idx_node_props_int_key_value ON node_props_int(key_id, value, node_id)",
    "CREATE INDEX IF NOT EXISTS idx_node_props_text_key_value ON node_props_text(key_id, value, node_id)",
    "CREATE INDEX IF NOT EXISTS idx_node_props_real_key_value ON node_props_real(key_id, value, node_id)",
    "CREATE INDEX IF NOT EXISTS idx_node_props_bool_key_value ON node_props_bool(key_id, value, node_id)",
    
    "CREATE INDEX IF NOT EXISTS idx_edge_props_int_key_value ON edge_props_int(key_id, value, edge_id)",
    "CREATE INDEX IF NOT EXISTS idx_edge_props_text_key_value ON edge_props_text(key_id, value, edge_id)",
    "CREATE INDEX IF NOT EXISTS idx_edge_props_real_key_value ON edge_props_real(key_id, value, edge_id)",
    "CREATE INDEX IF NOT EXISTS idx_edge_props_bool_key_value ON edge_props_bool(key_id, value, edge_id)",
    
    // Label indexes
    "CREATE INDEX IF NOT EXISTS idx_node_labels_label ON node_labels(label, node_id)",
    
    // Property key index
    "CREATE INDEX IF NOT EXISTS idx_property_keys_key ON property_keys(key)",
    
    NULL  // Sentinel
};

// ============================================================================
// Database Lifecycle Functions
// ============================================================================

graphqlite_t* graphqlite_open(const char *path, int flags) {
    if (!path) {
        return NULL;
    }
    
    // For now, ignore flags and use default behavior
    
    graphqlite_db_t *db = malloc(sizeof(graphqlite_db_t));
    if (!db) {
        return NULL;
    }
    
    // Initialize structure
    memset(db, 0, sizeof(graphqlite_db_t));
    
    // Store database path
    db->db_path = strdup(path);
    if (!db->db_path) {
        free(db);
        return NULL;
    }
    
    // Open SQLite database
    int rc = sqlite3_open(path, &db->sqlite_db);
    if (rc != SQLITE_OK) {
        free(db->db_path);
        free(db);
        return NULL;
    }
    
    // Enable foreign keys
    rc = sqlite3_exec(db->sqlite_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db->sqlite_db);
        free(db->db_path);
        free(db);
        return NULL;
    }
    
    // Create schema
    rc = graphqlite_create_schema(db);
    if (rc != SQLITE_OK) {
        sqlite3_close(db->sqlite_db);
        free(db->db_path);
        free(db);
        return NULL;
    }
    
    // Initialize property key cache
    db->key_cache = create_property_key_cache(db->sqlite_db);
    if (!db->key_cache) {
        sqlite3_close(db->sqlite_db);
        free(db->db_path);
        free(db);
        return NULL;
    }
    
    // Initialize statement manager
    rc = initialize_statement_manager(db);
    if (rc != SQLITE_OK) {
        destroy_property_key_cache(db->key_cache);
        sqlite3_close(db->sqlite_db);
        free(db->db_path);
        free(db);
        return NULL;
    }
    
    // Initialize transaction state
    db->tx_state.state = TX_STATE_NONE;
    db->tx_state.nesting_level = 0;
    db->tx_state.auto_transaction = false;
    db->tx_state.savepoint_name = NULL;
    pthread_mutex_init(&db->tx_state.tx_mutex, NULL);
    
    // Initialize mode manager
    db->mode_manager.current_mode = GRAPHQLITE_MODE_INTERACTIVE;
    db->mode_manager.previous_mode = GRAPHQLITE_MODE_INTERACTIVE;
    db->mode_manager.transition_in_progress = false;
    pthread_mutex_init(&db->mode_manager.mode_mutex, NULL);
    
    // Initialize operations mutex
    pthread_mutex_init(&db->operations_mutex, NULL);
    
    // Set up default interactive mode
    rc = graphqlite_switch_to_interactive_mode(db);
    if (rc != SQLITE_OK) {
        // Cleanup on failure
        pthread_mutex_destroy(&db->operations_mutex);
        pthread_mutex_destroy(&db->mode_manager.mode_mutex);
        pthread_mutex_destroy(&db->tx_state.tx_mutex);
        cleanup_statement_manager(db);
        destroy_property_key_cache(db->key_cache);
        sqlite3_close(db->sqlite_db);
        free(db->db_path);
        free(db);
        return NULL;
    }
    
    db->is_open = true;
    return db;
}

int graphqlite_close(graphqlite_t *db) {
    graphqlite_db_t *internal_db = (graphqlite_db_t*)db;
    if (!internal_db || !internal_db->is_open) {
        return SQLITE_ERROR;
    }
    
    // Ensure no active transactions
    if (graphqlite_in_transaction(internal_db)) {
        graphqlite_rollback_transaction(internal_db);
    }
    
    // Cleanup components
    cleanup_statement_manager(internal_db);
    destroy_property_key_cache(internal_db->key_cache);
    
    // Cleanup mutexes
    pthread_mutex_destroy(&internal_db->operations_mutex);
    pthread_mutex_destroy(&internal_db->mode_manager.mode_mutex);
    pthread_mutex_destroy(&internal_db->tx_state.tx_mutex);
    
    // Close SQLite database
    sqlite3_close(internal_db->sqlite_db);
    
    // Free memory
    free(internal_db->db_path);
    free(internal_db->last_error_message);
    free(internal_db->tx_state.savepoint_name);
    free(internal_db->mode_manager.saved_pragma_state);
    
    memset(internal_db, 0, sizeof(graphqlite_db_t));
    free(internal_db);
    
    return SQLITE_OK;
}

int graphqlite_create_schema(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
    char *error_msg = NULL;
    int rc;
    
    // Create tables
    for (int i = 0; CREATE_SCHEMA_SQL[i] != NULL; i++) {
        rc = sqlite3_exec(db->sqlite_db, CREATE_SCHEMA_SQL[i], NULL, NULL, &error_msg);
        if (rc != SQLITE_OK) {
            if (error_msg) {
                // Store error message
                free(db->last_error_message);
                db->last_error_message = strdup(error_msg);
                sqlite3_free(error_msg);
            }
            db->last_error_code = rc;
            return rc;
        }
    }
    
    // Create indexes
    for (int i = 0; CREATE_INDEXES_SQL[i] != NULL; i++) {
        rc = sqlite3_exec(db->sqlite_db, CREATE_INDEXES_SQL[i], NULL, NULL, &error_msg);
        if (rc != SQLITE_OK) {
            if (error_msg) {
                free(db->last_error_message);
                db->last_error_message = strdup(error_msg);
                sqlite3_free(error_msg);
            }
            db->last_error_code = rc;
            return rc;
        }
    }
    
    return SQLITE_OK;
}

// ============================================================================
// Error Handling
// ============================================================================

const char* graphqlite_error_message(graphqlite_db_t *db) {
    if (!db) {
        return "Invalid database handle";
    }
    
    if (db->last_error_message) {
        return db->last_error_message;
    }
    
    if (db->sqlite_db) {
        return sqlite3_errmsg(db->sqlite_db);
    }
    
    return "Unknown error";
}

int graphqlite_error_code(graphqlite_db_t *db) {
    if (!db) {
        return SQLITE_ERROR;
    }
    
    if (db->last_error_code) {
        return db->last_error_code;
    }
    
    if (db->sqlite_db) {
        return sqlite3_errcode(db->sqlite_db);
    }
    
    return SQLITE_ERROR;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* graphqlite_version(void) {
    return "1.0.0-alpha";
}