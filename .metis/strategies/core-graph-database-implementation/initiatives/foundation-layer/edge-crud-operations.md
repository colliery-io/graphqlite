---
id: edge-crud-operations
level: task
title: "Edge CRUD operations"
created_at: 2025-07-19T23:02:25.416450+00:00
updated_at: 2025-07-19T23:02:25.416450+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Edge CRUD operations

## Parent Initiative

[[foundation-layer]]

## Objective

Implement comprehensive Create, Read, Update, Delete (CRUD) operations for graph edges, including edge creation, deletion, type management, and property operations. Edge operations must maintain referential integrity with nodes and provide efficient graph traversal capabilities.

The edge operations must support high-performance graph queries with proper indexing on source/target node relationships and edge types, enabling the sub-150ms query performance requirements validated in Phase Zero.

## Acceptance Criteria

- [ ] **Edge Creation**: Create edges with source/target validation and automatic ID assignment
- [ ] **Edge Deletion**: Delete edges with proper cleanup of properties
- [ ] **Type Management**: Support edge types with validation and indexing
- [ ] **Property Operations**: Set/get/delete edge properties with type validation
- [ ] **Referential Integrity**: Enforce foreign key constraints to existing nodes
- [ ] **Graph Traversal Performance**: Support efficient neighbor queries and path traversals
- [ ] **Batch Operations**: Support efficient bulk edge creation for import scenarios
- [ ] **Transaction Support**: All operations respect transaction boundaries and rollback properly

## Implementation Notes

**Edge CRUD API:**

```c
// Edge creation
int64_t graphqlite_create_edge(graphqlite_db_t *db, int64_t source_id, 
                              int64_t target_id, const char *type);

// Edge deletion
int graphqlite_delete_edge(graphqlite_db_t *db, int64_t edge_id);

// Edge existence check
bool graphqlite_edge_exists(graphqlite_db_t *db, int64_t edge_id);

// Edge queries
int64_t* graphqlite_get_outgoing_edges(graphqlite_db_t *db, int64_t node_id, 
                                      const char *type, size_t *count);
int64_t* graphqlite_get_incoming_edges(graphqlite_db_t *db, int64_t node_id,
                                      const char *type, size_t *count);
int64_t* graphqlite_get_neighbors(graphqlite_db_t *db, int64_t node_id,
                                 const char *edge_type, bool outgoing, size_t *count);

// Property operations
int graphqlite_set_edge_property(graphqlite_db_t *db, int64_t edge_id,
                                const char *key, property_value_t *value);
int graphqlite_get_edge_property(graphqlite_db_t *db, int64_t edge_id,
                                const char *key, property_value_t *value);
int graphqlite_delete_edge_property(graphqlite_db_t *db, int64_t edge_id, const char *key);

// Batch operations
int graphqlite_create_edges_batch(graphqlite_db_t *db, edge_batch_t *edges, size_t count);
```

**Core Implementation:**

```c
int64_t graphqlite_create_edge(graphqlite_db_t *db, int64_t source_id,
                              int64_t target_id, const char *type) {
    if (!db || !db->sqlite_db || source_id <= 0 || target_id <= 0 || !type) return -1;
    
    // Validate source and target nodes exist
    if (!graphqlite_node_exists(db, source_id) || !graphqlite_node_exists(db, target_id)) {
        return -1;  // Referential integrity violation
    }
    
    // Create edge with prepared statement
    sqlite3_stmt *stmt = db->prepared_statements[STMT_CREATE_EDGE];
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
    if (!db || !db->sqlite_db || edge_id <= 0) return SQLITE_ERROR;
    
    // Delete edge (CASCADE will handle properties)
    sqlite3_stmt *stmt = db->prepared_statements[STMT_DELETE_EDGE];
    sqlite3_bind_int64(stmt, 1, edge_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

int64_t* graphqlite_get_outgoing_edges(graphqlite_db_t *db, int64_t node_id,
                                      const char *type, size_t *count) {
    if (!db || !db->sqlite_db || node_id <= 0 || !count) return NULL;
    
    sqlite3_stmt *stmt;
    if (type) {
        stmt = db->prepared_statements[STMT_GET_OUTGOING_EDGES_BY_TYPE];
        sqlite3_bind_int64(stmt, 1, node_id);
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    } else {
        stmt = db->prepared_statements[STMT_GET_OUTGOING_EDGES];
        sqlite3_bind_int64(stmt, 1, node_id);
    }
    
    // First pass: count results
    size_t result_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result_count++;
    }
    sqlite3_reset(stmt);
    
    if (result_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // Second pass: collect results
    int64_t *results = malloc(result_count * sizeof(int64_t));
    size_t index = 0;
    
    if (type) {
        sqlite3_bind_int64(stmt, 1, node_id);
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_int64(stmt, 1, node_id);
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW && index < result_count) {
        results[index++] = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_reset(stmt);
    
    *count = result_count;
    return results;
}

int64_t* graphqlite_get_neighbors(graphqlite_db_t *db, int64_t node_id,
                                 const char *edge_type, bool outgoing, size_t *count) {
    if (!db || !db->sqlite_db || node_id <= 0 || !count) return NULL;
    
    sqlite3_stmt *stmt;
    if (outgoing) {
        stmt = edge_type ? db->prepared_statements[STMT_GET_OUTGOING_NEIGHBORS_BY_TYPE] :
                          db->prepared_statements[STMT_GET_OUTGOING_NEIGHBORS];
    } else {
        stmt = edge_type ? db->prepared_statements[STMT_GET_INCOMING_NEIGHBORS_BY_TYPE] :
                          db->prepared_statements[STMT_GET_INCOMING_NEIGHBORS];
    }
    
    sqlite3_bind_int64(stmt, 1, node_id);
    if (edge_type) {
        sqlite3_bind_text(stmt, 2, edge_type, -1, SQLITE_STATIC);
    }
    
    // Implementation similar to get_outgoing_edges...
    // [Omitted for brevity - follows same pattern]
}
```

**Prepared Statements:**
```sql
-- Edge creation/deletion
INSERT INTO edges (source_id, target_id, type) VALUES (?, ?, ?);
DELETE FROM edges WHERE id = ?;

-- Edge queries (optimized with indexes)
SELECT id FROM edges WHERE source_id = ?;
SELECT id FROM edges WHERE source_id = ? AND type = ?;
SELECT id FROM edges WHERE target_id = ?;
SELECT id FROM edges WHERE target_id = ? AND type = ?;

-- Neighbor queries (for graph traversal)
SELECT target_id FROM edges WHERE source_id = ?;
SELECT target_id FROM edges WHERE source_id = ? AND type = ?;
SELECT source_id FROM edges WHERE target_id = ?;
SELECT source_id FROM edges WHERE target_id = ? AND type = ?;

-- Property operations (per type)
INSERT OR REPLACE INTO edge_props_int (edge_id, key_id, value) VALUES (?, ?, ?);
INSERT OR REPLACE INTO edge_props_text (edge_id, key_id, value) VALUES (?, ?, ?);
INSERT OR REPLACE INTO edge_props_real (edge_id, key_id, value) VALUES (?, ?, ?);
INSERT OR REPLACE INTO edge_props_bool (edge_id, key_id, value) VALUES (?, ?, ?);
```

**Critical Indexes for Performance:**
```sql
-- Primary traversal indexes
CREATE INDEX idx_edges_source ON edges(source_id, type);
CREATE INDEX idx_edges_target ON edges(target_id, type);
CREATE INDEX idx_edges_type ON edges(type);

-- Edge property indexes (same pattern as node properties)
CREATE INDEX idx_edge_props_int_key_value ON edge_props_int(key_id, value, edge_id);
CREATE INDEX idx_edge_props_text_key_value ON edge_props_text(key_id, value, edge_id);
-- etc.
```

**Performance Optimizations:**
- Use prepared statements for all edge operations
- Neighbor queries leverage source/target indexes for O(log n) performance
- Type filtering uses composite indexes for efficient edge type queries
- Batch edge creation uses transactions for bulk performance
- Edge property operations use same interning system as nodes
- Memory management for result arrays with proper cleanup

## Status Updates

*To be added during implementation*