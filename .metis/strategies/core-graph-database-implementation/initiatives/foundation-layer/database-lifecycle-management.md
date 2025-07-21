---
id: database-lifecycle-management
level: task
title: "Database lifecycle management"
created_at: 2025-07-19T23:02:08.242758+00:00
updated_at: 2025-07-19T23:02:08.242758+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Database lifecycle management

## Parent Initiative

[[foundation-layer]]

## Objective

Implement the core database lifecycle management functions that handle database creation, opening, closing, and mode switching between interactive and bulk operations. This system provides the foundation API that all other components depend on and ensures proper resource management throughout the database lifetime.

The lifecycle management must support both standalone database files and SQLite extension loading scenarios, with proper initialization of all subsystems including schema creation, property key caching, and prepared statement management.

## Acceptance Criteria

- [ ] **Database Opening**: graphqlite_open() successfully opens/creates database files with proper initialization
- [ ] **Schema Initialization**: Automatic schema creation for new databases, validation for existing ones
- [ ] **Mode Management**: Support for interactive and bulk modes with proper mode switching
- [ ] **Resource Cleanup**: graphqlite_close() properly releases all resources (statements, cache, memory)
- [ ] **Extension Integration**: Database functions work correctly when loaded as SQLite extension
- [ ] **Error Handling**: Clear error codes and messages for all failure scenarios
- [ ] **Performance**: Database open/close operations complete in <10ms
- [ ] **Concurrency**: Thread-safe operations for multi-threaded applications

## Implementation Notes

**Core Database Handle:**

```c
typedef enum {
    GRAPHQLITE_MODE_INTERACTIVE,  // Full ACID, indexes active
    GRAPHQLITE_MODE_BULK_IMPORT   // Deferred indexing, relaxed durability
} graphqlite_mode_t;

typedef struct {
    sqlite3 *sqlite_db;
    graphqlite_mode_t current_mode;
    property_key_cache_t *key_cache;
    sqlite3_stmt *prepared_statements[STMT_COUNT];
    pthread_mutex_t db_mutex;
    int schema_version;
    bool is_extension_mode;
} graphqlite_db_t;
```

**Lifecycle API:**
```c
// Open/create database
graphqlite_db_t* graphqlite_open(const char *filename);
graphqlite_db_t* graphqlite_open_memory(void);

// Mode management
int graphqlite_set_mode(graphqlite_db_t *db, graphqlite_mode_t mode);
graphqlite_mode_t graphqlite_get_mode(graphqlite_db_t *db);

// Resource cleanup
int graphqlite_close(graphqlite_db_t *db);

// Extension integration
int sqlite3_graphqlite_init(sqlite3 *db, char **pzErrMsg, 
                           const sqlite3_api_routines *pApi);
```

**Database Opening Sequence:**
```c
graphqlite_db_t* graphqlite_open(const char *filename) {
    graphqlite_db_t *db = calloc(1, sizeof(graphqlite_db_t));
    if (!db) return NULL;
    
    // 1. Open SQLite database
    int rc = sqlite3_open(filename, &db->sqlite_db);
    if (rc != SQLITE_OK) {
        free(db);
        return NULL;
    }
    
    // 2. Initialize mutex for thread safety
    pthread_mutex_init(&db->db_mutex, NULL);
    
    // 3. Enable foreign keys
    sqlite3_exec(db->sqlite_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
    
    // 4. Check/create schema
    if (ensure_schema_current(db) != 0) {
        graphqlite_close(db);
        return NULL;
    }
    
    // 5. Initialize property key cache
    db->key_cache = init_property_key_cache(db->sqlite_db);
    if (!db->key_cache) {
        graphqlite_close(db);
        return NULL;
    }
    
    // 6. Prepare common statements
    if (prepare_statements(db) != 0) {
        graphqlite_close(db);
        return NULL;
    }
    
    // 7. Set default mode
    db->current_mode = GRAPHQLITE_MODE_INTERACTIVE;
    
    return db;
}
```

**Mode Switching Implementation:**
```c
int graphqlite_set_mode(graphqlite_db_t *db, graphqlite_mode_t mode) {
    if (db->current_mode == mode) return SQLITE_OK;
    
    pthread_mutex_lock(&db->db_mutex);
    
    if (mode == GRAPHQLITE_MODE_BULK_IMPORT) {
        // Optimize for bulk operations
        sqlite3_exec(db->sqlite_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);
        sqlite3_exec(db->sqlite_db, "PRAGMA journal_mode = MEMORY", NULL, NULL, NULL);
        sqlite3_exec(db->sqlite_db, "PRAGMA cache_size = 20000", NULL, NULL, NULL);
    } else {
        // Restore ACID guarantees
        sqlite3_exec(db->sqlite_db, "PRAGMA synchronous = NORMAL", NULL, NULL, NULL);
        sqlite3_exec(db->sqlite_db, "PRAGMA journal_mode = DELETE", NULL, NULL, NULL);
        sqlite3_exec(db->sqlite_db, "PRAGMA cache_size = 2000", NULL, NULL, NULL);
    }
    
    db->current_mode = mode;
    pthread_mutex_unlock(&db->db_mutex);
    return SQLITE_OK;
}
```

**SQLite Extension Entry Point:**
```c
int sqlite3_graphqlite_init(sqlite3 *db, char **pzErrMsg, 
                           const sqlite3_api_routines *pApi) {
    SQLITE_EXTENSION_INIT2(pApi);
    
    // Create extension-mode database handle
    graphqlite_db_t *gql_db = calloc(1, sizeof(graphqlite_db_t));
    gql_db->sqlite_db = db;
    gql_db->is_extension_mode = true;
    
    // Initialize subsystems
    ensure_schema_current(gql_db);
    gql_db->key_cache = init_property_key_cache(db);
    prepare_statements(gql_db);
    
    // Register custom functions
    sqlite3_create_function(db, "gql_query", 1, SQLITE_UTF8, gql_db,
                           gql_query_function, NULL, NULL);
    
    // Store handle in SQLite user data for cleanup
    sqlite3_user_data(db) = gql_db;
    
    return SQLITE_OK;
}
```

**Resource Cleanup:**
```c
int graphqlite_close(graphqlite_db_t *db) {
    if (!db) return SQLITE_OK;
    
    // Finalize prepared statements
    for (int i = 0; i < STMT_COUNT; i++) {
        if (db->prepared_statements[i]) {
            sqlite3_finalize(db->prepared_statements[i]);
        }
    }
    
    // Cleanup property key cache
    if (db->key_cache) {
        cleanup_property_key_cache(db->key_cache);
    }
    
    // Close SQLite (only if not extension mode)
    if (!db->is_extension_mode && db->sqlite_db) {
        sqlite3_close(db->sqlite_db);
    }
    
    // Cleanup mutex
    pthread_mutex_destroy(&db->db_mutex);
    
    free(db);
    return SQLITE_OK;
}
```

**Error Handling:**
- Define comprehensive error codes for all failure scenarios
- Provide detailed error messages for debugging
- Ensure partial initialization cleanup on failures
- Log critical errors for debugging

## Status Updates

*To be added during implementation*