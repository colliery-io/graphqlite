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

**Error handling**: on failure the function raises an SQLite error whose text is a JSON object:

```json
{"error": "Line 1, Col 17: syntax error, unexpected RETURN, expecting ')'", "code": "PARSE_ERROR", "line": 1, "column": 17}
```

| Key | Present | Meaning |
|-----|---------|---------|
| `error` | always | Human-readable reason. Grammar errors name the unexpected token and, where known, the expected one |
| `code` | always | `PARSE_ERROR` (scanner or grammar), `VALIDATION_ERROR` (compile-time semantic check such as `RETURN NOT 1` or a UNION column mismatch), `EXECUTION_ERROR` (transform or runtime), `NOT_IMPLEMENTED`, `MEMORY_ERROR`, `INTERNAL_ERROR` |
| `line`, `column` | parse errors | 1-based location of the offending token |

`{"error", "code"}` is the stable subset; `line` and `column` are only added when known. The message is JSON-escaped, so quotes and control characters in it are safe to decode. Use `cypher_validate()` to get the same diagnostics without executing.

---

### `cypher_rows(query [, params_json])` (table-valued)

```sql
SELECT c0, c1 FROM cypher_rows('MATCH (n:Person) RETURN n.name, n.age') LIMIT 100;
SELECT row FROM cypher_rows('MATCH (n:Person) WHERE n.age > $min RETURN n', '{"min": 25}');
```

An eponymous virtual table that exposes the same query as SQL rows. Use it
for large results and for anything you want to page, filter, or join in SQL:
SQLite steps the table one row at a time, so peak memory is one row rather
than the whole JSON string `cypher()` builds, and `LIMIT` stops the scan.

| Column | Type | Description |
|--------|------|-------------|
| `row` | TEXT (JSON) | The row as a JSON object keyed by `RETURN` column name — the same object `cypher()` puts in its array |
| `cols` | TEXT (JSON) | Array of the `RETURN` column names |
| `ncols` | INTEGER | Number of `RETURN` columns |
| `c0` … `c31` | native | Positional values: integers, reals and booleans (0/1) keep their SQLite types, strings are TEXT, nodes/relationships/paths/lists/maps are JSON text |
| `query`, `params` | hidden | The function-call arguments |

A write query without `RETURN` yields one row whose `row` is the statistics
object and whose `c0`…`c4` are `nodes_created`, `relationships_created`,
`nodes_deleted`, `relationships_deleted`, `properties_set`. Errors raise the
same structured `{"error": ..., "code": ...}` message `cypher()` raises.

```sql
-- compose with the rest of SQL
SELECT p.c0 AS name, sum(value) AS total
FROM cypher_rows('MATCH (p:Person) RETURN p.name, p.scores') AS p, json_each(p.c1)
GROUP BY p.c0;
```

Positional columns beyond the 32nd are not exposed; use `row` for wider
projections. The table shares the connection's executor and statement cache
with `cypher()`.

---

### `cypher_validate(query)`

```sql
SELECT cypher_validate('MATCH (n:Person) RETURN n.name');
```

Validates a Cypher query without executing it. The query goes through the
scanner, the grammar and the same compile-time semantic pass `cypher()` runs
before transform; the graph is never read or written, so validating a
`CREATE` creates nothing. Errors that only surface during transform or
execution (an unknown variable, for example) are not detected.

**Returns**: TEXT — a JSON object:

```json
{"valid": true}
```

or, for a syntax error, the same keys the `cypher()` error object carries:

```json
{"valid": false, "error": "Line 1, Col 17: syntax error, unexpected RETURN, expecting ')'", "code": "PARSE_ERROR", "line": 1, "column": 17}
```

or, for a static semantic violation:

```json
{"valid": false, "error": "SyntaxError: InvalidArgumentType: Type mismatch: expected Boolean but was Integer", "code": "VALIDATION_ERROR"}
```

`line` and `column` are 1-based and present only when the parser can locate
the problem (scanner and grammar errors). Scanner-stage errors such as an
unterminated string or an out-of-range integer literal point at the start of
the offending token.

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

---

### `cypher_capabilities()`

```sql
SELECT cypher_capabilities();
SELECT json_extract(cypher_capabilities(), '$.features.existential_subquery_full');
```

Returns what this build of the extension supports, so a client can detect
features at run time instead of comparing version strings (GitHub #17).
The function takes no arguments and never touches the graph.

**Returns**: TEXT — a JSON object:

```json
{"schema_version": 1,
 "graphqlite_version": "0.9.2",
 "cypher_dialect": "openCypher 9",
 "sqlite": {"version": "3.47.2", "json1": true},
 "neo4j_compat": false,
 "features": {"bracket_property_access": true, "list_literals": true,
              "existential_subquery_full": true, "load_csv": false, "...": true}}
```

| Key | Meaning |
|---|---|
| `schema_version` | Changes only when a key of this object is added, renamed or removed. New entries inside `features` do not change it. |
| `graphqlite_version` | The extension version (`GRAPHQLITE_VERSION`). |
| `cypher_dialect` | The Cypher dialect the grammar follows. |
| `sqlite.version`, `sqlite.json1` | The SQLite library the extension is running in and whether its JSON functions are available. |
| `neo4j_compat` | Reserved for a Neo4j compatibility mode; always `false` today. |
| `features` | One boolean per feature flag. A flag that is missing must be read as `false`. |

The bindings expose the same document as `Connection::capabilities()` (Rust,
a `Capabilities` struct) and `Connection.capabilities()` (Python, a
`Capabilities` dataclass).

