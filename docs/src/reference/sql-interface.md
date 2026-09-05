# SQL Interface Reference

GraphQLite is a standard SQLite extension. Once loaded, it registers SQL scalar functions and creates the graph schema in the current database.

---

## Loading the Extension

**SQLite shell**

```sql
.load ./libgraphqlite
SELECT graphqlite_test();
```

**Python (manual)**

```python
import sqlite3
conn = sqlite3.connect(":memory:")
conn.enable_load_extension(True)
conn.load_extension("./libgraphqlite")
```

**Python (via graphqlite)**

```python
import graphqlite
conn = graphqlite.connect(":memory:")
```

**Rust**

```rust
let conn = graphqlite::Connection::open_in_memory()?;
```

**Entry point symbol**: `sqlite3_graphqlite_init`

On initialization the extension:
1. Creates all schema tables (if not already present).
2. Creates all indexes.
3. Registers the SQL functions listed below.

---

## Registered SQL Functions

### `cypher(query [, params_json])`

```sql
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age');
SELECT cypher('MATCH (n:Person) WHERE n.age > $min RETURN n.name', '{"min": 25}');
```

**Arguments**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| `query` | TEXT | Yes | Cypher query string |
| `params_json` | TEXT (JSON) | No | JSON object; keys map to `$name` placeholders |

**Returns**: TEXT — a JSON array of objects. Each object represents one result row. Keys are the column names from the `RETURN` clause. A query with no results returns `[]`.

**Result format**

```json
[
  {"n.name": "Alice", "n.age": 30},
  {"n.name": "Bob",   "n.age": 25}
]
```

For a single-column result the key is the expression text or alias from `RETURN`. For write queries with no `RETURN` clause, the result is a JSON **object** of modification counts:

```json
{"nodes_created": 2, "relationships_created": 1, "nodes_deleted": 0, "relationships_deleted": 0, "properties_set": 0}
```

The first byte distinguishes the two shapes: `[` is a result set, `{` is a statistics object (or an error object, which additionally carries an `error` key). The empty array `[]` is only returned when a `RETURN` clause produced zero matching rows.

**Error handling**: Sets SQLite error text and returns an error result on parse failure or execution failure.

---

### `cypher_validate(query)`

```sql
SELECT cypher_validate('MATCH (n:Person) RETURN n.name');
```

Validates a Cypher query without executing it.

**Returns**: TEXT — a JSON object:

```json
{"valid": true}
```

or

```json
{"valid": false, "error": "...", "line": 1, "column": 15}
```

---

### `regexp(pattern, string)`

```sql
SELECT regexp('^Al.*', 'Alice');   -- 1
SELECT regexp('^Al.*', 'Bob');     -- 0
```

POSIX extended regular expression (ERE) match. Used internally to implement the `=~` operator. Returns `1` if `string` matches `pattern`, `0` otherwise. The `(?i)` prefix enables case-insensitive matching.

**Arguments**

| Argument | Type | Description |
|----------|------|-------------|
| `pattern` | TEXT | POSIX extended regular expression (ERE) |
| `string` | TEXT | String to test |

**Returns**: INTEGER (`1` or `0`)

---

### `gql_load_graph()`

```sql
SELECT gql_load_graph();
```

Load the graph adjacency structure into an in-memory cache for algorithm execution. Must be called before running graph algorithm functions.

**Returns**: TEXT — JSON status object: `{"status": "loaded", "nodes": N, "edges": M}`. If the graph is already loaded, returns `{"status": "already_loaded", "nodes": N, "edges": M}` instead.

---

### `gql_unload_graph()`

```sql
SELECT gql_unload_graph();
```

Release the in-memory adjacency cache.

**Returns**: TEXT — JSON status object: `{"status": "unloaded"}`

---

### `gql_reload_graph()`

```sql
SELECT gql_reload_graph();
```

Unload and reload the cache. Use after bulk data changes to refresh the algorithm cache.

**Returns**: TEXT — JSON status object: `{"status": "reloaded", "nodes": N, "edges": M}`

---

### `gql_graph_loaded()`

```sql
SELECT gql_graph_loaded();
```

Check whether the adjacency cache is currently loaded.

**Returns**: TEXT — JSON object: `{"loaded": true, "nodes": N, "edges": M}` if loaded, `{"loaded": false, "nodes": 0, "edges": 0}` if not.

---

### `graphqlite_test()`

```sql
SELECT graphqlite_test();
```

Smoke-test function. Returns a success string if the extension is loaded and functioning.

**Returns**: TEXT — `"GraphQLite extension loaded successfully!"`

---

## Query Patterns

**Read and iterate rows in Python**

```python
import json, sqlite3, graphqlite

conn = graphqlite.connect("graph.db")
raw = conn.execute("SELECT cypher('MATCH (n:Person) RETURN n.name, n.age')").fetchone()[0]
rows = json.loads(raw)
for row in rows:
    print(row["n.name"], row["n.age"])
```

**Parameterized query via SQL**

```sql
SELECT cypher(
  'MATCH (n:Person) WHERE n.age > $min RETURN n.name',
  json_object('min', 25)
);
```

**Write query**

```sql
SELECT cypher('CREATE (:Person {name: ''Alice'', age: 30})');
```

String literals inside Cypher must use single quotes. To embed a literal single quote in a SQL string, double it: `''`.

---

## Transaction Behavior

- The `cypher()` function participates in the current SQLite transaction.
- Write operations (`CREATE`, `MERGE`, `SET`, `DELETE`, etc.) are not auto-committed; wrap in `BEGIN`/`COMMIT` for explicit control.
- `gql_load_graph()` reads a snapshot at call time; subsequent writes are not reflected until `gql_reload_graph()` is called.

**Example**

```sql
BEGIN;
SELECT cypher('CREATE (:Person {name: ''Alice''})');
SELECT cypher('CREATE (:Person {name: ''Bob''})');
COMMIT;
```

---

## Direct Schema Access

The graph schema tables are ordinary SQLite tables. You can query them directly for inspection or integration.

```sql
-- Count nodes by label
SELECT label, count(*) FROM node_labels GROUP BY label;

-- List all property keys
SELECT key FROM property_keys ORDER BY key;

-- Find all text properties for node 1
SELECT pk.key, np.value
FROM node_props_text np
JOIN property_keys pk ON pk.id = np.key_id
WHERE np.node_id = 1;
```

Direct writes to schema tables bypass Cypher validation and the property key cache. Prefer `cypher()` for mutations.
