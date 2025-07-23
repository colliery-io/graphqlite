#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Mode Management Implementation
// ============================================================================

int graphqlite_switch_to_interactive_mode(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
    // Set default interactive mode configuration
    interactive_mode_config_t config = {
        .synchronous_mode = true,
        .foreign_keys = true,
        .journal_mode_wal = true,
        .cache_size = 2000,
        .page_size = 4096,
        .temp_store_memory = true,
        .auto_commit = true,
        .lock_timeout = 5000,  // 5 second timeout
        .max_connections = 10,
        .read_uncommitted = false
    };
    
    return apply_interactive_mode_config(db, &config);
}

int apply_interactive_mode_config(graphqlite_db_t *db, interactive_mode_config_t *config) {
    if (!db || !config) {
        return SQLITE_ERROR;
    }
    
    int rc;
    
    // ACID guarantees
    if (config->synchronous_mode) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA synchronous = NORMAL", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    if (config->foreign_keys) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Concurrency optimization with WAL mode
    if (config->journal_mode_wal) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA journal_mode = WAL", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Performance tuning
    char sql[256];
    snprintf(sql, sizeof(sql), "PRAGMA cache_size = %d", config->cache_size);
    rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    snprintf(sql, sizeof(sql), "PRAGMA page_size = %d", config->page_size);
    rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    if (config->temp_store_memory) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA temp_store = MEMORY", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Set busy timeout
    rc = sqlite3_busy_timeout(db->sqlite_db, config->lock_timeout);
    if (rc != SQLITE_OK) return rc;
    
    // Update mode manager
    db->mode_manager.current_mode = GRAPHQLITE_MODE_INTERACTIVE;
    db->mode_manager.interactive_config = *config;
    
    return SQLITE_OK;
}

int graphqlite_switch_to_bulk_import_mode(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
    // Set aggressive performance configuration
    bulk_import_config_t config = {
        .synchronous_off = true,
        .journal_mode_memory = true,
        .temp_store_memory = true,
        .large_cache_size = 100000,     // 100MB cache
        .large_page_size = 65536,       // 64KB pages
        .defer_foreign_keys = true,
        .defer_index_updates = true,
        .batch_size = 10000,            // 10K operations per transaction
        .memory_limit = 500 * 1024 * 1024, // 500MB memory limit
        .integrity_check_on_complete = true,
        .auto_analyze_on_complete = true
    };
    
    return apply_bulk_import_config(db, &config);
}

int apply_bulk_import_config(graphqlite_db_t *db, bulk_import_config_t *config) {
    if (!db || !config) {
        return SQLITE_ERROR;
    }
    
    int rc;
    
    // Disable synchronous writes for maximum speed
    if (config->synchronous_off) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Use memory journal for faster writes
    if (config->journal_mode_memory) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA journal_mode = MEMORY", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Large cache for better performance
    char sql[256];
    snprintf(sql, sizeof(sql), "PRAGMA cache_size = %d", config->large_cache_size);
    rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    snprintf(sql, sizeof(sql), "PRAGMA page_size = %d", config->large_page_size);
    rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Defer foreign key checks
    if (config->defer_foreign_keys) {
        rc = sqlite3_exec(db->sqlite_db, "PRAGMA defer_foreign_keys = ON", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Update mode manager and bulk state
    db->mode_manager.current_mode = GRAPHQLITE_MODE_BULK_IMPORT;
    db->bulk_config = *config;
    
    // Initialize bulk import state
    memset(&db->bulk_state, 0, sizeof(bulk_import_state_t));
    db->bulk_state.memory_limit = config->memory_limit;
    db->bulk_state.transaction_limit = config->batch_size;
    
    return SQLITE_OK;
}

int graphqlite_switch_to_readonly_mode(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
    int rc;
    
    // Optimize for read performance
    rc = sqlite3_exec(db->sqlite_db, "PRAGMA query_only = ON", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Large cache for read operations
    rc = sqlite3_exec(db->sqlite_db, "PRAGMA cache_size = 50000", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    db->mode_manager.current_mode = GRAPHQLITE_MODE_READONLY;
    
    return SQLITE_OK;
}

int graphqlite_switch_to_maintenance_mode(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
    int rc;
    
    // Ensure data integrity for maintenance operations
    rc = sqlite3_exec(db->sqlite_db, "PRAGMA synchronous = FULL", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    rc = sqlite3_exec(db->sqlite_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    db->mode_manager.current_mode = GRAPHQLITE_MODE_MAINTENANCE;
    
    return SQLITE_OK;
}

graphqlite_mode_t graphqlite_get_current_mode(graphqlite_db_t *db) {
    if (!db) {
        return GRAPHQLITE_MODE_INTERACTIVE; // Default
    }
    
    return db->mode_manager.current_mode;
}

// ============================================================================
// Safe Mode Transition Management
// ============================================================================

int graphqlite_switch_mode(graphqlite_db_t *db, graphqlite_mode_t target_mode) {
    if (!db) {
        return SQLITE_ERROR;
    }
    
    pthread_mutex_lock(&db->mode_manager.mode_mutex);
    
    // Check if transition is already in progress
    if (db->mode_manager.transition_in_progress) {
        pthread_mutex_unlock(&db->mode_manager.mode_mutex);
        return SQLITE_BUSY;
    }
    
    // Check if already in target mode
    if (db->mode_manager.current_mode == target_mode) {
        pthread_mutex_unlock(&db->mode_manager.mode_mutex);
        return SQLITE_OK;
    }
    
    // Validate transition safety
    if (!graphqlite_is_mode_transition_safe(db, target_mode)) {
        pthread_mutex_unlock(&db->mode_manager.mode_mutex);
        return SQLITE_ERROR;
    }
    
    // Mark transition in progress
    db->mode_manager.transition_in_progress = true;
    db->mode_manager.previous_mode = db->mode_manager.current_mode;
    
    pthread_mutex_unlock(&db->mode_manager.mode_mutex);
    
    // Perform the actual transition
    int rc = perform_mode_transition(db, target_mode);
    
    pthread_mutex_lock(&db->mode_manager.mode_mutex);
    
    if (rc == SQLITE_OK) {
        db->mode_manager.current_mode = target_mode;
    } else {
        // Rollback on failure
        perform_mode_transition(db, db->mode_manager.previous_mode);
    }
    
    db->mode_manager.transition_in_progress = false;
    pthread_mutex_unlock(&db->mode_manager.mode_mutex);
    
    return rc;
}

int perform_mode_transition(graphqlite_db_t *db, graphqlite_mode_t target_mode) {
    int rc;
    
    // Step 1: Finalize current mode operations
    rc = finalize_current_mode(db);
    if (rc != SQLITE_OK) return rc;
    
    // Step 2: Apply target mode configuration
    switch (target_mode) {
        case GRAPHQLITE_MODE_INTERACTIVE:
            rc = graphqlite_switch_to_interactive_mode(db);
            break;
        case GRAPHQLITE_MODE_BULK_IMPORT:
            rc = graphqlite_switch_to_bulk_import_mode(db);
            break;
        case GRAPHQLITE_MODE_READONLY:
            rc = graphqlite_switch_to_readonly_mode(db);
            break;
        case GRAPHQLITE_MODE_MAINTENANCE:
            rc = graphqlite_switch_to_maintenance_mode(db);
            break;
        default:
            return SQLITE_ERROR;
    }
    
    return rc;
}

bool graphqlite_is_mode_transition_safe(graphqlite_db_t *db, graphqlite_mode_t target_mode) {
    if (!db) {
        return false;
    }
    
    // Check for active transactions
    if (graphqlite_in_transaction(db)) {
        // Some transitions may be safe within transactions
        switch (db->mode_manager.current_mode) {
            case GRAPHQLITE_MODE_INTERACTIVE:
                // Can switch to readonly within transaction
                return (target_mode == GRAPHQLITE_MODE_READONLY);
            case GRAPHQLITE_MODE_READONLY:
                // Can switch back to interactive within transaction
                return (target_mode == GRAPHQLITE_MODE_INTERACTIVE);
            default:
                return false;  // Most transitions require no active transaction
        }
    }
    
    // Check for bulk import in progress
    if (db->mode_manager.current_mode == GRAPHQLITE_MODE_BULK_IMPORT) {
        bulk_import_state_t *state = &db->bulk_state;
        if (state->transaction_active || state->operations_in_transaction > 0) {
            return false;  // Must complete bulk import first
        }
    }
    
    // Check for concurrent operations
    if (db->active_operations > 0) {
        return false;  // Wait for operations to complete
    }
    
    return true;
}

int finalize_current_mode(graphqlite_db_t *db) {
    switch (db->mode_manager.current_mode) {
        case GRAPHQLITE_MODE_INTERACTIVE:
            return finalize_interactive_mode(db);
        case GRAPHQLITE_MODE_BULK_IMPORT:
            return complete_bulk_import(db);
        case GRAPHQLITE_MODE_READONLY:
            return finalize_readonly_mode(db);
        case GRAPHQLITE_MODE_MAINTENANCE:
            return finalize_maintenance_mode(db);
        default:
            return SQLITE_OK;
    }
}

int finalize_interactive_mode(graphqlite_db_t *db) {
    // Commit any pending transactions
    if (graphqlite_in_transaction(db)) {
        int rc = graphqlite_commit_transaction(db);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Flush any cached writes
    return sqlite3_exec(db->sqlite_db, "PRAGMA wal_checkpoint(TRUNCATE)", NULL, NULL, NULL);
}

int finalize_readonly_mode(graphqlite_db_t *db) {
    // Disable query-only mode
    return sqlite3_exec(db->sqlite_db, "PRAGMA query_only = OFF", NULL, NULL, NULL);
}

int finalize_maintenance_mode(graphqlite_db_t *db) {
    // No special finalization needed for maintenance mode
    return SQLITE_OK;
}

// ============================================================================
// Bulk Import Operations
// ============================================================================

int graphqlite_bulk_create_nodes(graphqlite_db_t *db,
                                size_t count,
                                const char **label_arrays[],
                                size_t label_counts[],
                                property_set_t *property_sets[],
                                int64_t *result_ids) {
    if (!db || db->mode_manager.current_mode != GRAPHQLITE_MODE_BULK_IMPORT) {
        return SQLITE_ERROR;
    }
    
    bulk_import_state_t *state = &db->bulk_state;
    
    // Process in batches to manage memory
    size_t processed = 0;
    while (processed < count) {
        size_t batch_size = (count - processed < state->transaction_limit - state->operations_in_transaction) ?
                           count - processed : state->transaction_limit - state->operations_in_transaction;
        
        // Start transaction if needed
        if (!state->transaction_active) {
            int rc = sqlite3_exec(db->sqlite_db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
            if (rc != SQLITE_OK) return rc;
            state->transaction_active = true;
            state->operations_in_transaction = 0;
        }
        
        // Bulk insert current batch
        int rc = bulk_insert_nodes_raw(db,
                                      batch_size,
                                      label_arrays ? &label_arrays[processed] : NULL,
                                      label_counts ? &label_counts[processed] : NULL,
                                      property_sets ? &property_sets[processed] : NULL,
                                      &result_ids[processed]);
        if (rc != SQLITE_OK) {
            sqlite3_exec(db->sqlite_db, "ROLLBACK", NULL, NULL, NULL);
            state->transaction_active = false;
            return rc;
        }
        
        processed += batch_size;
        state->operations_in_transaction += batch_size;
        
        // Commit transaction if batch limit reached
        if (state->operations_in_transaction >= state->transaction_limit) {
            rc = sqlite3_exec(db->sqlite_db, "COMMIT", NULL, NULL, NULL);
            if (rc != SQLITE_OK) return rc;
            state->transaction_active = false;
        }
    }
    
    return SQLITE_OK;
}

int bulk_insert_nodes_raw(graphqlite_db_t *db,
                         size_t count,
                         const char **label_arrays[],
                         size_t label_counts[],
                         property_set_t *property_sets[],
                         int64_t *result_ids) {
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_CREATE_NODE);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    for (size_t i = 0; i < count; i++) {
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_reset(stmt);
            return SQLITE_ERROR;
        }
        
        result_ids[i] = sqlite3_last_insert_rowid(db->sqlite_db);
        sqlite3_reset(stmt);
    }
    
    // Bulk insert labels and properties separately
    if (label_arrays && label_counts) {
        int rc = bulk_insert_labels(db, count, result_ids, label_arrays, label_counts);
        if (rc != SQLITE_OK) return rc;
    }
    
    if (property_sets) {
        int rc = bulk_insert_properties(db, ENTITY_NODE, count, result_ids, property_sets);
        if (rc != SQLITE_OK) return rc;
    }
    
    return SQLITE_OK;
}

int bulk_insert_labels(graphqlite_db_t *db, size_t count, int64_t node_ids[],
                      const char **label_arrays[], size_t label_counts[]) {
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_ADD_NODE_LABEL);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (label_arrays[i] && label_counts[i] > 0) {
            for (size_t j = 0; j < label_counts[i]; j++) {
                sqlite3_bind_int64(stmt, 1, node_ids[i]);
                sqlite3_bind_text(stmt, 2, label_arrays[i][j], -1, SQLITE_STATIC);
                
                int rc = sqlite3_step(stmt);
                sqlite3_reset(stmt);
                
                if (rc != SQLITE_DONE) {
                    return SQLITE_ERROR;
                }
            }
        }
    }
    
    return SQLITE_OK;
}

int bulk_insert_properties(graphqlite_db_t *db, entity_type_t entity_type,
                          size_t count, int64_t entity_ids[],
                          property_set_t *property_sets[]) {
    // This is a simplified version - full implementation would group by type
    for (size_t i = 0; i < count; i++) {
        if (property_sets[i]) {
            int rc = graphqlite_set_properties(db, entity_type, entity_ids[i], property_sets[i]);
            if (rc != SQLITE_OK) {
                return rc;
            }
        }
    }
    
    return SQLITE_OK;
}

int complete_bulk_import(graphqlite_db_t *db) {
    if (!db || db->mode_manager.current_mode != GRAPHQLITE_MODE_BULK_IMPORT) {
        return SQLITE_ERROR;
    }
    
    bulk_import_state_t *state = &db->bulk_state;
    int rc;
    
    // Commit any pending transaction
    if (state->transaction_active) {
        rc = sqlite3_exec(db->sqlite_db, "COMMIT", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
        state->transaction_active = false;
    }
    
    // Re-enable foreign key checks
    rc = sqlite3_exec(db->sqlite_db, "PRAGMA defer_foreign_keys = OFF", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Perform integrity check if requested
    if (db->bulk_config.integrity_check_on_complete) {
        rc = perform_integrity_check(db);
        if (rc != SQLITE_OK) return rc;
    }
    
    // Update table statistics for query optimization
    if (db->bulk_config.auto_analyze_on_complete) {
        rc = sqlite3_exec(db->sqlite_db, "ANALYZE", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return rc;
    }
    
    return SQLITE_OK;
}

int perform_integrity_check(graphqlite_db_t *db) {
    const char *sql = "PRAGMA integrity_check";
    sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *result = (const char*)sqlite3_column_text(stmt, 0);
        if (strcmp(result, "ok") != 0) {
            // Integrity check failed
            sqlite3_reset(stmt);
            return SQLITE_ERROR;
        }
    }
    
    sqlite3_reset(stmt);
    return SQLITE_OK;
}

