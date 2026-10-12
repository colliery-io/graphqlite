---
title: Storage Model
slug: 0.9.4/explanation/storage-model
---

GraphQLite stores property graphs in SQLite using an Entity-Attribute-Value (EAV) schema. This document explains why that design was chosen, how the tables are structured, and what the trade-offs look like in practice.

## Why EAV?

A property graph has two requirements that are in tension with standard relational design:

1. **Schema flexibility.** Different nodes can have completely different properties. A `Person` node might have `name`, `age`, and `email`. A `Document` node might have `title`, `content`, and `created_at`. You cannot know the full set of property names at schema creation time.

2. **Type heterogeneity.** A property named `score` might be an integer on one node and a float on another. Cypher does not enforce types on property keys.

Three storage strategies are common:

| Strategy | Approach | Problem |
|---|---|---|
| Fixed schema | One table per node type | Requires schema migration for every new property; hard to query across types |
| JSON blob | Single `properties TEXT` column | No index support on property values; comparisons require full-table scans |
| EAV | Separate row per property | Flexible schema, indexable values, but more joins per query |

GraphQLite uses EAV. This trades query complexity (more JOIN operations per Cypher query) for schema flexibility and efficient indexed lookups on property values.

## Table Structure

### Core Tables

**`nodes`** is intentionally minimal:

```sql
CREATE TABLE nodes (
  id INTEGER PRIMARY KEY AUTOINCREMENT
);
```

A node is just an identity. All semantic content lives in the label and property tables. This allows the core table to stay compact and allows the autoincrement sequence to serve as a reliable surrogate key.

**`edges`** carries connectivity and type:

```sql
CREATE TABLE edges (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  source_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  target_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  type TEXT NOT NULL
);
```

The relationship type (`KNOWS`, `FOLLOWS`, `WORKS_AT`, etc.) is stored inline because it is always present and is the primary filter when traversing the graph. The `ON DELETE CASCADE` constraints mean deleting a node automatically removes all its incident edges without requiring explicit cleanup in Cypher.

**`node_labels`** is a many-to-many table between nodes and labels:

```sql
CREATE TABLE node_labels (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  label TEXT NOT NULL,
  PRIMARY KEY (node_id, label)
);
```

A node can carry multiple labels (e.g., `Person` and `Employee`). The composite primary key prevents duplicate labels and serves as the natural index for label lookups.

**`property_keys`** is a normalisation table:

```sql
CREATE TABLE property_keys (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  key TEXT UNIQUE NOT NULL
);
```

Rather than storing the property key string (e.g., `"name"`) directly in every property row, GraphQLite stores an integer `key_id` and looks up the string once. This reduces storage for graphs with many nodes sharing the same property names, and enables the property key cache described below.

### Property Tables

There are ten property tables in total: five for nodes and five for edges. They follow the same pattern:

```sql
-- Node properties, integer values
CREATE TABLE node_props_int (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   INTEGER NOT NULL,
  PRIMARY KEY (node_id, key_id)
);

-- Node properties, text values
CREATE TABLE node_props_text (
  node_id INTEGER NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  key_id  INTEGER NOT NULL REFERENCES property_keys(id),
  value   TEXT NOT NULL,
  PRIMARY KEY (node_id, key_id)
);

-- node_props_real, node_props_bool, node_props_json follow the same shape
-- edge_props_int, edge_props_text, edge_props_real, edge_props_bool, edge_props_json likewise
```

Booleans are stored as `INTEGER CHECK (value IN (0, 1))` because SQLite has no native boolean type. JSON values are stored as `TEXT CHECK (json_valid(value))` — the constraint ensures the stored bytes are parseable JSON.

## Why Separate Tables per Type?

The type-per-table design might seem verbose. Why not a single `node_props` table with a `type` discriminator column?

**Efficient indexes.** SQLite's B-tree indexes work best when a column contains values of a single type. An index on `node_props_int(key_id, value, node_id)` allows the query planner to use a range scan when evaluating `a.age > 30`. If integers and strings were mixed in one column, comparisons would degrade to text comparisons, silently changing semantics.

**No type coercion surprises.** SQLite's flexible type affinity means that storing `42` as text and later comparing it to the integer `42` would require careful `CAST`. Keeping types in separate tables makes the column's affinity unambiguous.

**COALESCE fan-out.** The transformer generates a `COALESCE(...)` that tries each type table in sequence and returns the first non-null result. This works correctly because a given `(node_id, key_id)` pair can exist in at most one type table — a property cannot simultaneously be an integer and a string.

## Property Type Inference

When Cypher writes a property, the type is inferred from the value:

| Value | Table |
|---|---|
| Integer literal (`42`) | `node_props_int` |
| Float literal (`3.14`) | `node_props_real` |
| `true` / `false` | `node_props_bool` |
| JSON object or array | `node_props_json` |
| Everything else | `node_props_text` |

Python's `bool` type is checked before `int` (since `bool` is a subclass of `int` in Python), so `True` goes to `_bool` rather than `_int`.

## Property Key Cache

Looking up a key's integer ID in `property_keys` is necessary for every property read or write. Without caching, a simple `MATCH (n) RETURN n.name, n.age` would issue two `SELECT id FROM property_keys WHERE key = ?` queries per result row, which becomes expensive when returning thousands of rows.

The schema manager maintains a hash table of 1024 slots using the djb2 algorithm:

```c
static unsigned long hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}
```

Each slot holds a `property_key_entry` with the string and its integer ID. On a cache hit, no SQL is issued. On a miss, the key is looked up (or inserted) and the result is stored in the cache.

1024 slots is enough to cover most graphs without collision chains. A graph with 50 distinct property keys will have a load factor under 5%, meaning nearly every lookup resolves in O(1) without a collision. The cache is per-executor, which means per-connection — multiple connections to the same database file each maintain their own independent cache.

## How Property Access Translates to SQL

Consider `RETURN a.age` where `a` is a node. The full translation chain:

1. **Parser** produces an `AST_NODE_PROPERTY` node with `expr = identifier("a")` and `property_name = "age"`.
2. **`transform_property_access()`** checks whether the context is a comparison (`WHERE a.age > 30`) or a projection (`RETURN a.age`). This matters because comparisons need the native type, while projections cast everything to text for uniform JSON serialisation.
3. The transformer emits a correlated `SELECT COALESCE(...)` subquery that queries all five type tables:

```sql
(SELECT COALESCE(
  (SELECT npt.value
   FROM node_props_text npt
   JOIN property_keys pk ON npt.key_id = pk.id
   WHERE npt.node_id = a.id AND pk.key = 'age'),
  (SELECT CAST(npi.value AS TEXT)
   FROM node_props_int npi
   JOIN property_keys pk ON npi.key_id = pk.id
   WHERE npi.node_id = a.id AND pk.key = 'age'),
  (SELECT CAST(npr.value AS TEXT)
   FROM node_props_real npr
   JOIN property_keys pk ON npr.key_id = pk.id
   WHERE npr.node_id = a.id AND pk.key = 'age'),
  (SELECT CASE WHEN npb.value THEN 'true' ELSE 'false' END
   FROM node_props_bool npb
   JOIN property_keys pk ON npb.key_id = pk.id
   WHERE npb.node_id = a.id AND pk.key = 'age'),
  (SELECT npj.value
   FROM node_props_json npj
   JOIN property_keys pk ON npj.key_id = pk.id
   WHERE npj.node_id = a.id AND pk.key = 'age')
))
```

4. **SQLite executes** this subquery. Because the composite indexes on each property table include `(key_id, value, node_id)`, and `property_keys` has an index on `key`, the join between `property_keys` and the property table resolves via index lookup. SQLite evaluates the COALESCE branches lazily — once a branch returns a non-null value, the rest are skipped.

The result is that each property access is effectively two index lookups (one for the key ID, one for the value) in the common case.

## JSON and Nested Property Storage

Properties whose values are JSON objects or arrays are stored in `node_props_json` (or `edge_props_json`). The `CHECK (json_valid(value))` constraint on those tables ensures that only valid JSON is stored.

Nested access — `n.metadata.city` — is handled at transform time. When `transform_property_access()` sees that the base of a property access is itself a property access (i.e., the AST has `AST_NODE_PROPERTY` nested inside another `AST_NODE_PROPERTY`), it generates a `json_extract()` call:

```sql
json_extract(
  (SELECT COALESCE(...) WHERE pk.key = 'metadata'),
  '$.city'
)
```

This means `metadata` is fetched from `node_props_json` as a JSON text value, and `json_extract` then navigates into it. Deeper nesting (`n.a.b.c`) produces nested `json_extract` calls.

String-keyed subscripts like `n['metadata']` are normalised at transform time to behave identically to `n.metadata`. The `AST_NODE_SUBSCRIPT` case in `transform_expression()` checks whether the key is a string literal and, if so, converts it to a property access before generating SQL.

## Index Strategy

GraphQLite creates the following indexes at schema initialisation time:

| Index | Columns | Purpose |
|---|---|---|
| `idx_edges_source` | `edges(source_id, type)` | Outgoing traversal with type filter |
| `idx_edges_target` | `edges(target_id, type)` | Incoming traversal with type filter |
| `idx_edges_type` | `edges(type)` | Full-graph type scans |
| `idx_node_labels_label` | `node_labels(label, node_id)` | Label-to-node lookup |
| `idx_property_keys_key` | `property_keys(key)` | Key name to ID lookup |
| `idx_node_props_int_key_value` | `node_props_int(key_id, value, node_id)` | Covered index for int property filters |
| `idx_node_props_text_key_value` | `node_props_text(key_id, value, node_id)` | Covered index for text property filters |
| `idx_node_props_real_key_value` | `node_props_real(key_id, value, node_id)` | Covered index for real property filters |
| `idx_node_props_bool_key_value` | `node_props_bool(key_id, value, node_id)` | Covered index for bool property filters |
| `idx_node_props_json_key_value` | `node_props_json(key_id, node_id)` | JSON key scans (value omitted; not comparable) |
| *(same pattern for edge_props\_\*)* | | |

The property indexes use a **covering index** pattern: `(key_id, value, node_id)`. When SQLite evaluates `WHERE a.age = 42`, it can satisfy the entire lookup from the index without touching the table heap — `key_id` filters to the right property, `value` satisfies the predicate, and `node_id` is the output needed to join back to the nodes table.

The JSON index omits the value because JSON blobs are not comparable as a unit; individual JSON paths are accessed via `json_extract()` at query time.

## Trade-offs

**Read performance.** A simple `MATCH (n:Person) RETURN n.name` requires a label scan via `idx_node_labels_label` plus one correlated subquery per returned property per row. For small graphs (under 100K nodes), this is fast. As the result set grows, the correlated subqueries become the bottleneck. The optimizer cannot always lift them into a join, though covering indexes mitigate this significantly.

**Write overhead.** Creating a single node with three properties requires:

* 1 insert into `nodes`
* 1 insert into `node_labels`
* Up to 3 inserts into `property_keys` (or cache hits)
* 3 inserts into the appropriate `node_props_*` tables

That is 7 or more inserts per node. For bulk loading, the Python and Rust bindings provide `insert_nodes_bulk()` and `insert_edges_bulk()` methods that bypass the Cypher parser and use direct SQL within a single `BEGIN IMMEDIATE` transaction. This is 100–500x faster than issuing `CREATE` queries through `cypher()`.

**Schema flexibility.** Adding new properties to existing nodes requires no migration. A `Person` node created yesterday with `name` and `age` can have `email` added tomorrow with no schema change. This is a significant advantage for evolving data models.

**Query complexity.** The generated SQL for even simple Cypher queries is verbose. This makes the `EXPLAIN` prefix particularly valuable: `EXPLAIN MATCH (a)-[:KNOWS]->(b) RETURN b.name` returns the generated SQL so you can understand exactly what SQLite will execute.
