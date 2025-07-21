---
id: mode-switching-and-optimization
level: task
title: "Mode switching and optimization"
created_at: 2025-07-19T23:03:09.730981+00:00
updated_at: 2025-07-19T23:03:09.730981+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Mode switching and optimization

## Parent Initiative

[[foundation-layer]]

## Objective

Implement dynamic switching between interactive and bulk import modes with proper state transition management, configuration persistence, and optimization strategies. The system must safely transition between modes while preserving data integrity and applying appropriate optimizations for each operational mode.

This system enables applications to seamlessly switch from high-throughput bulk loading to real-time interactive operations without requiring separate database connections or complex application logic.

## Acceptance Criteria

- [ ] **Safe Mode Transitions**: Proper state management during mode switches without data corruption
- [ ] **Configuration Persistence**: Mode-specific settings preserved and restored correctly
- [ ] **Transaction Boundaries**: Respect existing transaction boundaries during mode switches
- [ ] **Performance Optimization**: Automatic optimization application for each mode
- [ ] **Error Recovery**: Rollback to previous mode on transition failures
- [ ] **Concurrent Safety**: Thread-safe mode switching with proper locking
- [ ] **Resource Management**: Proper cleanup and resource allocation during transitions
- [ ] **Validation**: Pre-transition validation to ensure safe mode switching

## Implementation Notes

**Mode Management System:**

```c
typedef enum {
    GRAPHQLITE_MODE_INTERACTIVE,
    GRAPHQLITE_MODE_BULK_IMPORT,
    GRAPHQLITE_MODE_MAINTENANCE,
    GRAPHQLITE_MODE_READONLY
} graphqlite_mode_t;

typedef struct {
    graphqlite_mode_t current_mode;
    graphqlite_mode_t previous_mode;
    
    // Mode-specific configurations
    interactive_mode_config_t interactive_config;
    bulk_import_config_t bulk_config;
    
    // Transition state
    bool transition_in_progress;
    pthread_mutex_t mode_mutex;
    
    // Saved state for rollback
    void *saved_pragma_state;
    size_t saved_state_size;
} mode_manager_t;
```

**Core Mode Switching API:**

```c
// Mode switching functions
int graphqlite_switch_to_interactive_mode(graphqlite_db_t *db);
int graphqlite_switch_to_bulk_import_mode(graphqlite_db_t *db);
int graphqlite_switch_to_readonly_mode(graphqlite_db_t *db);
int graphqlite_switch_to_maintenance_mode(graphqlite_db_t *db);

// Mode query functions
graphqlite_mode_t graphqlite_get_current_mode(graphqlite_db_t *db);
bool graphqlite_is_mode_transition_safe(graphqlite_db_t *db, graphqlite_mode_t target_mode);

// Configuration management
int graphqlite_save_mode_configuration(graphqlite_db_t *db);
int graphqlite_restore_mode_configuration(graphqlite_db_t *db, graphqlite_mode_t mode);
```

**Safe Mode Transition Implementation:**

```c
int graphqlite_switch_mode(graphqlite_db_t *db, graphqlite_mode_t target_mode) {
    if (!db) return SQLITE_ERROR;
    
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
    
    // Save current configuration for potential rollback
    rc = save_current_pragma_state(db);
    if (rc != SQLITE_OK) return rc;
    
    // Step 1: Finalize current mode operations
    rc = finalize_current_mode(db);
    if (rc != SQLITE_OK) return rc;
    
    // Step 2: Apply target mode configuration
    switch (target_mode) {
        case GRAPHQLITE_MODE_INTERACTIVE:
            rc = initialize_interactive_mode(db);
            break;
        case GRAPHQLITE_MODE_BULK_IMPORT:
            rc = initialize_bulk_import_mode(db);
            break;
        case GRAPHQLITE_MODE_READONLY:
            rc = initialize_readonly_mode(db);
            break;
        case GRAPHQLITE_MODE_MAINTENANCE:
            rc = initialize_maintenance_mode(db);
            break;
        default:
            return SQLITE_ERROR;
    }
    
    if (rc != SQLITE_OK) {
        // Restore previous configuration on failure
        restore_saved_pragma_state(db);
        return rc;
    }
    
    // Step 3: Optimize for new mode
    rc = optimize_for_mode(db, target_mode);
    if (rc != SQLITE_OK) {
        restore_saved_pragma_state(db);
        return rc;
    }
    
    return SQLITE_OK;
}
```

**Mode Finalization:**

```c
int finalize_current_mode(graphqlite_db_t *db) {
    switch (db->mode_manager.current_mode) {
        case GRAPHQLITE_MODE_INTERACTIVE:
            return finalize_interactive_mode(db);
        case GRAPHQLITE_MODE_BULK_IMPORT:
            return complete_bulk_import(db);  // This rebuilds indexes
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

int finalize_bulk_import_mode(graphqlite_db_t *db) {
    // Complete bulk import (rebuilds indexes, etc.)
    return complete_bulk_import(db);
}
```

**Configuration State Management:**

```c
typedef struct {
    char *pragma_name;
    char *current_value;
} pragma_state_t;

int save_current_pragma_state(graphqlite_db_t *db) {
    const char *important_pragmas[] = {
        "synchronous",
        "journal_mode", 
        "cache_size",
        "page_size",
        "temp_store",
        "foreign_keys",
        "defer_foreign_keys",
        NULL
    };
    
    // Count pragmas
    size_t pragma_count = 0;
    while (important_pragmas[pragma_count]) pragma_count++;
    
    // Allocate state array
    pragma_state_t *saved_state = malloc(pragma_count * sizeof(pragma_state_t));
    
    // Save each pragma value
    for (size_t i = 0; i < pragma_count; i++) {
        saved_state[i].pragma_name = strdup(important_pragmas[i]);
        
        char sql[256];
        snprintf(sql, sizeof(sql), "PRAGMA %s", important_pragmas[i]);
        
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db->sqlite_db, sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            const char *value = (const char*)sqlite3_column_text(stmt, 0);
            saved_state[i].current_value = strdup(value ? value : "");
        } else {
            saved_state[i].current_value = strdup("");
        }
        sqlite3_finalize(stmt);
    }
    
    // Store in db structure
    free(db->mode_manager.saved_pragma_state);
    db->mode_manager.saved_pragma_state = saved_state;
    db->mode_manager.saved_state_size = pragma_count;
    
    return SQLITE_OK;
}

int restore_saved_pragma_state(graphqlite_db_t *db) {
    if (!db->mode_manager.saved_pragma_state) return SQLITE_ERROR;
    
    pragma_state_t *saved_state = (pragma_state_t*)db->mode_manager.saved_pragma_state;
    
    for (size_t i = 0; i < db->mode_manager.saved_state_size; i++) {
        char sql[256];
        snprintf(sql, sizeof(sql), "PRAGMA %s = %s", 
                saved_state[i].pragma_name, 
                saved_state[i].current_value);
        
        int rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            // Log warning but continue restoration
            fprintf(stderr, "Warning: Failed to restore pragma %s\n", 
                   saved_state[i].pragma_name);
        }
    }
    
    return SQLITE_OK;
}
```

**Mode-Specific Optimizations:**

```c
int optimize_for_mode(graphqlite_db_t *db, graphqlite_mode_t mode) {
    switch (mode) {
        case GRAPHQLITE_MODE_INTERACTIVE:
            return optimize_for_interactive(db);
        case GRAPHQLITE_MODE_BULK_IMPORT:
            return optimize_for_bulk_import(db);
        case GRAPHQLITE_MODE_READONLY:
            return optimize_for_readonly(db);
        case GRAPHQLITE_MODE_MAINTENANCE:
            return optimize_for_maintenance(db);
        default:
            return SQLITE_OK;
    }
}

int optimize_for_interactive(graphqlite_db_t *db) {
    // Ensure indexes are up-to-date for fast queries
    int rc = sqlite3_exec(db->sqlite_db, "ANALYZE", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Precompile frequently used statements
    rc = prepare_interactive_statements(db);
    if (rc != SQLITE_OK) return rc;
    
    // Warm up property key cache
    rc = preload_property_key_cache(db);
    if (rc != SQLITE_OK) return rc;
    
    return SQLITE_OK;
}

int optimize_for_bulk_import(graphqlite_db_t *db) {
    // Pre-allocate bulk import buffers
    int rc = allocate_bulk_import_buffers(db);
    if (rc != SQLITE_OK) return rc;
    
    // Disable automatic maintenance tasks
    rc = disable_background_maintenance(db);
    if (rc != SQLITE_OK) return rc;
    
    return SQLITE_OK;
}

int optimize_for_readonly(graphqlite_db_t *db) {
    // Optimize for read performance
    int rc = sqlite3_exec(db->sqlite_db, "PRAGMA query_only = ON", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return rc;
    
    // Cache all property keys for faster lookups
    rc = cache_all_property_keys(db);
    if (rc != SQLITE_OK) return rc;
    
    return SQLITE_OK;
}
```

**Transaction-Safe Mode Switching:**

```c
bool graphqlite_is_mode_transition_safe(graphqlite_db_t *db, graphqlite_mode_t target_mode) {
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
```

**Performance Monitoring During Transitions:**

```c
typedef struct {
    uint64_t transitions_completed;
    uint64_t transitions_failed;
    uint64_t total_transition_time_us;
    graphqlite_mode_t most_used_mode;
    uint64_t mode_usage_counters[4];  // One per mode
} mode_transition_stats_t;

void record_mode_transition(graphqlite_db_t *db, 
                           graphqlite_mode_t from_mode,
                           graphqlite_mode_t to_mode,
                           uint64_t transition_time_us,
                           bool success) {
    mode_transition_stats_t *stats = &db->transition_stats;
    
    if (success) {
        stats->transitions_completed++;
        stats->total_transition_time_us += transition_time_us;
        stats->mode_usage_counters[to_mode]++;
    } else {
        stats->transitions_failed++;
    }
    
    // Update most used mode
    graphqlite_mode_t most_used = GRAPHQLITE_MODE_INTERACTIVE;
    for (int i = 1; i < 4; i++) {
        if (stats->mode_usage_counters[i] > stats->mode_usage_counters[most_used]) {
            most_used = i;
        }
    }
    stats->most_used_mode = most_used;
}
```

**Critical Design Considerations:**
- Mode transitions must be atomic to prevent inconsistent state
- Bulk import completion automatically rebuilds indexes before switching
- Transaction boundaries are strictly respected during mode changes
- Configuration rollback ensures system stability on transition failures
- Thread safety through mutex protection during transitions
- Performance optimizations applied automatically for each mode
- Resource cleanup prevents memory leaks during mode switches

## Status Updates

*To be added during implementation*