# Storage Model

GraphQLite uses a typed property graph model stored in regular SQLite tables. The schema is designed for query efficiency using an Entity-Attribute-Value (EAV) pattern with property key normalization.

## Schema Overview

```
┌─────────────────────────────────────┐
│              nodes                   │
│  id (PK, auto-increment)            │
├─────────────────────────────────────┤
│  1                                  │
│  2                                  │
│  3                                  │
└─────────────────────────────────────┘
           │
           │ 1:N
           ▼
┌─────────────────────────────────────┐
│           node_labels                │
│  node_id (FK) │ label               │
├───────────────┼─────────────────────┤
│  1            │ "Person"            │
│  2            │ "Person"            │
│  3            │ "Company"           │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│           property_keys              │
│  id (PK) │ key (UNIQUE)             │
├──────────┼──────────────────────────┤
│  1       │ "name"                   │
│  2       │ "age"                    │
│  3       │ "id"                     │
└─────────────────────────────────────┘
           │
           │ 1:N (via key_id)
           ▼
┌───────────────────────────────────────────┐
│            node_props_text                 │
│  node_id (FK) │ key_id (FK) │ value       │
├───────────────┼─────────────┼─────────────┤
│  1            │ 3           │ "alice"     │
│  1            │ 1           │ "Alice"     │
│  2            │ 3           │ "bob"       │
│  2            │ 1           │ "Bob"       │
└───────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                         edges                            │
│  id (PK) │ source_id (FK) │ target_id (FK) │ type       │
├──────────┼────────────────┼────────────────┼────────────┤
│  1       │ 1              │ 2              │ "KNOWS"    │
│  2       │ 1              │ 3              │ "WORKS_AT" │
└─────────────────────────────────────────────────────────┘
```

## Core Tables

### nodes

The nodes table stores graph nodes with a simple auto-incrementing ID. Node metadata such as labels and properties are stored in separate tables, enabling nodes to have multiple labels and efficient property queries.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY AUTOINCREMENT | Internal node identifier |

### node_labels

Labels are stored in a separate table allowing nodes to have multiple labels. This normalized design enables efficient label-based filtering through indexed lookups.

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | INTEGER FK → nodes(id) | References the node |
| `label` | TEXT | Label name (e.g., "Person") |

The primary key is the composite `(node_id, label)`, which prevents duplicate labels on the same node.

### edges

The edges table stores relationships between nodes with a required relationship type.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY AUTOINCREMENT | Internal edge identifier |
| `source_id` | INTEGER FK → nodes(id) | Source node |
| `target_id` | INTEGER FK → nodes(id) | Target node |
| `type` | TEXT NOT NULL | Relationship type (e.g., "KNOWS") |

Foreign keys use `ON DELETE CASCADE` so removing a node automatically removes its edges.

### property_keys

Property names are normalized into a lookup table to reduce storage overhead and improve query performance. Instead of storing the property name string with every property value, we store a small integer key ID.

| Column | Type | Description |
|--------|------|-------------|
| `id` | INTEGER PRIMARY KEY AUTOINCREMENT | Property key identifier |
| `key` | TEXT UNIQUE | Property name (e.g., "name", "age") |

## Property Tables

Properties are stored in separate tables by type. This approach enables type-safe queries, efficient indexing by value, and proper numeric comparisons without type conversion overhead.

**Node property tables:**
- `node_props_text` — String values
- `node_props_int` — Integer values
- `node_props_real` — Floating-point values
- `node_props_bool` — Boolean values (stored as 0 or 1)

**Edge property tables:**
- `edge_props_text`
- `edge_props_int`
- `edge_props_real`
- `edge_props_bool`

Each property table has the same structure:

| Column | Type | Description |
|--------|------|-------------|
| `node_id` / `edge_id` | INTEGER FK | References the owner entity |
| `key_id` | INTEGER FK → property_keys(id) | References the property name |
| `value` | (varies by table) | The property value |

The primary key is the composite `(node_id, key_id)` or `(edge_id, key_id)`, ensuring each entity has at most one value per property.

## Indexes

GraphQLite creates indexes optimized for common graph query patterns:

```sql
-- Edge traversal (covers both directions and type filtering)
CREATE INDEX idx_edges_source ON edges(source_id, type);
CREATE INDEX idx_edges_target ON edges(target_id, type);
CREATE INDEX idx_edges_type ON edges(type);

-- Label filtering
CREATE INDEX idx_node_labels_label ON node_labels(label, node_id);

-- Property key lookup
CREATE INDEX idx_property_keys_key ON property_keys(key);

-- Property value queries (enables efficient WHERE clauses)
CREATE INDEX idx_node_props_text_key_value ON node_props_text(key_id, value, node_id);
CREATE INDEX idx_node_props_int_key_value ON node_props_int(key_id, value, node_id);
-- ... similar for other property tables
```

The property indexes are designed "key-first" to efficiently satisfy queries like `WHERE n.name = 'Alice'`, which translate to lookups by key_id and value.

## Why This Design?

**Typed property tables** provide several advantages over storing all properties as JSON or a single TEXT column. Integer comparisons are performed natively rather than through string parsing. Type-specific indexes enable efficient range queries. Storage is more compact since values don't require type metadata.

**Property key normalization** through the `property_keys` table reduces storage by replacing repeated property name strings with integer IDs. This also enables efficient property-first queries and simplifies schema introspection.

**Separate label table** allows nodes to have multiple labels, which is a common requirement in graph modeling. The label index supports efficient label-based filtering without scanning all nodes.

## Query Translation

When you write:

```cypher
MATCH (p:Person {name: 'Alice'})
WHERE p.age > 25
RETURN p.name, p.age
```

GraphQLite translates this to SQL that joins the appropriate tables:

```sql
SELECT
    name_prop.value AS "p.name",
    age_prop.value AS "p.age"
FROM nodes p
JOIN node_labels p_label ON p.id = p_label.node_id AND p_label.label = 'Person'
LEFT JOIN node_props_text name_prop
    ON p.id = name_prop.node_id
    AND name_prop.key_id = (SELECT id FROM property_keys WHERE key = 'name')
LEFT JOIN node_props_int age_prop
    ON p.id = age_prop.node_id
    AND age_prop.key_id = (SELECT id FROM property_keys WHERE key = 'age')
WHERE name_prop.value = 'Alice'
    AND age_prop.value > 25
```

In practice, the query optimizer uses cached prepared statements for property key lookups, making this translation efficient.

## Direct SQL Access

You can query the underlying tables directly for advanced use cases:

```sql
-- Count nodes by label
SELECT label, COUNT(*) FROM node_labels GROUP BY label;

-- Find all properties of a specific node
SELECT pk.key, 'text' as type, pt.value
FROM node_props_text pt
JOIN property_keys pk ON pt.key_id = pk.id
WHERE pt.node_id = 1
UNION ALL
SELECT pk.key, 'int' as type, CAST(pi.value AS TEXT)
FROM node_props_int pi
JOIN property_keys pk ON pi.key_id = pk.id
WHERE pi.node_id = 1;

-- Find nodes with a specific property value
SELECT nl.node_id, nl.label, pt.value as name
FROM node_props_text pt
JOIN property_keys pk ON pt.key_id = pk.id
JOIN node_labels nl ON pt.node_id = nl.node_id
WHERE pk.key = 'name' AND pt.value = 'Alice';
```
