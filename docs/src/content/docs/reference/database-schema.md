---
title: Database Schema Reference
---

GraphQLite uses an Entity-Attribute-Value (EAV) schema stored in plain SQLite tables. All tables are created with `CREATE TABLE IF NOT EXISTS` during extension initialization, so they are safe to call multiple times.

---

## Core Tables

### `nodes`

Stores graph nodes. Each node has an auto-assigned integer primary key.

```sql
CREATE TABLE IF NOT EXISTS nodes (
  id INTEGER PRIMARY KEY AUTOINCREMENT
);
```

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PK | Internal node identifier; auto-incremented |

User-facing node IDs (strings) are stored as text properties and looked up by the higher-level API. The internal `id` is used in all join operations.

---

### `edges`

Stores directed graph edges.

```sql
CREATE TABLE IF NOT EXISTS edges (
  id        INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  type      TEXT NOT NULL
);
```

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PK | Internal edge identifier |
| `source_id` | INTEGER FK → `nodes.id` | Source node; cascades on delete |
| `target_id` | INTEGER FK → `nodes.id` | Target node; cascades on delete |
| `type` | TEXT | Relationship type (e.g. `"KNOWS"`) |

---

### `node_labels`

Maps nodes to their labels. A node may have multiple labels.

```sql
CREATE TABLE IF NOT EXISTS node_labels (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  label   TEXT NOT NULL,
  PRIMARY KEY (node_id, label)
);
```

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | INTEGER FK → `nodes.id` | References `nodes.id`; cascades on delete |
| `label` | TEXT | Label string (e.g. `"Person"`) |

The composite primary key `(node_id, label)` enforces uniqueness.

---

### `property_keys`

Normalized lookup table for property key names. Shared by all node and edge property tables.

```sql
CREATE TABLE IF NOT EXISTS property_keys (
  id  INTEGER PRIMARY KEY AUTOINCREMENT,
  key TEXT UNIQUE NOT NULL
);
```

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PK | Numeric key identifier |
| `key` | TEXT UNIQUE | Property name string (e.g. `"name"`, `"age"`) |

All property value tables reference `property_keys.id` rather than storing key strings directly. An in-memory hash-map cache (`property_key_cache`) avoids repeated lookups during query execution.

---

## Node Property Tables

One table per Cypher type. A property is stored in exactly one table, determined at write time by the value type.

### `node_props_int`

```sql
CREATE TABLE IF NOT EXISTS node_props_int (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   INTEGER NOT NULL,
  PRIMARY KEY (node_id, key_id)
);
```

### `node_props_real`

```sql
CREATE TABLE IF NOT EXISTS node_props_real (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   REAL NOT NULL,
  PRIMARY KEY (node_id, key_id)
);
```

### `node_props_text`

```sql
CREATE TABLE IF NOT EXISTS node_props_text (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   TEXT NOT NULL,
  PRIMARY KEY (node_id, key_id)
);
```

### `node_props_bool`

```sql
CREATE TABLE IF NOT EXISTS node_props_bool (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   INTEGER NOT NULL CHECK (value IN (0, 1)),
  PRIMARY KEY (node_id, key_id)
);
```

Stores `0` (false) or `1` (true). The `CHECK` constraint enforces this.

### `node_props_json`

```sql
CREATE TABLE IF NOT EXISTS node_props_json (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   TEXT NOT NULL CHECK (json_valid(value)),
  PRIMARY KEY (node_id, key_id)
);
```

Stores JSON objects and arrays as text. The `CHECK` constraint enforces valid JSON via SQLite's `json_valid()`.

---

## Edge Property Tables

Identical structure to node property tables, with `edge_id` replacing `node_id`.

### `edge_props_int`

```sql
CREATE TABLE IF NOT EXISTS edge_props_int (
  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   INTEGER NOT NULL,
  PRIMARY KEY (edge_id, key_id)
);
```

### `edge_props_real`

```sql
CREATE TABLE IF NOT EXISTS edge_props_real (
  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   REAL NOT NULL,
  PRIMARY KEY (edge_id, key_id)
);
```

### `edge_props_text`

```sql
CREATE TABLE IF NOT EXISTS edge_props_text (
  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   TEXT NOT NULL,
  PRIMARY KEY (edge_id, key_id)
);
```

### `edge_props_bool`

```sql
CREATE TABLE IF NOT EXISTS edge_props_bool (
  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   INTEGER NOT NULL CHECK (value IN (0, 1)),
  PRIMARY KEY (edge_id, key_id)
);
```

### `edge_props_json`

```sql
CREATE TABLE IF NOT EXISTS edge_props_json (
  edge_id INTEGER NOT NULL REFERENCES edges(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   TEXT NOT NULL CHECK (json_valid(value)),
  PRIMARY KEY (edge_id, key_id)
);
```

---

## Indexes

All indexes use `CREATE INDEX IF NOT EXISTS`.

### Edge traversal indexes

```sql
CREATE INDEX IF NOT EXISTS idx_edges_source ON edges(source_id, type);
CREATE INDEX IF NOT EXISTS idx_edges_target ON edges(target_id, type);
CREATE INDEX IF NOT EXISTS idx_edges_type   ON edges(type);
```

- `idx_edges_source`: supports outgoing edge lookups and type-filtered traversals.
- `idx_edges_target`: supports incoming edge lookups and type-filtered traversals.
- `idx_edges_type`: supports edge type scans (e.g. `MATCH ()-[:TYPE]-()`).

### Label index

```sql
CREATE INDEX IF NOT EXISTS idx_node_labels_label ON node_labels(label, node_id);
```

Supports label-filtered `MATCH` patterns (e.g. `MATCH (n:Person)`).

### Property key index

```sql
CREATE INDEX IF NOT EXISTS idx_property_keys_key ON property_keys(key);
```

Speeds up property key lookups by name when the in-memory cache is cold.

### Node property indexes

```sql
CREATE INDEX IF NOT EXISTS idx_node_props_int_key_value  ON node_props_int(key_id, value, node_id);
CREATE INDEX IF NOT EXISTS idx_node_props_text_key_value ON node_props_text(key_id, value, node_id);
CREATE INDEX IF NOT EXISTS idx_node_props_real_key_value ON node_props_real(key_id, value, node_id);
CREATE INDEX IF NOT EXISTS idx_node_props_bool_key_value ON node_props_bool(key_id, value, node_id);
CREATE INDEX IF NOT EXISTS idx_node_props_json_key_value ON node_props_json(key_id, node_id);
```

Cover index for `WHERE` predicates on node properties. The `(key_id, value, node_id)` order supports equality and range filters without a table scan.

The JSON index omits `value` (JSON columns are not range-indexed) but indexes `(key_id, node_id)` for existence checks.

### Edge property indexes

```sql
CREATE INDEX IF NOT EXISTS idx_edge_props_int_key_value  ON edge_props_int(key_id, value, edge_id);
CREATE INDEX IF NOT EXISTS idx_edge_props_text_key_value ON edge_props_text(key_id, value, edge_id);
CREATE INDEX IF NOT EXISTS idx_edge_props_real_key_value ON edge_props_real(key_id, value, edge_id);
CREATE INDEX IF NOT EXISTS idx_edge_props_bool_key_value ON edge_props_bool(key_id, value, edge_id);
CREATE INDEX IF NOT EXISTS idx_edge_props_json_key_value ON edge_props_json(key_id, edge_id);
```

Same structure as node property indexes.

---

## Property Type Inference Rules

When a Cypher write operation stores a property value, the type is inferred from the value and determines which table receives the row.

| Condition | Table |
|-----------|-------|
| Value is a Cypher integer literal or Python `int` | `*_props_int` |
| Value is a Cypher float literal or Python `float` | `*_props_real` |
| Value is the string `'true'` or `'false'` (case-insensitive), or Python `bool` | `*_props_bool` |
| Value is a JSON object (`{…}`) or JSON array (`[…]`) | `*_props_json` |
| All other values | `*_props_text` |

A property key may appear in only one type table per entity at a time. Updating a property with a different type removes the old row and inserts into the new table.

---

## Property Key Cache

The `property_key_cache` is an in-process hash map (djb2 hash, chained buckets) that caches `property_key.id` lookups by key string. It is created per connection during `cypher_executor_create()` and freed when the connection closes. The cache avoids a `SELECT id FROM property_keys WHERE key = ?` round-trip for each property access during query execution.

---

## Cascade Delete Behavior

All `REFERENCES nodes(id)` and `REFERENCES edges(id)` foreign keys include `ON DELETE CASCADE`. This means:

- Deleting a row from `nodes` automatically removes all rows in `node_labels`, `node_props_*`, and all `edges` that reference it.
- Deleting a row from `edges` automatically removes all rows in `edge_props_*`.

SQLite foreign key enforcement must be enabled: `PRAGMA foreign_keys = ON;` (GraphQLite enables this automatically at connection open).

---

## Summary Table List

| Table | Rows represent |
|-------|---------------|
| `nodes` | Graph nodes |
| `edges` | Graph edges (directed) |
| `node_labels` | Node-to-label assignments |
| `property_keys` | Property name registry |
| `node_props_int` | Integer node properties |
| `node_props_real` | Float node properties |
| `node_props_text` | String node properties |
| `node_props_bool` | Boolean node properties |
| `node_props_json` | JSON object/array node properties |
| `edge_props_int` | Integer edge properties |
| `edge_props_real` | Float edge properties |
| `edge_props_text` | String edge properties |
| `edge_props_bool` | Boolean edge properties |
| `edge_props_json` | JSON object/array edge properties |
