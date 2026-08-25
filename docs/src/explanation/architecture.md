# Architecture Overview

GraphQLite adds a Cypher query language interface to SQLite by functioning as a transpiler: it parses Cypher, translates it to SQL, and executes the resulting SQL against a set of tables that represent a property graph. Understanding this pipeline helps you reason about query behaviour, error messages, and performance.

## Why a Transpiler, Not a Custom Engine

The most important architectural decision in GraphQLite is what it chose not to build: a dedicated graph storage engine or query runtime.

Building a purpose-built graph engine would require implementing disk layout, buffer management, query optimisation, transaction handling, concurrency control, and crash recovery. SQLite already provides all of this, and provides it correctly across a wide range of platforms, with a 35-year track record of reliability.

The transpiler approach means:

- **Durability and atomicity come for free.** Every write goes through SQLite's WAL and journalling machinery.
- **Concurrent access inherits SQLite's guarantees.** Multiple processes (not just threads) can safely read and write the same database file at the same time; SQLite's locking serialises the writes with no corruption or lost updates. The default rollback-journal mode allows one writer at a time with readers blocked during writes; enabling [WAL mode](https://www.sqlite.org/wal.html) (`PRAGMA journal_mode=WAL`) lets readers proceed concurrently with a writer and typically improves write throughput. GraphQLite adds no locking of its own — whatever concurrency SQLite supports in your configuration is what you get.
- **Standard tooling works.** The underlying tables are plain SQLite tables. You can inspect them with the SQLite CLI, use SQLite backup APIs, and attach the database to other tools.
- **Query execution is handled by a proven optimiser.** The generated SQL benefits from SQLite's query planner, covering indexes, and prepared statement caching.

The cost of this approach is translation overhead on every query, and the impedance mismatch between graph patterns and relational joins. Both are manageable: the translation is fast (typically under 1ms for simple queries), and the join structure is deterministic once you understand the EAV schema.

## The Query Pipeline

A Cypher query passes through four stages before results are returned:

```
Cypher string
      │
      ▼
┌─────────────────────────────────┐
│  1. PARSER                      │
│  cypher_gram.y + cypher_scanner.l│
│  ──────────────────────────────  │
│  Cypher string → AST nodes      │
└──────────────┬──────────────────┘
               │ ast_node tree
               ▼
┌─────────────────────────────────┐
│  2. TRANSFORMER                 │
│  transform_match.c, etc.        │
│  ──────────────────────────────  │
│  AST → SQL string               │
└──────────────┬──────────────────┘
               │ SQL string
               ▼
┌─────────────────────────────────┐
│  3. EXECUTOR                    │
│  cypher_executor.c              │
│  ──────────────────────────────  │
│  sqlite3_prepare + step         │
└──────────────┬──────────────────┘
               │ raw SQLite rows
               ▼
┌─────────────────────────────────┐
│  4. RESULT FORMATTER            │
│  executor_result.c, agtype.c    │
│  ──────────────────────────────  │
│  rows → JSON text returned      │
│  by the cypher() SQL function   │
└─────────────────────────────────┘
```

### Stage 1: The Parser

The parser is a Bison GLR grammar (`cypher_gram.y`) with a Flex scanner (`cypher_scanner.l`). It produces a typed AST: `ast_node` structs that include `cypher_query`, `cypher_match`, `cypher_create`, `cypher_return`, `cypher_node_pattern`, `cypher_rel_pattern`, and expression types like `cypher_binary_op`, `cypher_property`, `cypher_identifier`, and `cypher_literal_*`.

**Why GLR?** Cypher has syntactic ambiguities that a standard LALR(1) parser cannot resolve. The most visible example is that `(n)` is simultaneously valid as a parenthesised expression and as a node pattern. GLR allows the parser to pursue both interpretations in parallel and resolve the ambiguity once more context is available. The grammar currently declares `%expect 4` shift/reduce conflicts and `%expect-rr 3` reduce/reduce conflicts — these are known, documented, and intentional.

Identifiers can be regular alphanumeric names or backtick-quoted names (`BQIDENT`), which the scanner strips to their bare text. The `END_P` keyword is also permitted as an identifier through the grammar's `identifier` rule, allowing queries like `MATCH (n) RETURN n.end`.

Error recovery is handled at this stage. When parsing fails, `parse_cypher_query_ext()` returns a `cypher_parse_result` with a populated `error_message` containing position information, which propagates back to the `cypher()` SQL function as a SQLite error.

### Stage 2: The Transformer

The transformer walks the AST and emits SQL strings. It is not a general-purpose SQL generator: it knows the exact schema of GraphQLite's EAV tables and generates SQL specifically against those tables.

Key files and responsibilities:

| File | Responsibility |
|---|---|
| `cypher_transform.c` | Entry point; creates transform context |
| `transform_match.c` | MATCH patterns → SQL FROM/JOIN/WHERE |
| `transform_return.c` | RETURN items → SQL SELECT list |
| `transform_expr_ops.c` | Expression operators and property access |
| `transform_create.c` | CREATE → INSERT INTO nodes/edges/properties |
| `transform_set.c` | SET → UPDATE on property tables |
| `transform_delete.c` | DELETE → DELETE FROM |
| `transform_variables.c` | Variable-to-alias tracking across clauses |
| `sql_builder.c` | Dynamic string buffer for SQL construction |
| `transform_func_*.c` | Function dispatch (string, math, path, etc.) |

The transform context (`cypher_transform_context`) carries the SQL buffer being built, a variable context (`var_ctx`) that maps Cypher variable names to SQL table aliases, and flags like `in_comparison` that alter how property access is generated.

**Concrete translation example.** Consider:

```cypher
MATCH (a:Person)-[:KNOWS]->(b)
WHERE a.name = 'Alice'
RETURN b.name
```

The transformer produces SQL roughly equivalent to:

```sql
SELECT
  (SELECT COALESCE(
    (SELECT npt.value FROM node_props_text npt
     JOIN property_keys pk ON npt.key_id = pk.id
     WHERE npt.node_id = n2.id AND pk.key = 'name'),
    (SELECT CAST(npi.value AS TEXT) FROM node_props_int npi
     JOIN property_keys pk ON npi.key_id = pk.id
     WHERE npi.node_id = n2.id AND pk.key = 'name'),
    ...
  )) AS "b.name"
FROM nodes n1
JOIN node_labels nl1 ON nl1.node_id = n1.id AND nl1.label = 'Person'
JOIN edges e1 ON e1.source_id = n1.id AND e1.type = 'KNOWS'
JOIN nodes n2 ON n2.id = e1.target_id
WHERE (SELECT COALESCE(
    (SELECT npt.value FROM node_props_text npt
     JOIN property_keys pk ON npt.key_id = pk.id
     WHERE npt.node_id = n1.id AND pk.key = 'name'),
    ...
  )) = 'Alice'
```

Each property access becomes a correlated subquery that fans out across all five typed property tables (`node_props_text`, `node_props_int`, `node_props_real`, `node_props_bool`, `node_props_json`) using `COALESCE` to return whichever type holds the value. In comparison contexts (WHERE clauses) the types are preserved natively; in RETURN contexts everything is cast to text.

**Nested property access.** For expressions like `n.metadata.city` — where `metadata` is stored as a JSON blob — the transformer generates `json_extract(n_metadata_subquery, '$.city')`, recursively building the extraction path.

### Stage 3: The Executor

The executor orchestrates the full pipeline and manages the SQLite connection state. Its entry point is `cypher_executor_execute()`, which:

1. Calls `parse_cypher_query_ext()` to get the AST.
2. Calls `cypher_executor_execute_ast()` to dispatch on AST type.
3. For `AST_NODE_QUERY` and `AST_NODE_SINGLE_QUERY`, delegates to `dispatch_query_pattern()`.
4. `dispatch_query_pattern()` analyses the clause combination present in the query (MATCH, RETURN, CREATE, SET, DELETE, etc.) and selects the best-matching handler from the pattern registry.
5. The selected handler calls the appropriate transformer functions to produce SQL, then calls `sqlite3_prepare_v2()` and `sqlite3_step()` to execute it.
6. UNION queries bypass the pattern dispatcher and go directly through the transform layer, which handles them as a special `AST_NODE_UNION` case.

**EXPLAIN mode.** If the query starts with `EXPLAIN`, the executor runs the transformer but does not execute the SQL. Instead it returns a text result containing the matched pattern name, the clause flags, and the generated SQL string. This is useful for debugging unexpected behaviour.

### Stage 4: Result Formatting

Raw SQLite column values are formatted into JSON by `executor_result.c` and `agtype.c`. The `cypher()` SQL function always returns a JSON array of row objects:

```json
[{"b.name": "Bob"}, {"b.name": "Carol"}]
```

For rich graph objects (nodes and relationships returned as entities rather than scalar properties), the AGE-compatible `agtype` system serialises them with type annotations. Modification queries without a RETURN clause return a plain-text statistics string.

## Extension Architecture

GraphQLite loads into SQLite as a shared library extension. The entry point `sqlite3_graphqlite_init()` registers several SQL functions on the current database connection.

### Per-Connection Caching

The most important structural detail is the `connection_cache`:

```c
typedef struct {
    sqlite3 *db;
    cypher_executor *executor;
    csr_graph *cached_graph;
} connection_cache;
```

This struct is allocated once per database connection and registered via `sqlite3_create_function`'s user-data pointer. It holds:

- A `cypher_executor` instance, which in turn holds the schema manager and the property key cache. Because executors are expensive to create (schema initialisation, prepared statement allocation), they are created on the first call to `cypher()` and reused for all subsequent calls on the same connection.
- A `csr_graph` pointer for the in-memory graph needed by algorithm functions. This is `NULL` until the user explicitly calls `gql_load_graph()`.

When the database connection closes, SQLite calls the destructor registered with the function, which frees both the executor and any cached graph.

### Registered SQL Functions

The extension registers:

| Function | Purpose |
|---|---|
| `cypher(query)` | Execute a Cypher query, return JSON |
| `cypher(query, params_json)` | Execute with parameters |
| `graphqlite_test()` | Health check |
| `gql_load_graph()` | Build CSR from current tables, cache it |
| `gql_unload_graph()` | Free cached CSR graph |
| `gql_reload_graph()` | Invalidate and rebuild CSR cache |

Schema initialisation (`CREATE TABLE IF NOT EXISTS ...`) happens inside `cypher_executor_create()`, which is called on the first `cypher()` invocation. Extension loading takes approximately 5ms to complete this step.

## Language Bindings

Both the Python and Rust bindings wrap the `cypher()` SQL function rather than linking directly against GraphQLite's C API.

**Python.** `Connection._load_extension()` calls `sqlite3.Connection.load_extension()` with the path to `graphqlite.dylib` (or `.so`/`.dll`). After loading, every `connection.cypher(query, params)` call issues `SELECT cypher(?, ?)` against the underlying `sqlite3.Connection`. The JSON result is parsed and returned as a `CypherResult` object (a list of dicts). This means Python adds one round-trip through `sqlite3_exec` but no C-level coupling beyond the SQLite extension API.

**Rust.** The Rust binding uses `rusqlite` and similarly loads the extension via `Connection::load_extension()`. Cypher queries are executed as `SELECT cypher(?)` statements. Higher-level helpers in `src/` (graph operations, algorithm wrappers) build Cypher strings and parse the JSON results. The `Graph` struct maintains an open `rusqlite::Connection` with the extension already loaded.

## Graph Algorithm Integration

Graph algorithms (PageRank, Betweenness Centrality, Dijkstra, Louvain, etc.) operate on the CSR graph cache rather than on the EAV tables directly. The integration path is:

1. User calls `SELECT gql_load_graph()`. This reads all rows from `nodes` and `edges`, builds a CSR (Compressed Sparse Row) representation in heap memory, and stores it in `connection_cache.cached_graph`.
2. Each subsequent `cypher()` call syncs the `cached_graph` pointer into the current executor: `executor->cached_graph = cache->cached_graph`.
3. When the query dispatcher processes a RETURN-only query and finds a function name like `pageRank()` or `dijkstra()`, it dispatches to the graph algorithm subsystem instead of the normal SQL path.
4. The algorithm reads the CSR structure, runs its computation in C, and returns results as a JSON array, which is formatted and returned by the normal result formatter.

The CSR provides O(1) access to a node's neighbours (via the `row_ptr` and `col_idx` arrays), which is critical for iterative algorithms that traverse the graph many times. The EAV tables do not have this property — following an edge via SQL requires at minimum a B-tree lookup on `edges.source_id`.

If `gql_load_graph()` has not been called, algorithm functions will fail with an error indicating the graph is not loaded. After bulk inserts or other modifications, the cache must be explicitly refreshed with `gql_reload_graph()`.
