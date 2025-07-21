---
id: bulk-import-mode
level: task
title: "Bulk import mode"
created_at: 2025-07-19T23:03:02.316203+00:00
updated_at: 2025-07-19T23:03:02.316203+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Bulk import mode

## Parent Initiative

[[foundation-layer]]

## Objective

Implement the bulk import operation mode that maximizes throughput for large-scale data loading operations. This mode trades some ACID guarantees and real-time consistency for dramatically improved insertion performance through deferred indexing, relaxed durability, and optimized batch processing.

The bulk import mode must achieve 50,000+ nodes/second insertion rate as validated in Phase Zero while ensuring data integrity can be restored through proper index rebuilding and consistency checks upon completion.

## Acceptance Criteria

- [ ] **Performance Target**: Achieve 50,000+ nodes/second insertion rate
- [ ] **Throughput Optimization**: Prioritize maximum throughput over ACID guarantees during import
- [ ] **Deferred Indexing**: Index maintenance deferred until bulk operation completion
- [ ] **Relaxed Durability**: Use PRAGMA synchronous = OFF during bulk operations
- [ ] **Batch Processing**: Process operations in large batches to minimize transaction overhead
- [ ] **Memory Efficiency**: Minimize memory footprint during large imports through streaming
- [ ] **Integrity Recovery**: Rebuild indexes and validate data integrity upon completion
- [ ] **Mode Isolation**: Proper isolation between bulk and interactive operations
- [ ] **Error Handling**: Rollback capabilities for partial imports with clear error reporting

## Implementation Notes

**Bulk Import Mode Configuration:**

```c
typedef struct {
    // Performance settings
    bool synchronous_off;       // PRAGMA synchronous = OFF
    bool journal_mode_memory;   // PRAGMA journal_mode = MEMORY
    bool temp_store_memory;     // PRAGMA temp_store = MEMORY
    int large_cache_size;       // PRAGMA cache_size = 100000
    int large_page_size;        // PRAGMA page_size = 65536
    
    // Indexing settings
    bool defer_foreign_keys;    // PRAGMA defer_foreign_keys = ON
    bool defer_index_updates;   // Custom deferred indexing
    
    // Batch settings
    size_t batch_size;          // Operations per transaction
    size_t memory_limit;        // Maximum memory before flush
    
    // Import tracking
    bool integrity_check_on_complete;
    bool auto_analyze_on_complete;
} bulk_import_config_t;
```

**Mode Initialization:**

```c
int initialize_bulk_import_mode(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return SQLITE_ERROR;
    
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
    
    // Disable automatic index updates
    if (config->defer_index_updates) {
        rc = disable_automatic_indexing(db);
        if (rc != SQLITE_OK) return rc;
    }
    
    db->current_mode = GRAPHQLITE_MODE_BULK_IMPORT;
    db->bulk_config = *config;
    
    return SQLITE_OK;
}
```

**Batch Processing System:**

```c
typedef struct {
    // Batch buffers
    node_create_batch_t node_batch;
    edge_create_batch_t edge_batch;
    property_batch_t property_batch;
    
    // Current batch sizes
    size_t nodes_in_batch;
    size_t edges_in_batch;
    size_t properties_in_batch;
    
    // Memory tracking
    size_t current_memory_usage;
    size_t memory_limit;
    
    // Transaction state
    bool transaction_active;
    size_t operations_in_transaction;
    size_t transaction_limit;
} bulk_import_state_t;

int graphqlite_bulk_create_nodes(graphqlite_db_t *db, 
                                size_t count,
                                const char **label_arrays[],
                                size_t label_counts[],
                                property_set_t *property_sets[],
                                int64_t *result_ids) {
    if (!db || db->current_mode != GRAPHQLITE_MODE_BULK_IMPORT) return SQLITE_ERROR;
    
    bulk_import_state_t *state = &db->bulk_state;
    
    // Process in batches to manage memory
    size_t processed = 0;
    while (processed < count) {
        size_t batch_size = min(count - processed, 
                               state->transaction_limit - state->operations_in_transaction);
        
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
                                      &label_arrays[processed],
                                      &label_counts[processed],
                                      &property_sets[processed],
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
        
        // Check memory limit and flush if needed
        if (state->current_memory_usage > state->memory_limit) {
            rc = flush_bulk_import_buffers(db);
            if (rc != SQLITE_OK) return rc;
        }
    }
    
    return SQLITE_OK;
}
```

**Raw Insertion Functions:**

```c
int bulk_insert_nodes_raw(graphqlite_db_t *db,
                         size_t count,
                         const char **label_arrays[],
                         size_t label_counts[],
                         property_set_t *property_sets[],
                         int64_t *result_ids) {
    // Use multi-row INSERT for maximum performance
    const char *bulk_insert_sql = 
        "INSERT INTO nodes DEFAULT VALUES";
    
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->sqlite_db, bulk_insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    
    for (size_t i = 0; i < count; i++) {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return SQLITE_ERROR;
        }
        
        result_ids[i] = sqlite3_last_insert_rowid(db->sqlite_db);
        sqlite3_reset(stmt);
    }
    
    sqlite3_finalize(stmt);
    
    // Bulk insert labels and properties separately
    if (label_arrays) {
        rc = bulk_insert_labels(db, count, result_ids, label_arrays, label_counts);
        if (rc != SQLITE_OK) return rc;
    }
    
    if (property_sets) {
        rc = bulk_insert_properties(db, ENTITY_NODE, count, result_ids, property_sets);
        if (rc != SQLITE_OK) return rc;
    }
    
    return SQLITE_OK;
}

int bulk_insert_properties(graphqlite_db_t *db, entity_type_t entity_type,
                          size_t count, int64_t entity_ids[],
                          property_set_t *property_sets[]) {
    // Group properties by type for efficient bulk insertion
    property_batch_buffer_t buffers[4] = {0}; // int, text, real, bool
    
    // Collect all properties by type
    for (size_t i = 0; i < count; i++) {
        if (!property_sets[i]) continue;
        
        for (size_t j = 0; j < property_sets[i]->count; j++) {
            property_pair_t *prop = &property_sets[i]->properties[j];
            int key_id = intern_property_key(db->key_cache, prop->key);
            
            switch (prop->value.type) {
                case PROP_INT:
                    add_to_batch_buffer(&buffers[0], entity_ids[i], key_id, &prop->value);
                    break;
                case PROP_TEXT:
                    add_to_batch_buffer(&buffers[1], entity_ids[i], key_id, &prop->value);
                    break;
                case PROP_REAL:
                    add_to_batch_buffer(&buffers[2], entity_ids[i], key_id, &prop->value);
                    break;
                case PROP_BOOL:
                    add_to_batch_buffer(&buffers[3], entity_ids[i], key_id, &prop->value);
                    break;
            }
        }
    }
    
    // Execute bulk inserts per type
    const char *table_prefix = (entity_type == ENTITY_NODE) ? "node_props" : "edge_props";
    
    int rc = execute_bulk_property_insert(db, table_prefix, "int", &buffers[0]);
    if (rc != SQLITE_OK) return rc;
    
    rc = execute_bulk_property_insert(db, table_prefix, "text", &buffers[1]);
    if (rc != SQLITE_OK) return rc;
    
    rc = execute_bulk_property_insert(db, table_prefix, "real", &buffers[2]);
    if (rc != SQLITE_OK) return rc;
    
    rc = execute_bulk_property_insert(db, table_prefix, "bool", &buffers[3]);
    if (rc != SQLITE_OK) return rc;
    
    // Clean up buffers
    for (int i = 0; i < 4; i++) {
        free_batch_buffer(&buffers[i]);
    }
    
    return SQLITE_OK;
}
```

**Index Rebuilding and Integrity Check:**

```c
int complete_bulk_import(graphqlite_db_t *db) {
    if (!db || db->current_mode != GRAPHQLITE_MODE_BULK_IMPORT) return SQLITE_ERROR;
    
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
    
    // Rebuild indexes for optimal query performance
    rc = rebuild_deferred_indexes(db);
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
    
    // Reset to normal operational settings
    rc = initialize_interactive_mode(db);
    if (rc != SQLITE_OK) return rc;
    
    return SQLITE_OK;
}

int rebuild_deferred_indexes(graphqlite_db_t *db) {
    // Rebuild all indexes that were deferred during bulk import
    const char *rebuild_indexes[] = {
        "REINDEX idx_nodes_id",
        "REINDEX idx_edges_source",
        "REINDEX idx_edges_target", 
        "REINDEX idx_edges_type",
        "REINDEX idx_node_props_int_key_value",
        "REINDEX idx_node_props_text_key_value",
        "REINDEX idx_node_props_real_key_value", 
        "REINDEX idx_node_props_bool_key_value",
        "REINDEX idx_edge_props_int_key_value",
        "REINDEX idx_edge_props_text_key_value",
        "REINDEX idx_edge_props_real_key_value",
        "REINDEX idx_edge_props_bool_key_value",
        NULL
    };
    
    for (int i = 0; rebuild_indexes[i]; i++) {
        int rc = sqlite3_exec(db->sqlite_db, rebuild_indexes[i], NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            return rc; // Index rebuild failed
        }
    }
    
    return SQLITE_OK;
}
```

**Performance Monitoring:**

```c
typedef struct {
    uint64_t nodes_imported;
    uint64_t edges_imported;
    uint64_t properties_imported;
    uint64_t total_import_time_us;
    uint64_t transactions_committed;
    uint64_t memory_flushes;
    double average_throughput_per_second;
} bulk_import_stats_t;

void update_bulk_import_stats(graphqlite_db_t *db, 
                             size_t nodes_added, 
                             size_t edges_added,
                             size_t properties_added,
                             uint64_t operation_time_us) {
    bulk_import_stats_t *stats = &db->bulk_stats;
    
    stats->nodes_imported += nodes_added;
    stats->edges_imported += edges_added; 
    stats->properties_imported += properties_added;
    stats->total_import_time_us += operation_time_us;
    
    // Update rolling average throughput
    if (stats->total_import_time_us > 0) {
        double total_operations = stats->nodes_imported + stats->edges_imported;
        stats->average_throughput_per_second = 
            (total_operations * 1000000.0) / stats->total_import_time_us;
    }
}
```

**Critical Optimizations:**
- PRAGMA synchronous = OFF eliminates disk sync overhead
- Memory journal mode avoids disk I/O during transactions
- Large cache and page sizes optimize memory usage
- Deferred indexing eliminates real-time index maintenance cost
- Bulk INSERT statements minimize SQL parsing overhead  
- Property type batching reduces statement preparation cost
- Transaction batching amortizes commit overhead across many operations
- Memory limit enforcement prevents unbounded memory growth

## Status Updates

*To be added during implementation*