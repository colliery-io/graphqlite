# Storage Model

GraphQLite uses a typed property graph model stored in regular SQLite tables.

## Schema Overview

```
┌────────────────────────────────────────────────────────────┐
│                        nodes                                │
│  id (PK) │ user_id (UNIQUE) │ label                        │
├──────────┼──────────────────┼──────────────────────────────┤
│  1       │ "alice"          │ "Person"                     │
│  2       │ "bob"            │ "Person"                     │
│  3       │ "acme"           │ "Company"                    │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│                        edges                                │
│  id (PK) │ source_id (FK) │ target_id (FK) │ label         │
├──────────┼────────────────┼────────────────┼───────────────┤
│  1       │ 1              │ 2              │ "KNOWS"       │
│  2       │ 1              │ 3              │ "WORKS_AT"    │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│                   node_props_text                           │
│  id (PK) │ node_id (FK) │ key      │ value                 │
├──────────┼──────────────┼──────────┼───────────────────────┤
│  1       │ 1            │ "name"   │ "Alice"               │
│  2       │ 2            │ "name"   │ "Bob"                 │
│  3       │ 3            │ "name"   │ "Acme Corp"           │
└────────────────────────────────────────────────────────────┘
```

## Tables

### nodes

Stores graph nodes.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY | Internal ID |
| `user_id` | TEXT UNIQUE | User-provided ID |
| `label` | TEXT | Node label (e.g., "Person") |

### edges

Stores relationships between nodes.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY | Internal ID |
| `source_id` | INTEGER FK | Source node internal ID |
| `target_id` | INTEGER FK | Target node internal ID |
| `label` | TEXT | Relationship type (e.g., "KNOWS") |

### Property Tables

Properties are stored in separate tables by type for efficient querying and proper type handling.

**Node properties**:
- `node_props_text` - Text/string properties
- `node_props_int` - Integer properties
- `node_props_real` - Floating-point properties
- `node_props_bool` - Boolean properties

**Edge properties**:
- `edge_props_text`
- `edge_props_int`
- `edge_props_real`
- `edge_props_bool`

Each property table has:

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY | Internal ID |
| `node_id`/`edge_id` | INTEGER FK | Owner entity |
| `key` | TEXT | Property name |
| `value` | (varies) | Property value |

## Indexes

GraphQLite creates these indexes for performance:

```sql
-- Node lookups
CREATE INDEX idx_nodes_user_id ON nodes(user_id);
CREATE INDEX idx_nodes_label ON nodes(label);

-- Edge traversal
CREATE INDEX idx_edges_source ON edges(source_id);
CREATE INDEX idx_edges_target ON edges(target_id);
CREATE INDEX idx_edges_label ON edges(label);

-- Property lookups
CREATE INDEX idx_node_props_text_lookup ON node_props_text(node_id, key);
CREATE INDEX idx_node_props_int_lookup ON node_props_int(node_id, key);
-- ... similar for other property tables
```

## Why Typed Properties?

Storing properties in typed tables provides several benefits:

1. **Type safety** - Integer properties are stored as integers
2. **Query efficiency** - Numeric comparisons are fast
3. **Storage efficiency** - No type conversion overhead
4. **Indexing** - Can create type-specific indexes

## Query Translation

When you write:

```cypher
MATCH (p:Person {name: 'Alice'})
WHERE p.age > 25
RETURN p.name, p.age
```

It becomes:

```sql
SELECT
    name_prop.value AS "p.name",
    age_prop.value AS "p.age"
FROM nodes p
LEFT JOIN node_props_text name_prop
    ON p.id = name_prop.node_id AND name_prop.key = 'name'
LEFT JOIN node_props_int age_prop
    ON p.id = age_prop.node_id AND age_prop.key = 'age'
WHERE p.label = 'Person'
    AND name_prop.value = 'Alice'
    AND age_prop.value > 25
```

## Direct SQL Access

You can query the underlying tables directly:

```sql
-- Count nodes by label
SELECT label, COUNT(*) FROM nodes GROUP BY label;

-- Find all properties of a node
SELECT 'text' as type, key, value FROM node_props_text WHERE node_id = 1
UNION ALL
SELECT 'int' as type, key, CAST(value AS TEXT) FROM node_props_int WHERE node_id = 1
UNION ALL
SELECT 'real' as type, key, CAST(value AS TEXT) FROM node_props_real WHERE node_id = 1
UNION ALL
SELECT 'bool' as type, key, CAST(value AS TEXT) FROM node_props_bool WHERE node_id = 1;
```

## Comparison with Document Model

Some graph databases store nodes as JSON documents. GraphQLite's typed property tables provide several advantages:

- Enable proper numeric comparisons
- Allow type-specific indexing
- Avoid JSON parsing overhead during queries
