---
id: transaction-management
level: task
title: "Transaction management"
created_at: 2025-07-19T23:02:44.699555+00:00
updated_at: 2025-07-19T23:02:44.699555+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Transaction management

## Parent Initiative

[[foundation-layer]]

## Objective

Implement comprehensive transaction management that provides ACID guarantees for all graph operations. The system must support explicit transactions, automatic transaction handling for batch operations, and proper rollback capabilities while maintaining the performance benefits of SQLite's transaction system.

Transaction management is critical for data integrity in graph databases where complex operations often span multiple nodes, edges, and properties that must be atomically committed or rolled back together.

## Acceptance Criteria

- [ ] **ACID Compliance**: Full Atomicity, Consistency, Isolation, and Durability guarantees
- [ ] **Explicit Transactions**: Support for manual begin/commit/rollback operations
- [ ] **Automatic Transactions**: Transparent transaction wrapping for batch operations
- [ ] **Nested Transactions**: Support for savepoints and nested transaction scenarios
- [ ] **Error Recovery**: Automatic rollback on operation failures with clear error reporting
- [ ] **Thread Safety**: Transaction isolation across concurrent operations
- [ ] **Performance**: Transaction overhead <1ms for simple operations
- [ ] **Deadlock Prevention**: Proper lock ordering and timeout handling

## Implementation Notes

**Transaction State Management:**

```c
typedef enum {
    TX_STATE_NONE,        // No active transaction
    TX_STATE_ACTIVE,      // Transaction in progress
    TX_STATE_COMMITTED,   // Transaction committed
    TX_STATE_ABORTED     // Transaction rolled back
} transaction_state_t;

typedef struct {
    transaction_state_t state;
    int nesting_level;    // For savepoint support
    bool auto_transaction; // Automatically managed transaction
    char *savepoint_name; // Current savepoint name
    pthread_mutex_t tx_mutex; // Thread safety
} transaction_context_t;
```

**Transaction API:**

```c
// Explicit transaction control
int graphqlite_begin_transaction(graphqlite_db_t *db);
int graphqlite_commit_transaction(graphqlite_db_t *db);
int graphqlite_rollback_transaction(graphqlite_db_t *db);

// Savepoint support for nested transactions
int graphqlite_savepoint(graphqlite_db_t *db, const char *name);
int graphqlite_release_savepoint(graphqlite_db_t *db, const char *name);
int graphqlite_rollback_to_savepoint(graphqlite_db_t *db, const char *name);

// Transaction state queries
bool graphqlite_in_transaction(graphqlite_db_t *db);
transaction_state_t graphqlite_transaction_state(graphqlite_db_t *db);

// Automatic transaction wrapper
int graphqlite_with_transaction(graphqlite_db_t *db, 
                               int (*operation)(graphqlite_db_t *db, void *data),
                               void *data);
```

**Core Transaction Implementation:**

```c
int graphqlite_begin_transaction(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return SQLITE_ERROR;
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    
    if (db->tx_state.state == TX_STATE_ACTIVE) {
        // Handle nested transaction with savepoint
        db->tx_state.nesting_level++;
        char savepoint_name[64];
        snprintf(savepoint_name, sizeof(savepoint_name), "sp_%d", db->tx_state.nesting_level);
        
        char sql[128];
        snprintf(sql, sizeof(sql), "SAVEPOINT %s", savepoint_name);
        int rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
        
        if (rc == SQLITE_OK) {
            free(db->tx_state.savepoint_name);
            db->tx_state.savepoint_name = strdup(savepoint_name);
        }
        
        pthread_mutex_unlock(&db->tx_state.tx_mutex);
        return rc;
    }
    
    // Begin new transaction
    int rc = sqlite3_exec(db->sqlite_db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    if (rc == SQLITE_OK) {
        db->tx_state.state = TX_STATE_ACTIVE;
        db->tx_state.nesting_level = 0;
        db->tx_state.auto_transaction = false;
    }
    
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    return rc;
}

int graphqlite_commit_transaction(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return SQLITE_ERROR;
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    
    if (db->tx_state.state != TX_STATE_ACTIVE) {
        pthread_mutex_unlock(&db->tx_state.tx_mutex);
        return SQLITE_ERROR; // No active transaction
    }
    
    int rc;
    if (db->tx_state.nesting_level > 0) {
        // Release savepoint
        char sql[128];
        snprintf(sql, sizeof(sql), "RELEASE SAVEPOINT %s", db->tx_state.savepoint_name);
        rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
        
        db->tx_state.nesting_level--;
        if (db->tx_state.nesting_level == 0) {
            free(db->tx_state.savepoint_name);
            db->tx_state.savepoint_name = NULL;
        }
    } else {
        // Commit main transaction
        rc = sqlite3_exec(db->sqlite_db, "COMMIT", NULL, NULL, NULL);
        if (rc == SQLITE_OK) {
            db->tx_state.state = TX_STATE_COMMITTED;
        }
    }
    
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    return rc;
}

int graphqlite_rollback_transaction(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return SQLITE_ERROR;
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    
    if (db->tx_state.state != TX_STATE_ACTIVE) {
        pthread_mutex_unlock(&db->tx_state.tx_mutex);
        return SQLITE_ERROR; // No active transaction
    }
    
    int rc;
    if (db->tx_state.nesting_level > 0) {
        // Rollback to savepoint
        char sql[128];
        snprintf(sql, sizeof(sql), "ROLLBACK TO SAVEPOINT %s", db->tx_state.savepoint_name);
        rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
        
        db->tx_state.nesting_level--;
        if (db->tx_state.nesting_level == 0) {
            free(db->tx_state.savepoint_name);
            db->tx_state.savepoint_name = NULL;
        }
    } else {
        // Rollback main transaction
        rc = sqlite3_exec(db->sqlite_db, "ROLLBACK", NULL, NULL, NULL);
        if (rc == SQLITE_OK) {
            db->tx_state.state = TX_STATE_ABORTED;
        }
    }
    
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    return rc;
}
```

**Automatic Transaction Wrapper:**

```c
int graphqlite_with_transaction(graphqlite_db_t *db,
                               int (*operation)(graphqlite_db_t *db, void *data),
                               void *data) {
    if (!db || !operation) return SQLITE_ERROR;
    
    bool started_transaction = false;
    
    // Start transaction if not already in one
    if (!graphqlite_in_transaction(db)) {
        int rc = graphqlite_begin_transaction(db);
        if (rc != SQLITE_OK) return rc;
        
        db->tx_state.auto_transaction = true;
        started_transaction = true;
    }
    
    // Execute operation
    int result = operation(db, data);
    
    // Handle transaction completion
    if (started_transaction) {
        if (result == SQLITE_OK) {
            int rc = graphqlite_commit_transaction(db);
            if (rc != SQLITE_OK) {
                graphqlite_rollback_transaction(db);
                return rc;
            }
        } else {
            graphqlite_rollback_transaction(db);
        }
        
        db->tx_state.auto_transaction = false;
    }
    
    return result;
}
```

**Batch Operation Transaction Management:**

```c
// Example: Batch node creation with automatic transaction
typedef struct {
    property_set_t **property_sets;
    char ***label_sets;
    size_t *label_counts;
    size_t node_count;
    int64_t *result_node_ids;
} batch_node_data_t;

int create_nodes_batch_operation(graphqlite_db_t *db, void *data) {
    batch_node_data_t *batch_data = (batch_node_data_t *)data;
    
    for (size_t i = 0; i < batch_data->node_count; i++) {
        // Create node
        int64_t node_id = graphqlite_create_node(db);
        if (node_id < 0) return SQLITE_ERROR;
        
        batch_data->result_node_ids[i] = node_id;
        
        // Add labels
        if (batch_data->label_sets && batch_data->label_sets[i]) {
            for (size_t j = 0; j < batch_data->label_counts[i]; j++) {
                int rc = graphqlite_add_node_label(db, node_id, batch_data->label_sets[i][j]);
                if (rc != SQLITE_OK) return rc;
            }
        }
        
        // Set properties
        if (batch_data->property_sets && batch_data->property_sets[i]) {
            int rc = graphqlite_set_properties(db, ENTITY_NODE, node_id, 
                                             batch_data->property_sets[i]);
            if (rc != SQLITE_OK) return rc;
        }
    }
    
    return SQLITE_OK;
}

int graphqlite_create_nodes_batch_tx(graphqlite_db_t *db, 
                                    property_set_t **property_sets,
                                    char ***label_sets,
                                    size_t *label_counts,
                                    size_t node_count,
                                    int64_t *result_node_ids) {
    batch_node_data_t batch_data = {
        .property_sets = property_sets,
        .label_sets = label_sets,
        .label_counts = label_counts,
        .node_count = node_count,
        .result_node_ids = result_node_ids
    };
    
    return graphqlite_with_transaction(db, create_nodes_batch_operation, &batch_data);
}
```

**Error Handling and Recovery:**

```c
typedef struct {
    int error_code;
    char *error_message;
    bool transaction_rolled_back;
} transaction_error_t;

transaction_error_t* get_transaction_error(graphqlite_db_t *db) {
    transaction_error_t *error = malloc(sizeof(transaction_error_t));
    error->error_code = sqlite3_errcode(db->sqlite_db);
    error->error_message = strdup(sqlite3_errmsg(db->sqlite_db));
    error->transaction_rolled_back = (db->tx_state.state == TX_STATE_ABORTED);
    return error;
}

void free_transaction_error(transaction_error_t *error) {
    if (error) {
        free(error->error_message);
        free(error);
    }
}
```

**Performance Optimizations:**
- Use IMMEDIATE transactions to reduce lock contention
- Minimize transaction scope to reduce lock duration
- Batch multiple operations within single transactions
- Use savepoints only when necessary for nested operations
- Prepared statements reused across transaction boundaries
- Connection-level transaction state tracking to avoid SQL queries

## Status Updates

*To be added during implementation*