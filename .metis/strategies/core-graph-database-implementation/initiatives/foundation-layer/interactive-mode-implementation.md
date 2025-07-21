---
id: interactive-mode-implementation
level: task
title: "Interactive mode implementation"
created_at: 2025-07-19T23:02:53.367039+00:00
updated_at: 2025-07-19T23:02:53.367039+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Interactive mode implementation

## Parent Initiative

[[foundation-layer]]

## Objective

Implement the interactive operation mode that provides full ACID guarantees, real-time consistency, and optimal performance for typical application workloads. Interactive mode is the default mode optimized for applications that perform frequent small transactions with immediate consistency requirements.

This mode must achieve the 10,000+ nodes/second creation rate validated in Phase Zero while maintaining all database integrity constraints and providing immediate visibility of changes across concurrent operations.

## Acceptance Criteria

- [ ] **Performance Target**: Achieve 10,000+ nodes/second creation rate
- [ ] **ACID Guarantees**: Full Atomicity, Consistency, Isolation, Durability for all operations
- [ ] **Real-time Consistency**: Immediate visibility of committed changes
- [ ] **Concurrent Safety**: Thread-safe operations with proper isolation
- [ ] **Default Mode**: Interactive mode is the default operation mode
- [ ] **Index Maintenance**: All indexes maintained in real-time for optimal query performance
- [ ] **Transaction Efficiency**: Individual operations complete with minimal transaction overhead
- [ ] **Memory Management**: Stable memory usage under sustained interactive workloads

## Implementation Notes

**Interactive Mode Configuration:**

```c
typedef struct {
    // ACID settings
    bool synchronous_mode;     // PRAGMA synchronous = NORMAL
    bool foreign_keys;         // PRAGMA foreign_keys = ON
    bool journal_mode_wal;     // PRAGMA journal_mode = WAL (for concurrency)
    
    // Performance settings
    int cache_size;            // PRAGMA cache_size = 2000 (moderate cache)
    int page_size;            // PRAGMA page_size = 4096
    bool temp_store_memory;   // PRAGMA temp_store = MEMORY
    
    // Transaction settings
    bool auto_commit;         // Individual operations auto-commit
    int lock_timeout;         // Timeout for lock acquisition
    
    // Concurrency settings
    int max_connections;      // Maximum concurrent connections
    bool read_uncommitted;    // Allow dirty reads for performance
} interactive_mode_config_t;
```

**Mode Initialization:**

```c
int initialize_interactive_mode(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return SQLITE_ERROR;
    
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
    
    db->current_mode = GRAPHQLITE_MODE_INTERACTIVE;
    return SQLITE_OK;
}
```

**Interactive Mode Operations:**

```c
// Optimized node creation for interactive mode
int64_t graphqlite_create_node_interactive(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return -1;
    
    // Use auto-commit for individual operations
    if (!graphqlite_in_transaction(db)) {
        sqlite3_stmt *stmt = db->prepared_statements[STMT_CREATE_NODE];
        int rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);
        
        if (rc == SQLITE_DONE) {
            return sqlite3_last_insert_rowid(db->sqlite_db);
        }
        return -1;
    } else {
        // Within existing transaction
        return graphqlite_create_node(db);
    }
}

// Batch operations optimized for interactive mode
int graphqlite_create_nodes_interactive_batch(graphqlite_db_t *db, size_t count, 
                                            int64_t *result_ids) {
    if (!db || count == 0 || !result_ids) return SQLITE_ERROR;
    
    // Use explicit transaction for batches to improve performance
    int rc = graphqlite_begin_transaction(db);
    if (rc != SQLITE_OK) return rc;
    
    for (size_t i = 0; i < count; i++) {
        int64_t node_id = graphqlite_create_node(db);
        if (node_id < 0) {
            graphqlite_rollback_transaction(db);
            return SQLITE_ERROR;
        }
        result_ids[i] = node_id;
    }
    
    return graphqlite_commit_transaction(db);
}
```

**Concurrency Management:**

```c
// Connection pool for concurrent access
typedef struct {
    graphqlite_db_t **connections;
    bool *connection_busy;
    pthread_mutex_t pool_mutex;
    pthread_cond_t connection_available;
    int pool_size;
    int active_connections;
} connection_pool_t;

connection_pool_t* create_connection_pool(const char *db_path, int pool_size) {
    connection_pool_t *pool = malloc(sizeof(connection_pool_t));
    pool->connections = malloc(sizeof(graphqlite_db_t*) * pool_size);
    pool->connection_busy = calloc(pool_size, sizeof(bool));
    pool->pool_size = pool_size;
    pool->active_connections = 0;
    
    pthread_mutex_init(&pool->pool_mutex, NULL);
    pthread_cond_init(&pool->connection_available, NULL);
    
    // Initialize connections
    for (int i = 0; i < pool_size; i++) {
        pool->connections[i] = graphqlite_open(db_path);
        if (pool->connections[i]) {
            initialize_interactive_mode(pool->connections[i]);
        }
    }
    
    return pool;
}

graphqlite_db_t* acquire_connection(connection_pool_t *pool) {
    pthread_mutex_lock(&pool->pool_mutex);
    
    while (pool->active_connections >= pool->pool_size) {
        pthread_cond_wait(&pool->connection_available, &pool->pool_mutex);
    }
    
    // Find available connection
    for (int i = 0; i < pool->pool_size; i++) {
        if (!pool->connection_busy[i] && pool->connections[i]) {
            pool->connection_busy[i] = true;
            pool->active_connections++;
            pthread_mutex_unlock(&pool->pool_mutex);
            return pool->connections[i];
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
    return NULL;
}

void release_connection(connection_pool_t *pool, graphqlite_db_t *db) {
    pthread_mutex_lock(&pool->pool_mutex);
    
    for (int i = 0; i < pool->pool_size; i++) {
        if (pool->connections[i] == db) {
            pool->connection_busy[i] = false;
            pool->active_connections--;
            pthread_cond_signal(&pool->connection_available);
            break;
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
}
```

**Performance Monitoring:**

```c
typedef struct {
    uint64_t operations_count;
    uint64_t total_operation_time_us;
    uint64_t transactions_count;
    uint64_t total_transaction_time_us;
    uint64_t cache_hits;
    uint64_t cache_misses;
    pthread_mutex_t stats_mutex;
} interactive_mode_stats_t;

void record_operation_stats(graphqlite_db_t *db, uint64_t operation_time_us) {
    pthread_mutex_lock(&db->stats.stats_mutex);
    db->stats.operations_count++;
    db->stats.total_operation_time_us += operation_time_us;
    pthread_mutex_unlock(&db->stats.stats_mutex);
}

double get_average_operation_time_ms(graphqlite_db_t *db) {
    pthread_mutex_lock(&db->stats.stats_mutex);
    double avg = 0.0;
    if (db->stats.operations_count > 0) {
        avg = (double)db->stats.total_operation_time_us / 
              (double)db->stats.operations_count / 1000.0;
    }
    pthread_mutex_unlock(&db->stats.stats_mutex);
    return avg;
}
```

**Real-time Index Maintenance:**

```c
// Ensure indexes are maintained immediately for interactive queries
int update_indexes_interactive(graphqlite_db_t *db) {
    // In interactive mode, indexes are automatically maintained
    // by SQLite after each INSERT/UPDATE/DELETE
    
    // Optionally analyze tables periodically for query optimization
    static time_t last_analyze = 0;
    time_t now = time(NULL);
    
    if (now - last_analyze > ANALYZE_INTERVAL_SECONDS) {
        sqlite3_exec(db->sqlite_db, "ANALYZE", NULL, NULL, NULL);
        last_analyze = now;
    }
    
    return SQLITE_OK;
}
```

**Interactive Mode Optimizations:**
- WAL mode for better concurrency (readers don't block writers)
- Moderate cache size for balanced memory usage and performance
- Auto-commit individual operations for immediate consistency
- Connection pooling for concurrent access
- Real-time statistics collection for performance monitoring
- Prepared statement reuse for common operations
- Immediate index maintenance for optimal query performance

## Status Updates

*To be added during implementation*