---
id: schema-creation-and-migration
level: task
title: "Schema creation and migration"
created_at: 2025-07-19T22:59:10.696244+00:00
updated_at: 2025-07-19T22:59:10.696244+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Schema creation and migration

## Parent Initiative

[[foundation-layer]]

## Objective

Implement the database schema creation and migration system that establishes the typed EAV storage architecture validated in Phase Zero. This system must reliably create all required tables, indexes, and constraints while supporting future schema evolution through versioned migrations.

The schema system forms the foundation for all other components and must ensure optimal performance through proper indexing strategies while maintaining data integrity through foreign key constraints.

## Acceptance Criteria

- [ ] **Schema Creation**: Database initialization creates all required tables (nodes, edges, property_keys, node_props_*, edge_props_*, node_labels) with proper constraints
- [ ] **Index Creation**: All critical indexes are created automatically (property-first optimization indexes, edge traversal indexes, label indexes)
- [ ] **Migration System**: Versioned migration system supports schema evolution with rollback capability
- [ ] **Performance Validation**: Schema creation completes in <10ms for new databases
- [ ] **Foreign Key Integrity**: All foreign key constraints are properly defined and enforced
- [ ] **SQLite Extension Integration**: Schema works correctly when loaded as SQLite extension
- [ ] **Error Handling**: Clear error messages for schema creation failures with recovery guidance

## Implementation Notes

**Core Schema Components:**

```sql
-- Schema version tracking
CREATE TABLE schema_version (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Core graph structure
CREATE TABLE nodes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE edges (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    type TEXT NOT NULL,
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Property key interning
CREATE TABLE property_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT UNIQUE NOT NULL
);

-- Typed property storage
CREATE TABLE node_props_int (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value INTEGER NOT NULL,
    PRIMARY KEY (node_id, key_id)
);
-- Similar tables for node_props_text, node_props_real, node_props_bool
-- Similar tables for edge_props_*

-- Node labels
CREATE TABLE node_labels (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    label TEXT NOT NULL,
    PRIMARY KEY (node_id, label)
);
```

**Critical Indexes (Property-First Optimization):**
```sql
-- Enable fast property filtering before graph traversal
CREATE INDEX idx_props_int_key_value ON node_props_int(key_id, value, node_id);
CREATE INDEX idx_props_text_key_value ON node_props_text(key_id, value, node_id);
CREATE INDEX idx_props_real_key_value ON node_props_real(key_id, value, node_id);
CREATE INDEX idx_props_bool_key_value ON node_props_bool(key_id, value, node_id);

-- Edge traversal optimization
CREATE INDEX idx_edges_source ON edges(source_id, type);
CREATE INDEX idx_edges_target ON edges(target_id, type);
CREATE INDEX idx_edges_type ON edges(type);

-- Label-based filtering  
CREATE INDEX idx_node_labels_label ON node_labels(label, node_id);
```

**Migration Framework:**
```c
typedef struct {
    int version;
    const char *description;
    const char *up_sql;
    const char *down_sql;
} schema_migration_t;

int apply_migrations(sqlite3 *db);
int rollback_migration(sqlite3 *db, int target_version);
int get_current_schema_version(sqlite3 *db);
```

**Key Implementation Considerations:**
- Use transactions for all schema operations to ensure atomicity
- Validate schema state before and after migrations
- SQLite extension entry point must call schema initialization
- Consider PRAGMA foreign_keys = ON for referential integrity
- Schema creation should be idempotent (safe to run multiple times)

## Status Updates

*To be added during implementation*