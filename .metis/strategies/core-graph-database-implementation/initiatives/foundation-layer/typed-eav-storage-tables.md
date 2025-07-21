---
id: typed-eav-storage-tables
level: task
title: "Typed EAV storage tables"
created_at: 2025-07-19T23:01:58.654791+00:00
updated_at: 2025-07-19T23:01:58.654791+00:00
parent: foundation-layer
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
---

# Typed EAV storage tables

## Parent Initiative

[[foundation-layer]]

## Objective

Implement the typed Entity-Attribute-Value (EAV) storage tables that provide optimal performance for graph property storage. This system separates properties by type (int, text, real, bool) into dedicated tables, enabling type-specific indexing and eliminating casting overhead.

The typed EAV approach was validated in Phase Zero to provide the best query performance (3.73ms vs 4.00ms for JSON) while maintaining flexibility for arbitrary graph properties. This implementation must support both node and edge properties efficiently.

## Acceptance Criteria

- [ ] **Typed Property Tables**: Separate tables for node_props_int, node_props_text, node_props_real, node_props_bool
- [ ] **Edge Property Support**: Equivalent edge_props_* tables with same structure as node properties
- [ ] **Type-Specific Indexes**: Optimized indexes on (key_id, value, entity_id) for each property type
- [ ] **Query Performance**: Property filtering queries achieve <5ms (maintaining Phase Zero performance)
- [ ] **Storage Efficiency**: Demonstrate space savings vs single EAV table through type-specific storage
- [ ] **Referential Integrity**: Foreign key constraints ensure data consistency
- [ ] **Property Operations**: Support for INSERT, UPDATE, DELETE, SELECT operations on all property types
- [ ] **Bulk Operations**: Efficient batch insert/update capabilities for bulk mode

## Implementation Notes

**Node Property Tables:**

```sql
-- Integer properties
CREATE TABLE node_props_int (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value INTEGER NOT NULL,
    PRIMARY KEY (node_id, key_id)
);

-- Text properties
CREATE TABLE node_props_text (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value TEXT NOT NULL,
    PRIMARY KEY (node_id, key_id)
);

-- Real (floating point) properties
CREATE TABLE node_props_real (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value REAL NOT NULL,
    PRIMARY KEY (node_id, key_id)
);

-- Boolean properties (stored as INTEGER 0/1)
CREATE TABLE node_props_bool (
    node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value INTEGER NOT NULL CHECK (value IN (0, 1)),
    PRIMARY KEY (node_id, key_id)
);
```

**Edge Property Tables:**
```sql
-- Mirror structure for edge properties
CREATE TABLE edge_props_int (
    edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,
    key_id INTEGER NOT NULL REFERENCES property_keys(id),
    value INTEGER NOT NULL,
    PRIMARY KEY (edge_id, key_id)
);
-- Similar for edge_props_text, edge_props_real, edge_props_bool
```

**Critical Indexes for Property-First Optimization:**
```sql
-- Property filtering before entity joins (key performance pattern)
CREATE INDEX idx_node_props_int_key_value ON node_props_int(key_id, value, node_id);
CREATE INDEX idx_node_props_text_key_value ON node_props_text(key_id, value, node_id);
CREATE INDEX idx_node_props_real_key_value ON node_props_real(key_id, value, node_id);
CREATE INDEX idx_node_props_bool_key_value ON node_props_bool(key_id, value, node_id);

-- Entity-centric access patterns
CREATE INDEX idx_node_props_int_node ON node_props_int(node_id, key_id);
CREATE INDEX idx_node_props_text_node ON node_props_text(node_id, key_id);
CREATE INDEX idx_node_props_real_node ON node_props_real(node_id, key_id);
CREATE INDEX idx_node_props_bool_node ON node_props_bool(node_id, key_id);

-- Equivalent indexes for edge properties
CREATE INDEX idx_edge_props_int_key_value ON edge_props_int(key_id, value, edge_id);
-- ... etc
```

**Property Operations API:**
```c
typedef enum {
    PROP_INT,
    PROP_TEXT, 
    PROP_REAL,
    PROP_BOOL
} property_type_t;

typedef struct {
    property_type_t type;
    union {
        int64_t int_val;
        char *text_val;
        double real_val;
        int bool_val;
    } value;
} property_value_t;

// Core property operations
int set_node_property(sqlite3 *db, int64_t node_id, const char *key, property_value_t *value);
int get_node_property(sqlite3 *db, int64_t node_id, const char *key, property_value_t *value);
int delete_node_property(sqlite3 *db, int64_t node_id, const char *key);

// Bulk operations for performance
int set_node_properties_batch(sqlite3 *db, int64_t node_id, 
                             property_batch_t *properties, size_t count);
```

**Property-First Query Pattern:**
```sql
-- OPTIMIZED: Filter by properties FIRST
WITH filtered_nodes AS (
    SELECT node_id FROM node_props_int 
    WHERE key_id = (SELECT id FROM property_keys WHERE key = 'age') 
    AND value > 25
)
SELECT n.id FROM nodes n 
JOIN filtered_nodes f ON n.id = f.node_id;

-- vs INEFFICIENT: Join first, filter later  
SELECT n.id FROM nodes n
JOIN node_props_int p ON n.id = p.node_id
JOIN property_keys k ON p.key_id = k.id
WHERE k.key = 'age' AND p.value > 25;
```

**Performance Optimizations:**
- Use prepared statements for all property operations
- Batch INSERT operations with transactions
- Type-specific value validation before insertion
- Index hints for complex property queries
- Consider UPSERT pattern for property updates

## Status Updates

*To be added during implementation*