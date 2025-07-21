---
id: prepared-statement-management
level: task
title: "Prepared statement management"
created_at: 2025-07-19T23:03:18.673628+00:00
updated_at: 2025-07-19T23:03:18.673628+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Prepared statement management

## Parent Initiative

[[foundation-layer]]

## Objective

Implement a comprehensive prepared statement management system that pre-compiles, caches, and reuses SQL statements for optimal performance. The system must provide automatic statement preparation, intelligent caching strategies, and proper resource management to minimize SQL parsing overhead and maximize query execution speed.

Prepared statement management is critical for achieving the sub-5ms operation performance targets by eliminating repeated SQL parsing and compilation costs for frequently executed queries.

## Acceptance Criteria

- [ ] **Statement Pre-compilation**: All common operations use prepared statements
- [ ] **Automatic Caching**: Intelligent caching of frequently used statements
- [ ] **Resource Management**: Proper cleanup and memory management for statements
- [ ] **Performance Optimization**: Statement reuse reduces SQL parsing overhead by 80%+
- [ ] **Thread Safety**: Safe concurrent access to prepared statement cache
- [ ] **Dynamic Statements**: Support for parameterized queries with varying complexity
- [ ] **Error Handling**: Proper error recovery and statement re-preparation
- [ ] **Memory Efficiency**: Bounded cache size with LRU eviction policy

## Implementation Notes

**Statement Classification System:**

```c
typedef enum {
    // Node operations
    STMT_CREATE_NODE,
    STMT_DELETE_NODE,
    STMT_GET_NODE,
    STMT_NODE_EXISTS,
    
    // Edge operations
    STMT_CREATE_EDGE,
    STMT_DELETE_EDGE,
    STMT_GET_OUTGOING_EDGES,
    STMT_GET_INCOMING_EDGES,
    STMT_GET_OUTGOING_NEIGHBORS,
    STMT_GET_INCOMING_NEIGHBORS,
    STMT_GET_OUTGOING_EDGES_BY_TYPE,
    STMT_GET_INCOMING_EDGES_BY_TYPE,
    STMT_GET_OUTGOING_NEIGHBORS_BY_TYPE,
    STMT_GET_INCOMING_NEIGHBORS_BY_TYPE,
    
    // Property operations (per type)
    STMT_SET_NODE_PROP_INT,
    STMT_SET_NODE_PROP_TEXT,
    STMT_SET_NODE_PROP_REAL,
    STMT_SET_NODE_PROP_BOOL,
    STMT_GET_NODE_PROP_INT,
    STMT_GET_NODE_PROP_TEXT,
    STMT_GET_NODE_PROP_REAL,
    STMT_GET_NODE_PROP_BOOL,
    STMT_DEL_NODE_PROP_INT,
    STMT_DEL_NODE_PROP_TEXT,
    STMT_DEL_NODE_PROP_REAL,
    STMT_DEL_NODE_PROP_BOOL,
    
    // Edge property operations
    STMT_SET_EDGE_PROP_INT,
    STMT_SET_EDGE_PROP_TEXT,
    STMT_SET_EDGE_PROP_REAL,
    STMT_SET_EDGE_PROP_BOOL,
    STMT_GET_EDGE_PROP_INT,
    STMT_GET_EDGE_PROP_TEXT,
    STMT_GET_EDGE_PROP_REAL,
    STMT_GET_EDGE_PROP_BOOL,
    STMT_DEL_EDGE_PROP_INT,
    STMT_DEL_EDGE_PROP_TEXT,
    STMT_DEL_EDGE_PROP_REAL,
    STMT_DEL_EDGE_PROP_BOOL,
    
    // Label operations
    STMT_ADD_NODE_LABEL,
    STMT_REMOVE_NODE_LABEL,
    STMT_GET_NODE_LABELS,
    STMT_FIND_NODES_BY_LABEL,
    
    // Property key management
    STMT_INTERN_PROPERTY_KEY,
    STMT_LOOKUP_PROPERTY_KEY,
    
    // Utility statements
    STMT_COUNT
} statement_type_t;
```

**Prepared Statement Management Structure:**

```c
typedef struct {
    sqlite3_stmt *stmt;
    char *sql;
    statement_type_t type;
    uint64_t usage_count;
    time_t last_used;
    bool is_reusable;
} prepared_statement_entry_t;

typedef struct {
    // Fixed statements (always prepared)
    sqlite3_stmt *fixed_statements[STMT_COUNT];
    
    // Dynamic statement cache
    prepared_statement_entry_t *dynamic_cache;
    size_t dynamic_cache_size;
    size_t dynamic_cache_capacity;
    
    // Cache management
    pthread_mutex_t cache_mutex;
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    // Configuration
    size_t max_dynamic_statements;
    time_t statement_ttl_seconds;
} statement_manager_t;
```

**Statement Initialization:**

```c
int initialize_statement_manager(graphqlite_db_t *db) {
    statement_manager_t *mgr = &db->stmt_manager;
    
    // Initialize fixed statements
    int rc = prepare_fixed_statements(db);
    if (rc != SQLITE_OK) return rc;
    
    // Initialize dynamic cache
    mgr->max_dynamic_statements = 100;
    mgr->statement_ttl_seconds = 300;  // 5 minutes
    mgr->dynamic_cache_capacity = mgr->max_dynamic_statements;
    mgr->dynamic_cache = calloc(mgr->dynamic_cache_capacity, 
                               sizeof(prepared_statement_entry_t));
    
    pthread_mutex_init(&mgr->cache_mutex, NULL);
    
    return SQLITE_OK;
}

int prepare_fixed_statements(graphqlite_db_t *db) {
    statement_manager_t *mgr = &db->stmt_manager;
    
    // Define all fixed SQL statements
    const char *fixed_sqls[STMT_COUNT] = {
        [STMT_CREATE_NODE] = "INSERT INTO nodes DEFAULT VALUES",
        [STMT_DELETE_NODE] = "DELETE FROM nodes WHERE id = ?",
        [STMT_GET_NODE] = "SELECT id FROM nodes WHERE id = ?",
        [STMT_NODE_EXISTS] = "SELECT 1 FROM nodes WHERE id = ? LIMIT 1",
        
        [STMT_CREATE_EDGE] = "INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?)",
        [STMT_DELETE_EDGE] = "DELETE FROM edges WHERE id = ?",
        [STMT_GET_OUTGOING_EDGES] = "SELECT id FROM edges WHERE source_id = ?",
        [STMT_GET_INCOMING_EDGES] = "SELECT id FROM edges WHERE target_id = ?",
        [STMT_GET_OUTGOING_NEIGHBORS] = "SELECT target_id FROM edges WHERE source_id = ?",
        [STMT_GET_INCOMING_NEIGHBORS] = "SELECT source_id FROM edges WHERE target_id = ?",
        [STMT_GET_OUTGOING_EDGES_BY_TYPE] = "SELECT id FROM edges WHERE source_id = ? AND type = ?",
        [STMT_GET_INCOMING_EDGES_BY_TYPE] = "SELECT id FROM edges WHERE target_id = ? AND type = ?",
        [STMT_GET_OUTGOING_NEIGHBORS_BY_TYPE] = "SELECT target_id FROM edges WHERE source_id = ? AND type = ?",
        [STMT_GET_INCOMING_NEIGHBORS_BY_TYPE] = "SELECT source_id FROM edges WHERE target_id = ? AND type = ?",
        
        // Property operations
        [STMT_SET_NODE_PROP_INT] = "INSERT OR REPLACE INTO node_props_int (node_id, key_id, value) VALUES (?, ?, ?)",
        [STMT_SET_NODE_PROP_TEXT] = "INSERT OR REPLACE INTO node_props_text (node_id, key_id, value) VALUES (?, ?, ?)",
        [STMT_SET_NODE_PROP_REAL] = "INSERT OR REPLACE INTO node_props_real (node_id, key_id, value) VALUES (?, ?, ?)",
        [STMT_SET_NODE_PROP_BOOL] = "INSERT OR REPLACE INTO node_props_bool (node_id, key_id, value) VALUES (?, ?, ?)",
        
        [STMT_GET_NODE_PROP_INT] = "SELECT value FROM node_props_int WHERE node_id = ? AND key_id = ?",
        [STMT_GET_NODE_PROP_TEXT] = "SELECT value FROM node_props_text WHERE node_id = ? AND key_id = ?",
        [STMT_GET_NODE_PROP_REAL] = "SELECT value FROM node_props_real WHERE node_id = ? AND key_id = ?",
        [STMT_GET_NODE_PROP_BOOL] = "SELECT value FROM node_props_bool WHERE node_id = ? AND key_id = ?",
        
        [STMT_DEL_NODE_PROP_INT] = "DELETE FROM node_props_int WHERE node_id = ? AND key_id = ?",
        [STMT_DEL_NODE_PROP_TEXT] = "DELETE FROM node_props_text WHERE node_id = ? AND key_id = ?",
        [STMT_DEL_NODE_PROP_REAL] = "DELETE FROM node_props_real WHERE node_id = ? AND key_id = ?",
        [STMT_DEL_NODE_PROP_BOOL] = "DELETE FROM node_props_bool WHERE node_id = ? AND key_id = ?",
        
        // Edge property operations (similar pattern)
        [STMT_SET_EDGE_PROP_INT] = "INSERT OR REPLACE INTO edge_props_int (edge_id, key_id, value) VALUES (?, ?, ?)",
        // ... (similar for other edge property types)
        
        // Label operations
        [STMT_ADD_NODE_LABEL] = "INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)",
        [STMT_REMOVE_NODE_LABEL] = "DELETE FROM node_labels WHERE node_id = ? AND label = ?",
        [STMT_GET_NODE_LABELS] = "SELECT label FROM node_labels WHERE node_id = ?",
        [STMT_FIND_NODES_BY_LABEL] = "SELECT node_id FROM node_labels WHERE label = ?",
        
        // Property key management
        [STMT_INTERN_PROPERTY_KEY] = "INSERT OR IGNORE INTO property_keys (key) VALUES (?)",
        [STMT_LOOKUP_PROPERTY_KEY] = "SELECT id FROM property_keys WHERE key = ?"
    };
    
    // Prepare all fixed statements
    for (int i = 0; i < STMT_COUNT; i++) {
        if (fixed_sqls[i]) {
            int rc = sqlite3_prepare_v2(db->sqlite_db, fixed_sqls[i], -1, 
                                       &mgr->fixed_statements[i], NULL);
            if (rc != SQLITE_OK) {
                cleanup_prepared_statements(db, i);  // Cleanup partial preparation
                return rc;
            }
        }
    }
    
    return SQLITE_OK;
}
```

**Dynamic Statement Caching:**

```c
sqlite3_stmt* get_or_prepare_statement(graphqlite_db_t *db, const char *sql) {
    statement_manager_t *mgr = &db->stmt_manager;
    
    pthread_mutex_lock(&mgr->cache_mutex);
    
    // Search existing cache
    for (size_t i = 0; i < mgr->dynamic_cache_size; i++) {
        if (mgr->dynamic_cache[i].sql && 
            strcmp(mgr->dynamic_cache[i].sql, sql) == 0) {
            
            // Cache hit
            mgr->cache_hits++;
            mgr->dynamic_cache[i].usage_count++;
            mgr->dynamic_cache[i].last_used = time(NULL);
            
            sqlite3_stmt *stmt = mgr->dynamic_cache[i].stmt;
            pthread_mutex_unlock(&mgr->cache_mutex);
            return stmt;
        }
    }
    
    // Cache miss - prepare new statement
    mgr->cache_misses++;
    
    sqlite3_stmt *new_stmt;
    int rc = sqlite3_prepare_v2(db->sqlite_db, sql, -1, &new_stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&mgr->cache_mutex);
        return NULL;
    }
    
    // Add to cache if there's space
    if (mgr->dynamic_cache_size < mgr->dynamic_cache_capacity) {
        size_t index = mgr->dynamic_cache_size++;
        mgr->dynamic_cache[index].stmt = new_stmt;
        mgr->dynamic_cache[index].sql = strdup(sql);
        mgr->dynamic_cache[index].usage_count = 1;
        mgr->dynamic_cache[index].last_used = time(NULL);
        mgr->dynamic_cache[index].is_reusable = true;
    } else {
        // Evict least recently used statement
        size_t lru_index = find_lru_statement(mgr);
        
        // Cleanup old statement
        sqlite3_finalize(mgr->dynamic_cache[lru_index].stmt);
        free(mgr->dynamic_cache[lru_index].sql);
        
        // Replace with new statement
        mgr->dynamic_cache[lru_index].stmt = new_stmt;
        mgr->dynamic_cache[lru_index].sql = strdup(sql);
        mgr->dynamic_cache[lru_index].usage_count = 1;
        mgr->dynamic_cache[lru_index].last_used = time(NULL);
        mgr->dynamic_cache[lru_index].is_reusable = true;
    }
    
    pthread_mutex_unlock(&mgr->cache_mutex);
    return new_stmt;
}

size_t find_lru_statement(statement_manager_t *mgr) {
    size_t lru_index = 0;
    time_t oldest_time = mgr->dynamic_cache[0].last_used;
    
    for (size_t i = 1; i < mgr->dynamic_cache_size; i++) {
        if (mgr->dynamic_cache[i].last_used < oldest_time) {
            oldest_time = mgr->dynamic_cache[i].last_used;
            lru_index = i;
        }
    }
    
    return lru_index;
}
```

**Statement Pool for High-Frequency Operations:**

```c
typedef struct {
    sqlite3_stmt **stmt_pool;
    bool *stmt_in_use;
    size_t pool_size;
    pthread_mutex_t pool_mutex;
    pthread_cond_t stmt_available;
} statement_pool_t;

statement_pool_t* create_statement_pool(graphqlite_db_t *db, 
                                       const char *sql, 
                                       size_t pool_size) {
    statement_pool_t *pool = malloc(sizeof(statement_pool_t));
    pool->stmt_pool = malloc(pool_size * sizeof(sqlite3_stmt*));
    pool->stmt_in_use = calloc(pool_size, sizeof(bool));
    pool->pool_size = pool_size;
    
    pthread_mutex_init(&pool->pool_mutex, NULL);
    pthread_cond_init(&pool->stmt_available, NULL);
    
    // Prepare all statements in pool
    for (size_t i = 0; i < pool_size; i++) {
        int rc = sqlite3_prepare_v2(db->sqlite_db, sql, -1, &pool->stmt_pool[i], NULL);
        if (rc != SQLITE_OK) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                sqlite3_finalize(pool->stmt_pool[j]);
            }
            free(pool->stmt_pool);
            free(pool->stmt_in_use);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

sqlite3_stmt* acquire_pooled_statement(statement_pool_t *pool) {
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Wait for available statement
    while (true) {
        for (size_t i = 0; i < pool->pool_size; i++) {
            if (!pool->stmt_in_use[i]) {
                pool->stmt_in_use[i] = true;
                sqlite3_stmt *stmt = pool->stmt_pool[i];
                pthread_mutex_unlock(&pool->pool_mutex);
                return stmt;
            }
        }
        
        // No statements available, wait
        pthread_cond_wait(&pool->stmt_available, &pool->pool_mutex);
    }
}

void release_pooled_statement(statement_pool_t *pool, sqlite3_stmt *stmt) {
    pthread_mutex_lock(&pool->pool_mutex);
    
    // Find and release statement
    for (size_t i = 0; i < pool->pool_size; i++) {
        if (pool->stmt_pool[i] == stmt) {
            sqlite3_reset(stmt);  // Reset for next use
            sqlite3_clear_bindings(stmt);
            pool->stmt_in_use[i] = false;
            pthread_cond_signal(&pool->stmt_available);
            break;
        }
    }
    
    pthread_mutex_unlock(&pool->pool_mutex);
}
```

**Statement Performance Monitoring:**

```c
typedef struct {
    uint64_t total_executions;
    uint64_t total_execution_time_us;
    uint64_t preparation_count;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double average_execution_time_us;
} statement_stats_t;

void record_statement_execution(graphqlite_db_t *db, 
                               statement_type_t type,
                               uint64_t execution_time_us) {
    statement_stats_t *stats = &db->stmt_manager.stats[type];
    
    stats->total_executions++;
    stats->total_execution_time_us += execution_time_us;
    
    // Update rolling average
    stats->average_execution_time_us = 
        (double)stats->total_execution_time_us / stats->total_executions;
}

statement_stats_t* get_statement_statistics(graphqlite_db_t *db) {
    return db->stmt_manager.stats;
}
```

**Automatic Statement Cleanup:**

```c
int cleanup_expired_statements(graphqlite_db_t *db) {
    statement_manager_t *mgr = &db->stmt_manager;
    time_t now = time(NULL);
    
    pthread_mutex_lock(&mgr->cache_mutex);
    
    size_t removed = 0;
    for (size_t i = 0; i < mgr->dynamic_cache_size; i++) {
        if (now - mgr->dynamic_cache[i].last_used > mgr->statement_ttl_seconds) {
            // Statement expired
            sqlite3_finalize(mgr->dynamic_cache[i].stmt);
            free(mgr->dynamic_cache[i].sql);
            
            // Shift remaining statements down
            memmove(&mgr->dynamic_cache[i], 
                   &mgr->dynamic_cache[i + 1],
                   (mgr->dynamic_cache_size - i - 1) * sizeof(prepared_statement_entry_t));
            
            mgr->dynamic_cache_size--;
            removed++;
            i--;  // Recheck this index
        }
    }
    
    pthread_mutex_unlock(&mgr->cache_mutex);
    return removed;
}
```

**Critical Performance Optimizations:**
- Fixed statements pre-compiled at initialization for zero preparation overhead
- LRU cache eviction ensures most frequently used statements remain available
- Statement pooling for high-concurrency scenarios eliminates lock contention
- Thread-safe caching with minimal lock duration
- Automatic cleanup prevents unbounded memory growth
- Performance monitoring enables optimization identification

## Status Updates

*To be added during implementation*