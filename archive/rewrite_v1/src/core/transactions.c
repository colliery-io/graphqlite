#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Transaction Management Implementation
// ============================================================================

int graphqlite_begin_transaction(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
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
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
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
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
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

bool graphqlite_in_transaction(graphqlite_db_t *db) {
    if (!db) {
        return false;
    }
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    bool in_tx = (db->tx_state.state == TX_STATE_ACTIVE);
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    
    return in_tx;
}

transaction_state_t graphqlite_transaction_state(graphqlite_db_t *db) {
    if (!db) {
        return TX_STATE_NONE;
    }
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    transaction_state_t state = db->tx_state.state;
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    
    return state;
}

// ============================================================================
// Savepoint Management
// ============================================================================

int graphqlite_savepoint(graphqlite_db_t *db, const char *name) {
    if (!db || !db->sqlite_db || !name) {
        return SQLITE_ERROR;
    }
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    
    if (db->tx_state.state != TX_STATE_ACTIVE) {
        pthread_mutex_unlock(&db->tx_state.tx_mutex);
        return SQLITE_ERROR; // No active transaction
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "SAVEPOINT %s", name);
    int rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    
    if (rc == SQLITE_OK) {
        db->tx_state.nesting_level++;
        free(db->tx_state.savepoint_name);
        db->tx_state.savepoint_name = strdup(name);
    }
    
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    return rc;
}

int graphqlite_release_savepoint(graphqlite_db_t *db, const char *name) {
    if (!db || !db->sqlite_db || !name) {
        return SQLITE_ERROR;
    }
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    
    if (db->tx_state.state != TX_STATE_ACTIVE || db->tx_state.nesting_level == 0) {
        pthread_mutex_unlock(&db->tx_state.tx_mutex);
        return SQLITE_ERROR;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "RELEASE SAVEPOINT %s", name);
    int rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    
    if (rc == SQLITE_OK) {
        db->tx_state.nesting_level--;
        if (db->tx_state.nesting_level == 0) {
            free(db->tx_state.savepoint_name);
            db->tx_state.savepoint_name = NULL;
        }
    }
    
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    return rc;
}

int graphqlite_rollback_to_savepoint(graphqlite_db_t *db, const char *name) {
    if (!db || !db->sqlite_db || !name) {
        return SQLITE_ERROR;
    }
    
    pthread_mutex_lock(&db->tx_state.tx_mutex);
    
    if (db->tx_state.state != TX_STATE_ACTIVE || db->tx_state.nesting_level == 0) {
        pthread_mutex_unlock(&db->tx_state.tx_mutex);
        return SQLITE_ERROR;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "ROLLBACK TO SAVEPOINT %s", name);
    int rc = sqlite3_exec(db->sqlite_db, sql, NULL, NULL, NULL);
    
    pthread_mutex_unlock(&db->tx_state.tx_mutex);
    return rc;
}

// ============================================================================
// Automatic Transaction Wrapper
// ============================================================================

int graphqlite_with_transaction(graphqlite_db_t *db,
                               int (*operation)(graphqlite_db_t *db, void *data),
                               void *data) {
    if (!db || !operation) {
        return SQLITE_ERROR;
    }
    
    bool started_transaction = false;
    
    // Start transaction if not already in one
    if (!graphqlite_in_transaction(db)) {
        int rc = graphqlite_begin_transaction(db);
        if (rc != SQLITE_OK) {
            return rc;
        }
        
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

// ============================================================================
// Transaction Error Handling
// ============================================================================

typedef struct {
    int error_code;
    char *error_message;
    bool transaction_rolled_back;
} transaction_error_t;

transaction_error_t* get_transaction_error(graphqlite_db_t *db) {
    if (!db) {
        return NULL;
    }
    
    transaction_error_t *error = malloc(sizeof(transaction_error_t));
    if (!error) {
        return NULL;
    }
    
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

// ============================================================================
// Transaction Statistics
// ============================================================================

typedef struct {
    uint64_t transactions_started;
    uint64_t transactions_committed;
    uint64_t transactions_rolled_back;
    uint64_t savepoints_created;
    uint64_t total_transaction_time_us;
    double average_transaction_time_us;
} transaction_stats_t;

static transaction_stats_t global_tx_stats = {0};

void record_transaction_completion(graphqlite_db_t *db,
                                 bool committed,
                                 uint64_t transaction_time_us) {
    if (committed) {
        global_tx_stats.transactions_committed++;
    } else {
        global_tx_stats.transactions_rolled_back++;
    }
    
    global_tx_stats.total_transaction_time_us += transaction_time_us;
    
    uint64_t total_tx = global_tx_stats.transactions_committed + 
                       global_tx_stats.transactions_rolled_back;
    
    if (total_tx > 0) {
        global_tx_stats.average_transaction_time_us = 
            (double)global_tx_stats.total_transaction_time_us / total_tx;
    }
}

transaction_stats_t* get_transaction_statistics(void) {
    return &global_tx_stats;
}