#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Edge CRUD Operations
// ============================================================================

int64_t graphqlite_create_edge(graphqlite_db_t *db, int64_t source_id,
                              int64_t target_id, const char *type) {
    if (!db || !db->sqlite_db || source_id <= 0 || target_id <= 0 || !type) {
        return -1;
    }
    
    // Validate source and target nodes exist
    if (!graphqlite_node_exists(db, source_id) || !graphqlite_node_exists(db, target_id)) {
        return -1;  // Referential integrity violation
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_CREATE_EDGE);
    if (!stmt) {
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, source_id);
    sqlite3_bind_int64(stmt, 2, target_id);
    sqlite3_bind_text(stmt, 3, type, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    if (rc == SQLITE_DONE) {
        return sqlite3_last_insert_rowid(db->sqlite_db);
    }
    
    return -1;
}

int graphqlite_delete_edge(graphqlite_db_t *db, int64_t edge_id) {
    if (!db || !db->sqlite_db || edge_id <= 0) {
        return SQLITE_ERROR;
    }
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_DELETE_EDGE);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    sqlite3_bind_int64(stmt, 1, edge_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

bool graphqlite_edge_exists(graphqlite_db_t *db, int64_t edge_id) {
    if (!db || !db->sqlite_db || edge_id <= 0) {
        return false;
    }
    
    // Use a simple query to check if edge exists
    const char *sql = "SELECT 1 FROM edges WHERE id = ? LIMIT 1";
    sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
    if (!stmt) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, edge_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_ROW);
}

// ============================================================================
// Edge Query Operations
// ============================================================================

int64_t* graphqlite_get_outgoing_edges(graphqlite_db_t *db, int64_t node_id,
                                      const char *type, size_t *count) {
    if (!db || !db->sqlite_db || node_id <= 0 || !count) {
        return NULL;
    }
    
    *count = 0;
    
    sqlite3_stmt *stmt;
    if (type) {
        stmt = get_prepared_statement(db, STMT_GET_OUTGOING_EDGES_BY_TYPE);
    } else {
        stmt = get_prepared_statement(db, STMT_GET_OUTGOING_EDGES);
    }
    
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    if (type) {
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    }
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    sqlite3_reset(stmt);
    
    if (result_count == 0) {
        return NULL;
    }
    
    // Allocate result array
    int64_t *results = malloc(result_count * sizeof(int64_t));
    if (!results) {
        return NULL;
    }
    
    // Second pass: collect results
    sqlite3_bind_int64(stmt, 1, node_id);
    if (type) {
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    }
    
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < result_count) {
        results[index++] = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_reset(stmt);
    
    *count = result_count;
    return results;
}

int64_t* graphqlite_get_incoming_edges(graphqlite_db_t *db, int64_t node_id,
                                      const char *type, size_t *count) {
    if (!db || !db->sqlite_db || node_id <= 0 || !count) {
        return NULL;
    }
    
    *count = 0;
    
    sqlite3_stmt *stmt;
    if (type) {
        stmt = get_prepared_statement(db, STMT_GET_INCOMING_EDGES_BY_TYPE);
    } else {
        stmt = get_prepared_statement(db, STMT_GET_INCOMING_EDGES);
    }
    
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    if (type) {
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    }
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    sqlite3_reset(stmt);
    
    if (result_count == 0) {
        return NULL;
    }
    
    // Allocate result array
    int64_t *results = malloc(result_count * sizeof(int64_t));
    if (!results) {
        return NULL;
    }
    
    // Second pass: collect results
    sqlite3_bind_int64(stmt, 1, node_id);
    if (type) {
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    }
    
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < result_count) {
        results[index++] = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_reset(stmt);
    
    *count = result_count;
    return results;
}

int64_t* graphqlite_get_neighbors(graphqlite_db_t *db, int64_t node_id,
                                 const char *edge_type, bool outgoing, size_t *count) {
    if (!db || !db->sqlite_db || node_id <= 0 || !count) {
        return NULL;
    }
    
    *count = 0;
    
    sqlite3_stmt *stmt;
    if (outgoing) {
        stmt = edge_type ? get_prepared_statement(db, STMT_GET_OUTGOING_NEIGHBORS_BY_TYPE) :
                          get_prepared_statement(db, STMT_GET_OUTGOING_NEIGHBORS);
    } else {
        stmt = edge_type ? get_prepared_statement(db, STMT_GET_INCOMING_NEIGHBORS_BY_TYPE) :
                          get_prepared_statement(db, STMT_GET_INCOMING_NEIGHBORS);
    }
    
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    if (edge_type) {
        sqlite3_bind_text(stmt, 2, edge_type, -1, SQLITE_STATIC);
    }
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    sqlite3_reset(stmt);
    
    if (result_count == 0) {
        return NULL;
    }
    
    // Allocate result array
    int64_t *results = malloc(result_count * sizeof(int64_t));
    if (!results) {
        return NULL;
    }
    
    // Second pass: collect results
    sqlite3_bind_int64(stmt, 1, node_id);
    if (edge_type) {
        sqlite3_bind_text(stmt, 2, edge_type, -1, SQLITE_STATIC);
    }
    
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < result_count) {
        results[index++] = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_reset(stmt);
    
    *count = result_count;
    return results;
}

// ============================================================================
// Edge Information Queries
// ============================================================================

typedef struct {
    int64_t edge_id;
    int64_t source_id;
    int64_t target_id;
    char *type;
} edge_info_t;

edge_info_t* graphqlite_get_edge_info(graphqlite_db_t *db, int64_t edge_id) {
    if (!db || !db->sqlite_db || edge_id <= 0) {
        return NULL;
    }
    
    const char *sql = "SELECT id, source_id, target_id, type FROM edges WHERE id = ?";
    sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
    if (!stmt) {
        return NULL;
    }
    
    sqlite3_bind_int64(stmt, 1, edge_id);
    
    edge_info_t *info = NULL;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        info = malloc(sizeof(edge_info_t));
        if (info) {
            info->edge_id = sqlite3_column_int64(stmt, 0);
            info->source_id = sqlite3_column_int64(stmt, 1);
            info->target_id = sqlite3_column_int64(stmt, 2);
            const char *type = (const char*)sqlite3_column_text(stmt, 3);
            info->type = strdup(type ? type : "");
        }
    }
    
    sqlite3_reset(stmt);
    return info;
}

void graphqlite_free_edge_info(edge_info_t *info) {
    if (info) {
        free(info->type);
        free(info);
    }
}

int64_t graphqlite_get_edge_source(graphqlite_db_t *db, int64_t edge_id) {
    edge_info_t *info = graphqlite_get_edge_info(db, edge_id);
    if (!info) {
        return -1;
    }
    
    int64_t source_id = info->source_id;
    graphqlite_free_edge_info(info);
    
    return source_id;
}

int64_t graphqlite_get_edge_target(graphqlite_db_t *db, int64_t edge_id) {
    edge_info_t *info = graphqlite_get_edge_info(db, edge_id);
    if (!info) {
        return -1;
    }
    
    int64_t target_id = info->target_id;
    graphqlite_free_edge_info(info);
    
    return target_id;
}

char* graphqlite_get_edge_type(graphqlite_db_t *db, int64_t edge_id) {
    edge_info_t *info = graphqlite_get_edge_info(db, edge_id);
    if (!info) {
        return NULL;
    }
    
    char *type = strdup(info->type);
    graphqlite_free_edge_info(info);
    
    return type;
}

// ============================================================================
// Batch Edge Operations
// ============================================================================

typedef struct {
    int64_t source_id;
    int64_t target_id;
    char *type;
} edge_batch_t;

int graphqlite_create_edges_batch(graphqlite_db_t *db, edge_batch_t *edges, 
                                 size_t count, int64_t *result_ids) {
    if (!db || !edges || count == 0) {
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
    
    sqlite3_stmt *stmt = get_prepared_statement(db, STMT_CREATE_EDGE);
    if (!stmt) {
        if (started_transaction) {
            graphqlite_rollback_transaction(db);
        }
        return SQLITE_ERROR;
    }
    
    for (size_t i = 0; i < count; i++) {
        // Validate nodes exist
        if (!graphqlite_node_exists(db, edges[i].source_id) ||
            !graphqlite_node_exists(db, edges[i].target_id)) {
            if (started_transaction) {
                graphqlite_rollback_transaction(db);
            }
            return SQLITE_ERROR;
        }
        
        sqlite3_bind_int64(stmt, 1, edges[i].source_id);
        sqlite3_bind_int64(stmt, 2, edges[i].target_id);
        sqlite3_bind_text(stmt, 3, edges[i].type, -1, SQLITE_STATIC);
        
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_reset(stmt);
            if (started_transaction) {
                graphqlite_rollback_transaction(db);
            }
            return SQLITE_ERROR;
        }
        
        if (result_ids) {
            result_ids[i] = sqlite3_last_insert_rowid(db->sqlite_db);
        }
        
        sqlite3_reset(stmt);
    }
    
    if (started_transaction) {
        return graphqlite_commit_transaction(db);
    }
    
    return SQLITE_OK;
}

// ============================================================================
// Edge Type Operations
// ============================================================================

char** graphqlite_get_edge_types(graphqlite_db_t *db, size_t *count) {
    if (!db || !db->sqlite_db || !count) {
        return NULL;
    }
    
    *count = 0;
    
    const char *sql = "SELECT DISTINCT type FROM edges ORDER BY type";
    sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
    if (!stmt) {
        return NULL;
    }
    
    // First pass: count types
    size_t type_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        type_count++;
    }
    sqlite3_reset(stmt);
    
    if (type_count == 0) {
        return NULL;
    }
    
    // Allocate array for types
    char **types = malloc(type_count * sizeof(char*));
    if (!types) {
        return NULL;
    }
    
    // Second pass: collect types
    size_t index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < type_count) {
        const char *type = (const char*)sqlite3_column_text(stmt, 0);
        types[index] = strdup(type ? type : "");
        if (!types[index]) {
            // Cleanup on failure
            for (size_t i = 0; i < index; i++) {
                free(types[i]);
            }
            free(types);
            sqlite3_reset(stmt);
            return NULL;
        }
        index++;
    }
    
    sqlite3_reset(stmt);
    
    *count = type_count;
    return types;
}

void graphqlite_free_edge_types(char **types, size_t count) {
    if (!types) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        free(types[i]);
    }
    free(types);
}

// ============================================================================
// Edge Validation and Utilities
// ============================================================================

bool graphqlite_is_valid_edge_id(int64_t edge_id) {
    return edge_id > 0;
}

int graphqlite_count_edges_by_type(graphqlite_db_t *db, const char *type, size_t *count) {
    if (!db || !db->sqlite_db || !count) {
        return SQLITE_ERROR;
    }
    
    const char *sql = type ? 
        "SELECT COUNT(*) FROM edges WHERE type = ?" :
        "SELECT COUNT(*) FROM edges";
    
    sqlite3_stmt *stmt = get_or_prepare_dynamic_statement(db, sql);
    if (!stmt) {
        return SQLITE_ERROR;
    }
    
    if (type) {
        sqlite3_bind_text(stmt, 1, type, -1, SQLITE_STATIC);
    }
    
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *count = sqlite3_column_int64(stmt, 0);
    } else {
        *count = 0;
        rc = SQLITE_ERROR;
    }
    
    sqlite3_reset(stmt);
    return (rc == SQLITE_ROW) ? SQLITE_OK : SQLITE_ERROR;
}

