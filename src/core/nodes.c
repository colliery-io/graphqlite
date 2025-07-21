#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Node CRUD Operations
// ============================================================================

int64_t graphqlite_create_node(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return -1;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_CREATE_NODE);
    if (!stmt) {
        return -1;
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    if (rc == SQLITE_DONE) {
        return sqlite3_last_insert_rowid(db->sqlite_db);
    }
    
    return -1;
}

int graphqlite_delete_node(graphqlite_db_t *db, int64_t node_id) {
    if (!db || !db->sqlite_db || node_id <= 0) {
        return SQLITE_ERROR;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_DELETE_NODE);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

bool graphqlite_node_exists(graphqlite_db_t *db, int64_t node_id) {
    if (!db || !db->sqlite_db || node_id <= 0) {
        return false;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_NODE_EXISTS);
    if (!stmt) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_ROW);
}

// ============================================================================
// Node Label Operations
// ============================================================================

int graphqlite_add_node_label(graphqlite_db_t *db, int64_t node_id, const char *label) {
    if (!db || !db->sqlite_db || node_id <= 0 || !label) {
        return SQLITE_ERROR;
    }
    
    // Verify node exists
    if (!graphqlite_node_exists(db, node_id)) {
        return SQLITE_ERROR;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_ADD_NODE_LABEL);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

int graphqlite_remove_node_label(graphqlite_db_t *db, int64_t node_id, const char *label) {
    if (!db || !db->sqlite_db || node_id <= 0 || !label) {
        return SQLITE_ERROR;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_REMOVE_NODE_LABEL);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

char** graphqlite_get_node_labels(graphqlite_db_t *db, int64_t node_id, size_t *count) {
    if (!db || !db->sqlite_db || node_id <= 0 || !count) {
        return NULL;
    }
    
    *count = 0;
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_GET_NODE_LABELS);
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    
    // First pass: count labels
    size_t label_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        label_count++;
    }
    sqlite3_reset(stmt);
    
    if (label_count == 0) {
        return NULL;
    }
    
    // Allocate array for labels
    char **labels = malloc(label_count * sizeof(char*));
    if (!labels) {
        return NULL;
    }
    
    // Second pass: collect labels
    sqlite3_bind_int64(stmt, 1, node_id);
    
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < label_count) {
        const char *label = (const char*)sqlite3_column_text(stmt, 0);
        labels[index] = strdup(label);
        if (!labels[index]) {
            // Cleanup on failure
            for (size_t i = 0; i < index; i++) {
                free(labels[i]);
            }
            free(labels);
            sqlite3_reset(stmt);
            return NULL;
        }
        index++;
    }
    
    sqlite3_reset(stmt);
    
    *count = label_count;
    return labels;
}

void graphqlite_free_labels(char **labels, size_t count) {
    if (!labels) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        free(labels[i]);
    }
    free(labels);
}

int64_t* graphqlite_find_nodes_by_label(graphqlite_db_t *db, const char *label, size_t *count) {
    if (!db || !db->sqlite_db || !label || !count) {
        return NULL;
    }
    
    *count = 0;
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_FIND_NODES_BY_LABEL);
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, label, -1, SQLITE_STATIC);
    
    // First pass: count nodes
    size_t node_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        node_count++;
    }
    sqlite3_reset(stmt);
    
    if (node_count == 0) {
        return NULL;
    }
    
    // Allocate array for node IDs
    int64_t *node_ids = malloc(node_count * sizeof(int64_t));
    if (!node_ids) {
        return NULL;
    }
    
    // Second pass: collect node IDs
    sqlite3_bind_text(stmt, 1, label, -1, SQLITE_STATIC);
    
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < node_count) {
        node_ids[index] = sqlite3_column_int64(stmt, 0);
        index++;
    }
    
    sqlite3_reset(stmt);
    
    *count = node_count;
    return node_ids;
}

// ============================================================================
// Batch Node Operations
// ============================================================================

int graphqlite_create_nodes_batch(graphqlite_db_t *db, size_t count, int64_t *result_ids) {
    if (!db || count == 0 || !result_ids) {
        return SQLITE_ERROR;
    }
    
    // Use transaction for batch operation
    bool started_transaction = false;
    if (!graphqlite_in_transaction(db)) {
        int rc = graphqlite_begin_transaction(db);
        if (rc != SQLITE_OK) {
            return rc;
        }
        started_transaction = true;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_CREATE_NODE);
    if (!stmt) {
        if (started_transaction) {
            graphqlite_rollback_transaction(db);
        }
        return SQLITE_ERROR;
    }
    
    for (size_t i = 0; i < count; i++) {
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_reset(stmt);
            if (started_transaction) {
                graphqlite_rollback_transaction(db);
            }
            return SQLITE_ERROR;
        }
        
        result_ids[i] = sqlite3_last_insert_rowid(db->sqlite_db);
        sqlite3_reset(stmt);
    }
    
    if (started_transaction) {
        return graphqlite_commit_transaction(db);
    }
    
    return SQLITE_OK;
}

int graphqlite_create_nodes_with_labels_batch(graphqlite_db_t *db,
                                             size_t count,
                                             const char ***label_arrays,
                                             size_t *label_counts,
                                             int64_t *result_ids) {
    if (!db || count == 0 || !result_ids) {
        return SQLITE_ERROR;
    }
    
    // Create nodes first
    int rc = graphqlite_create_nodes_batch(db, count, result_ids);
    if (rc != SQLITE_OK) {
        return rc;
    }
    
    // Add labels if provided
    if (label_arrays && label_counts) {
        bool started_transaction = false;
        if (!graphqlite_in_transaction(db)) {
            rc = graphqlite_begin_transaction(db);
            if (rc != SQLITE_OK) {
                return rc;
            }
            started_transaction = true;
        }
        
        for (size_t i = 0; i < count; i++) {
            if (label_arrays[i] && label_counts[i] > 0) {
                for (size_t j = 0; j < label_counts[i]; j++) {
                    rc = graphqlite_add_node_label(db, result_ids[i], label_arrays[i][j]);
                    if (rc != SQLITE_OK) {
                        if (started_transaction) {
                            graphqlite_rollback_transaction(db);
                        }
                        return rc;
                    }
                }
            }
        }
        
        if (started_transaction) {
            rc = graphqlite_commit_transaction(db);
            if (rc != SQLITE_OK) {
                return rc;
            }
        }
    }
    
    return SQLITE_OK;
}

// ============================================================================
// Node Validation and Utilities
// ============================================================================

bool graphqlite_is_valid_node_id(int64_t node_id) {
    return node_id > 0;
}

int graphqlite_get_node_degree(graphqlite_db_t *db, int64_t node_id, 
                              size_t *in_degree, size_t *out_degree) {
    if (!db || !db->sqlite_db || node_id <= 0) {
        return SQLITE_ERROR;
    }
    
    if (in_degree) {
        size_t count;
        int64_t *edges = graphqlite_get_incoming_edges(db, node_id, NULL, &count);
        *in_degree = count;
        free(edges);
    }
    
    if (out_degree) {
        size_t count;
        int64_t *edges = graphqlite_get_outgoing_edges(db, node_id, NULL, &count);
        *out_degree = count;
        free(edges);
    }
    
    return SQLITE_OK;
}

