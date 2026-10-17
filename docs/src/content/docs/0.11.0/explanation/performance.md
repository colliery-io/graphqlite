---
title: Performance Characteristics
slug: 0.11.0/explanation/performance
---

GraphQLite's performance profile is shaped by three factors: the overhead of the Cypher-to-SQL translation pipeline, the cost of the EAV schema's join-heavy queries, and the behaviour of the in-memory CSR cache for graph algorithms. Understanding these factors lets you make informed decisions about data loading, query structure, and when to reach for the bulk APIs.

## Benchmark Reference Numbers

These figures come from a single-core MacBook workload with an in-memory SQLite database (`:memory:`). Disk-backed databases will be faster with WAL mode enabled and slower when the OS page cache is cold.

| Operation | Typical latency |
|---|---|
| Extension loading (schema init) | ~5ms (once per connection) |
| Simple `CREATE (:Person {name: 'Alice'})` | 0.5–1ms |
| Simple `MATCH (n:Person) RETURN n.name` (10 nodes) | 0.5–2ms |
| `MATCH (a)-[:KNOWS]->(b) RETURN b.name` (100 relationships) | 1–5ms |
| Bulk insert via Python `insert_nodes_bulk()` | 100–500x faster than Cypher CREATE |
| `gql_load_graph()` on 100K nodes/edges | ~50–100ms |
| PageRank on 100K nodes | ~180ms |
| PageRank on 1M nodes | ~38s |
| Property key cache lookup (hit) | O(1), no SQL |
| Property key cache lookup (miss) | 1 SQL roundtrip to `property_keys` |

The 5ms extension loading cost is a one-time expense per connection. After the first `cypher()` call, the executor is cached and reused for all subsequent calls on the same connection.

## The CSR Graph Cache

Graph algorithms (PageRank, Dijkstra, Betweenness Centrality, Louvain, etc.) cannot run efficiently against the EAV tables. Finding a node's neighbours requires a B-tree lookup on `edges.source_id`, and iterative algorithms like PageRank traverse the full graph hundreds of times. At 1M nodes that would be hundreds of millions of B-tree lookups.

The CSR (Compressed Sparse Row) representation solves this. After `SELECT gql_load_graph()`, GraphQLite:

1. Reads all rows from `nodes` to build the node ID array.
2. Builds a hash table mapping node IDs to CSR array indices.
3. Reads all rows from `edges` twice: first to count out-edges per node (to compute row pointer offsets), then to fill the column index arrays.
4. Builds a parallel in-edges structure for algorithms that need reverse traversal.

The result is two arrays (`row_ptr` and `col_idx`) where `row_ptr[i]` is the offset in `col_idx` where node `i`'s neighbours begin, and `row_ptr[i+1] - row_ptr[i]` is its degree. Neighbour access is O(1): `col_idx[row_ptr[i] .. row_ptr[i+1]]`.

### When to Load

The cache must be loaded before running any algorithm, and must be refreshed after structural changes (adding or deleting nodes or edges). The cache persists for the lifetime of the connection; a stale cache causes algorithms to operate on the graph state at the time of the last load, silently ignoring newer data.

For cache management instructions — when and how to call `gql_load_graph()`, `gql_reload_graph()`, and `gql_unload_graph()` — see [Using Graph Algorithms](../../how-to/graph-algorithms/).

### Memory Implications

The CSR graph holds two integer arrays of length `edge_count` (for `col_idx` and `in_col_idx`) and one array of length `node_count + 1` (for `row_ptr` and `in_row_ptr`). For a graph with N nodes and E edges:

* `row_ptr` arrays: 2 × (N+1) × 4 bytes ≈ 8N bytes
* `col_idx` arrays: 2 × E × 4 bytes ≈ 8E bytes
* Node ID array: N × 4 bytes ≈ 4N bytes
* Hash table: ~4 × N × 4 bytes ≈ 16N bytes (open addressing, load factor ~25%)

A graph with 1M nodes and 5M edges uses approximately 60MB of heap memory for the CSR structure. The user-defined ID strings (if present) add additional allocation per node.

If memory is constrained, `gql_unload_graph()` can be called after algorithm runs to free the CSR heap allocation. The trade-off is that the next algorithm call will require a full reload from the database tables. On a 1M-node graph, `gql_load_graph()` takes approximately 50–100ms, so unloading between algorithm calls is only worthwhile when memory pressure is severe.

## Property Key Cache

Every property read or write needs the integer `key_id` for the property name. The property key cache uses djb2 hashing over 1024 slots, held in the `cypher_schema_manager` (which is per-executor, so per-connection).

A typical graph with 20–50 distinct property keys will have a cache load factor well under 10%, meaning:

* **Cache hit**: Hash the key string, index into the slot array, compare the stored string, return the `key_id`. Zero SQL.
* **Cache miss**: Hash lookup fails; issue `SELECT id FROM property_keys WHERE key = ?` (covered by `idx_property_keys_key`); store the result in the cache for future lookups.

Cache misses only occur for property keys not yet seen in this connection's session. After the first query that touches a given key, all subsequent accesses to that key on the same connection are cache hits.

The cache has no eviction policy — it grows monotonically, but with at most 1024 slots before collisions occur. For graphs with more than a few hundred distinct property key names, you may start seeing hash collisions. Collisions do not cause correctness problems (misses fall through to SQL), but they do degrade performance toward one SQL lookup per property access.

## Bulk Insert

The most important performance optimisation available to users is bypassing the Cypher pipeline entirely for bulk data loading.

Issuing `SELECT cypher('CREATE (:Person {name: "Alice", age: 30})')` for each node in a large graph is slow because each call:

1. Parses the Cypher string (Bison GLR parse).
2. Transforms the AST to SQL insert statements.
3. Executes three or more SQL statements (insert nodes, insert label, insert properties).
4. Formats the result.

The Python `insert_nodes_bulk()` and `insert_edges_bulk()` methods skip steps 1 and 2 entirely and batch all of step 3 inside a single `BEGIN IMMEDIATE` transaction:

```python
id_map = g.insert_nodes_bulk([
    ("alice", {"name": "Alice", "age": 30}, "Person"),
    ("bob",   {"name": "Bob",   "age": 25}, "Person"),
])
g.insert_edges_bulk([
    ("alice", "bob", {"since": 2020}, "KNOWS"),
], id_map)
```

The transaction amortises the cost of page writes across thousands of rows. The `id_map` dictionary eliminates the need for a `SELECT node_id FROM node_props_text WHERE value = ?` lookup per edge source and target.

Benchmarks show 100–500x throughput improvement for bulk loads compared to equivalent Cypher CREATE queries. For a graph with 100K nodes and 500K edges, bulk insert completes in seconds; Cypher CREATE would take minutes.

The Rust binding offers the equivalent `Graph::insert_nodes_bulk()` method with the same semantics.

## Index Utilisation

SQLite's query planner makes decisions based on index statistics. For GraphQLite's EAV schema, the most important index patterns are:

**Label filtering** (`MATCH (n:Person)`): Uses `idx_node_labels_label` which is `(label, node_id)`. The query `WHERE label = 'Person'` is a single B-tree range scan that returns all node IDs with that label. This is the primary entry point for most read queries.

**Property equality** (`WHERE n.age = 30`): The generated SQL contains a correlated subquery that filters `node_props_int` with `key_id = ? AND value = 30`. The covering index `idx_node_props_int_key_value` on `(key_id, value, node_id)` allows this to be satisfied entirely from the index, returning the `node_id` without a table heap read.

**Edge traversal** (`MATCH (a)-[:KNOWS]->(b)`): Uses `idx_edges_source` on `(source_id, type)` for outgoing traversal. The type filter is folded into the index scan. Incoming traversal uses `idx_edges_target` on `(target_id, type)`.

**When indexes are not used**: Range predicates on JSON properties (e.g., `WHERE n.metadata.city = 'London'`) require evaluating `json_extract()` for every row that passes the outer filter. SQLite cannot use a B-tree index to accelerate `json_extract()` comparisons without a generated column or expression index.

## SQLite-Specific Optimisations

**WAL mode.** For disk-backed databases with any level of concurrent access (even one writer, one reader), WAL mode allows readers to proceed while a writer is active. This is the most impactful single setting for mixed read/write workloads of Cypher queries.

**Prepared statements.** The `cypher_executor` caches the executor per connection. Within each query execution, the generated SQL is prepared and executed, but the prepared statement is finalised after each use because the generated SQL changes with each Cypher query. For repeated identical queries, this means the SQL planning cost is paid each time. If you are calling the same parameterised Cypher query many times (e.g., a lookup by ID), issuing the same query string with different parameter values — rather than constructing slightly different Cypher strings — allows SQLite's prepared statement cache to reuse the plan.

**Page cache.** SQLite's default page cache is 2MB (512 pages × 4KB). For graphs with many nodes, the EAV tables span many pages. A larger cache reduces I/O on repeated queries, with the trade-off of higher baseline memory use per connection.

**Synchronous mode.** Reducing the synchronous setting eliminates fsync calls and can roughly double write throughput for bulk loads. The trade-off is reduced durability: a crash during a write can leave the database in an inconsistent state. This setting is appropriate for analytics workloads on expendable data, but should never be used for production data without explicit acceptance of that risk.

For the specific PRAGMA values to use and when to apply each setting, see the [how-to guides](../../how-to/graph-algorithms/).

## Scaling Characteristics

**Under 10K nodes**: Performance is dominated by connection overhead and query parsing. The EAV join pattern is fast because the tables fit in the SQLite page cache. Simple MATCH+RETURN queries complete in under 1ms.

**10K–100K nodes**: Property lookup correlated subqueries become noticeable. Queries that return many rows with many properties per row can take 5–50ms. The covering indexes keep most lookups out of the table heap, but the sheer number of subquery evaluations adds up. Bulk insert becomes worthwhile at this scale.

**100K–1M nodes**: The EAV fan-out is the dominant cost. A full-graph scan (no label filter, no index pushdown) requires visiting every row in the relevant label and property tables. Graph algorithms should always operate via the CSR cache at this scale, not via Cypher queries that generate SQL. PageRank on 100K nodes via CSR takes ~180ms; the equivalent SQL-based traversal would be orders of magnitude slower.

**Above 1M nodes**: Memory usage for the CSR cache becomes significant (60MB+ for 1M nodes, 5M edges). Disk-backed databases benefit strongly from WAL mode and a large page cache. PageRank on 1M nodes takes ~38s on a single core. For workloads at this scale, consider whether batching algorithm results, pre-computing centrality scores and storing them as properties, or partitioning the graph into sub-graphs makes sense for your use case.

## Memory Usage Guidelines

| Component | Approximate size |
|---|---|
| `cypher_executor` struct | ~1KB (plus schema manager) |
| Property key cache (1024 slots) | ~50KB empty, grows with distinct keys |
| CSR graph (N nodes, E edges) | ~(20N + 8E) bytes |
| SQL buffer per query | 1–50KB depending on query complexity |
| Result data | Proportional to row count × column count |

For a graph with 100K nodes and 500K edges, the CSR cache uses approximately 6MB. The property key cache for a graph with 100 distinct property names uses approximately 100KB. The executor and schema manager overhead is negligible.
