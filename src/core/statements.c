#include "graphqlite_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Forward declarations
static size_t find_lru_statement(statement_manager_t *mgr);

// ============================================================================
// SQL Statement Definitions
// ============================================================================

static const char *FIXED_STATEMENT_SQL[STMT_COUNT] = {
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
    
    // Node property operations
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
    
    // Edge property operations
    [STMT_SET_EDGE_PROP_INT] = "INSERT OR REPLACE INTO edge_props_int (edge_id, key_id, value) VALUES (?, ?, ?)",
    [STMT_SET_EDGE_PROP_TEXT] = "INSERT OR REPLACE INTO edge_props_text (edge_id, key_id, value) VALUES (?, ?, ?)",
    [STMT_SET_EDGE_PROP_REAL] = "INSERT OR REPLACE INTO edge_props_real (edge_id, key_id, value) VALUES (?, ?, ?)",
    [STMT_SET_EDGE_PROP_BOOL] = "INSERT OR REPLACE INTO edge_props_bool (edge_id, key_id, value) VALUES (?, ?, ?)",
    
    [STMT_GET_EDGE_PROP_INT] = "SELECT value FROM edge_props_int WHERE edge_id = ? AND key_id = ?",
    [STMT_GET_EDGE_PROP_TEXT] = "SELECT value FROM edge_props_text WHERE edge_id = ? AND key_id = ?",
    [STMT_GET_EDGE_PROP_REAL] = "SELECT value FROM edge_props_real WHERE edge_id = ? AND key_id = ?",
    [STMT_GET_EDGE_PROP_BOOL] = "SELECT value FROM edge_props_bool WHERE edge_id = ? AND key_id = ?",
    
    [STMT_DEL_EDGE_PROP_INT] = "DELETE FROM edge_props_int WHERE edge_id = ? AND key_id = ?",
    [STMT_DEL_EDGE_PROP_TEXT] = "DELETE FROM edge_props_text WHERE edge_id = ? AND key_id = ?",
    [STMT_DEL_EDGE_PROP_REAL] = "DELETE FROM edge_props_real WHERE edge_id = ? AND key_id = ?",
    [STMT_DEL_EDGE_PROP_BOOL] = "DELETE FROM edge_props_bool WHERE edge_id = ? AND key_id = ?",
    
    // Label operations
    [STMT_ADD_NODE_LABEL] = "INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?)",
    [STMT_REMOVE_NODE_LABEL] = "DELETE FROM node_labels WHERE node_id = ? AND label = ?",
    [STMT_GET_NODE_LABELS] = "SELECT label FROM node_labels WHERE node_id = ?",
    [STMT_FIND_NODES_BY_LABEL] = "SELECT node_id FROM node_labels WHERE label = ?",
    
    // Property key management
    [STMT_INTERN_PROPERTY_KEY] = "INSERT OR IGNORE INTO property_keys (key) VALUES (?)",
    [STMT_LOOKUP_PROPERTY_KEY] = "SELECT id FROM property_keys WHERE key = ?"
};

// ============================================================================
// Statement Manager Implementation
// ============================================================================

int initialize_statement_manager(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) {
        return SQLITE_ERROR;
    }
    
    statement_manager_t *mgr = &db->stmt_manager;
    
    // Initialize structure
    memset(mgr, 0, sizeof(statement_manager_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&mgr->cache_mutex, NULL) != 0) {
        return SQLITE_ERROR;
    }
    
    // Set configuration
    mgr->max_dynamic_statements = 100;
    mgr->statement_ttl_seconds = 300; // 5 minutes
    
    // Prepare all fixed statements
    for (int i = 0; i < STMT_COUNT; i++) {
        if (FIXED_STATEMENT_SQL[i]) {
            int rc = sqlite3_prepare_v2(db->sqlite_db, FIXED_STATEMENT_SQL[i], -1,
                                       &mgr->fixed_statements[i], NULL);
            if (rc != SQLITE_OK) {
                // Cleanup partial preparation
                cleanup_statement_manager(db);
                return rc;
            }
        }
    }
    
    // Initialize dynamic cache
    mgr->dynamic_cache_capacity = mgr->max_dynamic_statements;
    mgr->dynamic_cache = calloc(mgr->dynamic_cache_capacity,
                               sizeof(prepared_statement_entry_t));
    if (!mgr->dynamic_cache) {
        cleanup_statement_manager(db);
        return SQLITE_NOMEM;
    }
    
    return SQLITE_OK;
}

void cleanup_statement_manager(graphqlite_db_t *db) {
    if (!db) {
        return;
    }
    
    statement_manager_t *mgr = &db->stmt_manager;
    
    pthread_mutex_lock(&mgr->cache_mutex);
    
    // Finalize fixed statements
    for (int i = 0; i < STMT_COUNT; i++) {
        if (mgr->fixed_statements[i]) {
            sqlite3_finalize(mgr->fixed_statements[i]);
            mgr->fixed_statements[i] = NULL;
        }
    }
    
    // Cleanup dynamic cache
    if (mgr->dynamic_cache) {
        for (size_t i = 0; i < mgr->dynamic_cache_size; i++) {
            if (mgr->dynamic_cache[i].stmt) {
                sqlite3_finalize(mgr->dynamic_cache[i].stmt);
            }
            free(mgr->dynamic_cache[i].sql);
        }
        free(mgr->dynamic_cache);
        mgr->dynamic_cache = NULL;
    }
    
    pthread_mutex_unlock(&mgr->cache_mutex);
    pthread_mutex_destroy(&mgr->cache_mutex);
    
    memset(mgr, 0, sizeof(statement_manager_t));
}

sqlite3_stmt* get_prepared_statement(graphqlite_db_t *db, statement_type_t type) {
    if (!db || type < 0 || type >= STMT_COUNT) {
        return NULL;
    }
    
    statement_manager_t *mgr = &db->stmt_manager;
    
    // Return fixed statement if available
    if (mgr->fixed_statements[type]) {
        return mgr->fixed_statements[type];
    }
    
    return NULL;
}

sqlite3_stmt* get_or_prepare_dynamic_statement(graphqlite_db_t *db, const char *sql) {
    if (!db || !sql) {
        return NULL;
    }
    
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

static size_t find_lru_statement(statement_manager_t *mgr) {
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

// ============================================================================
// Statement Performance Tracking
// ============================================================================

void record_statement_execution(graphqlite_db_t *db,
                               statement_type_t type,
                               uint64_t execution_time_us) {
    if (!db || type < 0 || type >= STMT_COUNT) {
        return;
    }
    
    statement_stats_t *stats = &db->stmt_manager.stats[type];
    
    stats->total_executions++;
    stats->total_execution_time_us += execution_time_us;
    
    // Update rolling average
    stats->average_execution_time_us =
        (double)stats->total_execution_time_us / stats->total_executions;
}

statement_stats_t* get_statement_statistics(graphqlite_db_t *db) {
    if (!db) {
        return NULL;
    }
    
    return db->stmt_manager.stats;
}

// ============================================================================
// Statement Cache Maintenance
// ============================================================================

int cleanup_expired_statements(graphqlite_db_t *db) {
    if (!db) {
        return 0;
    }
    
    statement_manager_t *mgr = &db->stmt_manager;
    time_t now = time(NULL);
    
    pthread_mutex_lock(&mgr->cache_mutex);
    
    size_t removed = 0;
    for (size_t i = 0; i < mgr->dynamic_cache_size; ) {
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
            // Don't increment i since we shifted elements down
        } else {
            i++;
        }
    }
    
    pthread_mutex_unlock(&mgr->cache_mutex);
    return removed;
}

void get_statement_cache_stats(graphqlite_db_t *db,
                              uint64_t *hits, uint64_t *misses,
                              size_t *cache_size, double *hit_ratio) {
    if (!db) {
        return;
    }
    
    statement_manager_t *mgr = &db->stmt_manager;
    
    pthread_mutex_lock(&mgr->cache_mutex);
    
    if (hits) *hits = mgr->cache_hits;
    if (misses) *misses = mgr->cache_misses;
    if (cache_size) *cache_size = mgr->dynamic_cache_size;
    
    if (hit_ratio) {
        uint64_t total = mgr->cache_hits + mgr->cache_misses;
        *hit_ratio = total > 0 ? (double)mgr->cache_hits / total : 0.0;
    }
    
    pthread_mutex_unlock(&mgr->cache_mutex);
}