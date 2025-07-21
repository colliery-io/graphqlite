---
id: node-crud-operations
level: task
title: "Node CRUD operations"
created_at: 2025-07-19T23:02:16.403805+00:00
updated_at: 2025-07-19T23:02:16.403805+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Node CRUD operations

## Parent Initiative

[[foundation-layer]]

## Objective

Implement comprehensive Create, Read, Update, Delete (CRUD) operations for graph nodes, including node creation, deletion, label management, and property operations. This API provides the fundamental building blocks for graph construction and manipulation that higher-level components depend on.

The node operations must achieve 10,000+ nodes/second creation rate in interactive mode while maintaining data integrity through proper transaction handling and referential constraints.

## Acceptance Criteria

- [ ] **Node Creation**: Create nodes with automatic ID assignment and timestamp tracking
- [ ] **Node Deletion**: Delete nodes with proper cleanup of properties, labels, and dependent edges
- [ ] **Label Management**: Add/remove labels with validation and duplicate prevention
- [ ] **Property Operations**: Set/get/delete node properties with type validation
- [ ] **Performance**: Achieve 10,000+ nodes/second creation rate in interactive mode
- [ ] **Transaction Support**: All operations respect transaction boundaries and rollback properly
- [ ] **Referential Integrity**: Foreign key constraints prevent orphaned data
- [ ] **Batch Operations**: Support efficient bulk node creation for import scenarios

## Implementation Notes

**Node CRUD API:**

```c
// Node creation
int64_t graphqlite_create_node(graphqlite_db_t *db);
int64_t graphqlite_create_node_with_labels(graphqlite_db_t *db, const char **labels, size_t label_count);

// Node deletion
int graphqlite_delete_node(graphqlite_db_t *db, int64_t node_id);

// Node existence check
bool graphqlite_node_exists(graphqlite_db_t *db, int64_t node_id);

// Label management
int graphqlite_add_node_label(graphqlite_db_t *db, int64_t node_id, const char *label);
int graphqlite_remove_node_label(graphqlite_db_t *db, int64_t node_id, const char *label);
bool graphqlite_node_has_label(graphqlite_db_t *db, int64_t node_id, const char *label);
char** graphqlite_get_node_labels(graphqlite_db_t *db, int64_t node_id, size_t *count);

// Property operations
int graphqlite_set_node_property(graphqlite_db_t *db, int64_t node_id, 
                                 const char *key, property_value_t *value);
int graphqlite_get_node_property(graphqlite_db_t *db, int64_t node_id,
                                 const char *key, property_value_t *value);
int graphqlite_delete_node_property(graphqlite_db_t *db, int64_t node_id, const char *key);
char** graphqlite_get_node_property_keys(graphqlite_db_t *db, int64_t node_id, size_t *count);

// Batch operations
int graphqlite_create_nodes_batch(graphqlite_db_t *db, int64_t *node_ids, size_t count);
```

**Core Implementation:**

```c
int64_t graphqlite_create_node(graphqlite_db_t *db) {
    if (!db || !db->sqlite_db) return -1;
    
    // Use prepared statement for performance
    sqlite3_stmt *stmt = db->prepared_statements[STMT_CREATE_NODE];
    if (!stmt) return -1;
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    if (rc == SQLITE_DONE) {
        return sqlite3_last_insert_rowid(db->sqlite_db);
    }
    
    return -1;
}

int graphqlite_delete_node(graphqlite_db_t *db, int64_t node_id) {
    if (!db || !db->sqlite_db || node_id <= 0) return SQLITE_ERROR;
    
    // Check for dependent edges first
    sqlite3_stmt *check_stmt = db->prepared_statements[STMT_CHECK_NODE_EDGES];
    sqlite3_bind_int64(check_stmt, 1, node_id);
    sqlite3_bind_int64(check_stmt, 2, node_id);
    
    if (sqlite3_step(check_stmt) == SQLITE_ROW) {
        int edge_count = sqlite3_column_int(check_stmt, 0);
        sqlite3_reset(check_stmt);
        
        if (edge_count > 0) {
            // Delete dependent edges first
            sqlite3_stmt *delete_edges = db->prepared_statements[STMT_DELETE_NODE_EDGES];
            sqlite3_bind_int64(delete_edges, 1, node_id);
            sqlite3_bind_int64(delete_edges, 2, node_id);
            sqlite3_step(delete_edges);
            sqlite3_reset(delete_edges);
        }
    }
    sqlite3_reset(check_stmt);
    
    // Delete node (CASCADE will handle properties and labels)
    sqlite3_stmt *stmt = db->prepared_statements[STMT_DELETE_NODE];
    sqlite3_bind_int64(stmt, 1, node_id);
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

int graphqlite_add_node_label(graphqlite_db_t *db, int64_t node_id, const char *label) {
    if (!db || !db->sqlite_db || node_id <= 0 || !label) return SQLITE_ERROR;
    
    // Use INSERT OR IGNORE to handle duplicates gracefully
    sqlite3_stmt *stmt = db->prepared_statements[STMT_ADD_NODE_LABEL];
    sqlite3_bind_int64(stmt, 1, node_id);
    sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}

int graphqlite_set_node_property(graphqlite_db_t *db, int64_t node_id,
                                 const char *key, property_value_t *value) {
    if (!db || !db->sqlite_db || node_id <= 0 || !key || !value) return SQLITE_ERROR;
    
    // Intern property key
    int key_id = intern_property_key(db->key_cache, key);
    if (key_id < 0) return SQLITE_ERROR;
    
    // Select appropriate prepared statement based on type
    sqlite3_stmt *stmt;
    switch (value->type) {
        case PROP_INT:
            stmt = db->prepared_statements[STMT_SET_NODE_PROP_INT];
            sqlite3_bind_int64(stmt, 1, node_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_int64(stmt, 3, value->value.int_val);
            break;
        case PROP_TEXT:
            stmt = db->prepared_statements[STMT_SET_NODE_PROP_TEXT];
            sqlite3_bind_int64(stmt, 1, node_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_text(stmt, 3, value->value.text_val, -1, SQLITE_STATIC);
            break;
        case PROP_REAL:
            stmt = db->prepared_statements[STMT_SET_NODE_PROP_REAL];
            sqlite3_bind_int64(stmt, 1, node_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_double(stmt, 3, value->value.real_val);
            break;
        case PROP_BOOL:
            stmt = db->prepared_statements[STMT_SET_NODE_PROP_BOOL];
            sqlite3_bind_int64(stmt, 1, node_id);
            sqlite3_bind_int(stmt, 2, key_id);
            sqlite3_bind_int(stmt, 3, value->value.bool_val ? 1 : 0);
            break;
        default:
            return SQLITE_ERROR;
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    
    return (rc == SQLITE_DONE) ? SQLITE_OK : SQLITE_ERROR;
}
```

**Prepared Statements:**
```sql
-- Node operations
INSERT INTO nodes DEFAULT VALUES;
DELETE FROM nodes WHERE id = ?;
SELECT COUNT(*) FROM edges WHERE source_id = ? OR target_id = ?;
DELETE FROM edges WHERE source_id = ? OR target_id = ?;

-- Label operations  
INSERT OR IGNORE INTO node_labels (node_id, label) VALUES (?, ?);
DELETE FROM node_labels WHERE node_id = ? AND label = ?;
SELECT 1 FROM node_labels WHERE node_id = ? AND label = ? LIMIT 1;

-- Property operations (per type)
INSERT OR REPLACE INTO node_props_int (node_id, key_id, value) VALUES (?, ?, ?);
INSERT OR REPLACE INTO node_props_text (node_id, key_id, value) VALUES (?, ?, ?);
INSERT OR REPLACE INTO node_props_real (node_id, key_id, value) VALUES (?, ?, ?);
INSERT OR REPLACE INTO node_props_bool (node_id, key_id, value) VALUES (?, ?, ?);
```

**Performance Optimizations:**
- Use prepared statements for all operations
- Batch operations use transactions automatically
- Property operations leverage key interning cache
- Bulk node creation uses multi-row INSERT statements
- Proper indexing ensures fast lookups and deletions

## Status Updates

*To be added during implementation*