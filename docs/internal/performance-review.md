# Performance review: speed and memory

Reviewed at commit `00a8661` (v0.7.0). All numbers below were measured on a
4-core Linux container with the release build (`make extension RELEASE=1`),
an in-memory database, and the harness in `tests/performance/python/`.
Graphs have 4 typed properties per node (`id` text, `name` text, `age` int,
`score` real), one label, and 5 edges per node with one real property.
Absolute times will differ on other hardware; the ratios and scaling
behaviour are the point.

## Summary

The engine has no single hot loop to optimize. Its cost is structural: the
shape of the SQL it generates against the EAV schema, the number of
statements it compiles per operation, and the number of times a result is
copied on its way out. Four findings are algorithmic (10x to 30,000x at
realistic sizes) and are local fixes. The rest are constant-factor
improvements of 2x to 8x that need small design changes in the transform
and result layers.

| # | Finding | Measured | Fix size |
|---|---------|----------|----------|
| 1 | `{prop: $param}` in MATCH compiles to a full table scan | 30 ms vs 0.01 ms at 20K nodes; Python `get_node` 116 ms at 50K | small |
| 2 | Variable-length paths enumerate every path in the graph | 4.5 s vs 0.13 ms at 20K nodes; 18 s at 50K | small |
| 3 | Louvain is O(n²) per iteration | 1.8 s at 10K, 44.6 s at 50K | small |
| 4 | Node similarity re-sorts adjacency per pair | 1.7 s at 5K nodes | small |
| 5 | `RETURN n` costs 35 µs and ~300 mallocs per node | 757 ms at 20K; SQL alone 182 ms; better SQL 104 ms | medium |
| 6 | Property access resolves key names at runtime | 70 ms vs 41 ms per 18K rows | medium |
| 7 | WHERE comparisons never use the value indexes | 24 ms vs 1.6 ms range filter at 20K | medium |
| 8 | Every query re-compiles a 1-3 KB SQL statement | ~80 µs fixed cost per query, 91% in `sqlite3_prepare_v2` | medium |
| 9 | CREATE issues 26 prepare/finalize pairs per 4-property node | 109 µs vs 14 µs raw SQL | small |
| 10 | Result path copies each value 6 to 8 times | 44 MB peak for a 6.6 MB result | large |

## Method

- Latency: `tests/performance/python/harness.py` builds the graph, runs one
  query shape 3 times, reports the median, and reads `VmHWM` before and
  after so peak memory is attributable to that query. Each query runs in
  its own process.
- SQL shape: `EXPLAIN <cypher>` returns the generated SQL, which was then
  fed to SQLite's `EXPLAIN QUERY PLAN` and timed on its own.
- Profiles: callgrind on a `-O2 -g` build, with SQLite API call counts
  extracted from the call graph.
- Heap: massif on `MATCH (n) RETURN n` over 20K nodes.

## Read path

### 1. Parameterized property match is a full scan

`MATCH (n {id: 'n42'}) RETURN n` compiles to a join that drives from the
covering index on `node_props_text(key_id, value, node_id)`. The same
pattern with `$id` compiles to
`FROM nodes WHERE (EXISTS(... text ...) OR EXISTS(... int ...) OR EXISTS(... real ...) OR EXISTS(... bool ...))`,
which SQLite plans as `SCAN nodes` with four correlated subqueries per
row. The branch is at `src/backend/transform/transform_match.c:286`
(nodes) and `:520` (edges). The specialised MATCH+MERGE and MATCH+CREATE
executors do not have this problem because they resolve the pattern
through `find_node_by_pattern`, which is why `upsert_edge` stays fast.

| Query | 1K nodes | 10K | 50K |
|-------|----------|-----|-----|
| literal id lookup | 0.13 ms | 0.13 ms | 0.13 ms |
| `$id` lookup | 1.8 ms | 16 ms | 111 ms |
| `MATCH (n {id:$id}) SET n.age=$v RETURN n` | | 40 ms | 218 ms |

Every convenience method in the Python and Rust bindings uses `$id`
(`has_node`, `get_node`, `get_neighbors`, `node_degree`, `delete_node`,
and `upsert_node` for existing nodes, which issues one MATCH+SET per
property). Measured through the Python binding on a 50K-node graph:

| Binding call | ms/op |
|--------------|-------|
| `get_node` | 116 |
| `upsert_node` new, 3 props | 109 |
| `upsert_node` existing, 3 props | 782 |
| `get_neighbors` | 114 |

The README quick start therefore degrades linearly with graph size.

Design: the transform layer should treat a parameter exactly like a
literal whose type is known at bind time. Two options:

- Pass `params_json` into `cypher_transform_context` so the transform can
  resolve the parameter's runtime type and emit the same typed join it
  emits for literals. Cheapest change, but the generated SQL then depends
  on parameter values, which conflicts with finding 8.
- Emit a type-agnostic semi-join that SQLite can drive from the value
  indexes: `n.id IN (SELECT node_id FROM node_props_text WHERE key_id = ? AND value = :id UNION ALL SELECT node_id FROM node_props_int ... UNION ALL ...)`.
  Measured at 0.01 ms against 30 ms. This keeps SQL independent of
  values and is the form I would take.

Either way, the bindings' `upsert_node` should send one `SET n += $props`
(or one `SET n.a = $a, n.b = $b`) instead of one round trip per property.

### 2. Variable-length paths are not anchored

`generate_varlen_cte` (`src/backend/transform/cypher_transform.c:1174`)
emits a recursive CTE whose anchor is
`SELECT ... FROM edges e WHERE e.type = 'KNOWS'`, that is, every edge of
that type in the graph. The CTE then expands every path up to `max_hops`
from every edge, and only the outer query filters `start_id` to the
matched node. The `visited NOT LIKE '%,id,%'` cycle check is a string scan
per step on top of that.

| `MATCH (a {id:'n42'})-[:KNOWS*1..3]->(b) RETURN count(DISTINCT b)` | time |
|---|---|
| 10K nodes / 50K edges | 2.6 s |
| 20K / 100K | 4.5 s |
| 50K / 250K | 18 s |
| same CTE anchored at the start node, 20K | 0.13 ms |

Design: when either endpoint of the varlen relationship is bound (by an
inline property map, a parameter, or a variable from an earlier clause),
push that predicate into the anchor `SELECT`. For an unbounded start,
keep the current form but add the depth bound to the anchor as well. For
the cycle check, an integer-set representation (a `json_each` over an
array, or a bitmap for small `max_hops`) avoids the `LIKE` scan; that is a
second-order gain compared with anchoring.

### 5. `RETURN n` materialisation

The per-node JSON object is built by SQL that, for every node, scans the
whole `property_keys` table and probes ten indexes per key (five `EXISTS`
plus five value subqueries), see `src/backend/transform/transform_return.c:313`
and the same template repeated at `executor_result_project.c:455` and six
other sites. Cost is therefore O(nodes × global property keys), so it grows
as the schema accumulates keys, not as nodes gain properties.

The executor then undoes the work: `agtype_value_from_vertex_json`
(`src/backend/executor/agtype.c:270`) parses the JSON back into an agtype
tree using three `sqlite3_prepare_v2` calls and a `json_each` per row,
and `extension.c` serialises the tree twice (once at line 183 to size the
buffer, once to emit).

Profile of `MATCH (n) RETURN n` over 2000 nodes: 3 prepares, 16 steps,
and 297 `malloc` calls per row. `agtype_value_from_vertex_json` alone is
25% of all instructions executed by the process, more than the SQL scan.

| `MATCH (n) RETURN n`, 20K nodes | time |
|---|---|
| through `cypher()` | 757 ms |
| the generated SQL, run directly | 182 ms |
| alternative SQL, `UNION ALL` over the five typed tables keyed by `node_id` | 104 ms |
| `MATCH (n:Person) RETURN n.id, n.name, n.age` through `cypher()` | 85 ms |

Design, in order of payoff:

1. Pass entity JSON through verbatim. The SQL already emits the final
   object shape; `extension.c` already has a verbatim path for values
   beginning with `{` or `[`. The agtype tree adds nothing for vertex and
   edge columns and should be reserved for the legacy id-only path.
2. Generate the property object from the five typed tables by `node_id`
   (one range scan on each `(node_id, key_id)` primary key) joined to
   `property_keys`, instead of scanning `property_keys` and probing per
   key. This is the alternative SQL above. Put the template in one
   function; it is currently copied at eight sites.
3. Size the output buffer once from `strlen` of the pass-through values
   and serialise once.

Expected end-to-end: roughly 757 ms to about 120 ms at 20K, and the memory
amplification in finding 10 drops with it.

### 6. Property access resolves key names at runtime

`n.age` compiles to a five-way `COALESCE` where each branch joins
`property_keys` by name. The executor already keeps a key-name cache
(`cypher_schema_get_property_key_id`), so the transform could emit
`key_id = 3` and drop the join. Measured on 18K rows and three properties:
70 ms for the generated SQL against 41 ms with resolved key ids. This is
also a prerequisite for finding 7. The transform layer currently has no
handle to the schema manager; the context needs one, or the executor
should pre-resolve the keys referenced by the AST and pass a map in.

### 7. Comparisons bypass the value indexes

`WHERE n.age > 85` compiles to `_gql_order_cmp(<5-way COALESCE>, 85, '>')`
(`src/backend/transform/transform_expr_ops.c:493`). Wrapping the operand in
a UDF means the `(key_id, value, node_id)` indexes on the typed tables can
never be used, so every predicate is a scan plus two to five subqueries per
row. A range filter over 20K nodes: 24 ms through `cypher()`, 1.6 ms when
written as `FROM node_props_int p JOIN nodes ... WHERE p.key_id = ? AND p.value > 85`.

Design: for `<var>.<prop> <op> <literal-or-param>` where the key id is
resolvable, emit an index-driven `EXISTS` (or a semi-join) against the
typed table matching the literal's type, and keep `_gql_order_cmp` as the
fallback for mixed-type or computed operands. The typed split of the EAV
schema makes this safe: an int literal can only match rows in
`node_props_int` (and `_real`), and null semantics are preserved because a
missing row is simply absent.

### 8. Per-query fixed cost is SQL compilation

A literal point lookup costs 85 µs through `cypher()` and 5 µs as a cached
raw SQL statement. Callgrind attributes 91% of the extension's per-query
instructions to `sqlite3_prepare_v2` on the generated statement (about
570K instructions to compile 1.8 KB of SQL with ten subqueries); parsing
the Cypher is 1.4%, and the transform is small. This caps point queries
at roughly 12K/s per connection regardless of data size.

Design: a per-connection LRU cache from Cypher text to (generated SQL,
prepared statement, column metadata), with `sqlite3_reset` and rebind on
hit. Parameterized queries produce identical SQL, so hit rates in OLTP
usage are high. This only works cleanly if the SQL does not depend on
parameter values (see the choice in finding 1) and if the cache is
finalised from `connection_cache_destroy`. Shrinking the SQL (findings 5
to 7) reduces compile cost on misses.

Two minor per-query costs seen in the profile: `cypher_transform_create_context`
calls `graphqlite_register_helper_udfs` on every query
(`src/backend/transform/cypher_transform.c:38`), which returns
`SQLITE_BUSY` on the first function inside an active statement, and the
schema manager runs `cypher_schema_initialize` twice per connection (once
from `sqlite3_graphqlite_init`, once from `cypher_executor_create`),
including a `PRAGMA user_version` write.

## Write path

### 9. CREATE compiles 26 statements per node

`bench_write.py` on a 10K-node graph, inside one transaction:

| Operation | µs/op |
|-----------|-------|
| `CREATE (n:Person {4 props})` literal | 109 |
| same with `$params` | 114 |
| `MERGE (n:Person {id: ...}) ON CREATE SET` | 95 |
| `MATCH (n {id:'x'}) SET n.age = 31` | 59 |
| `UNWIND $rows CREATE`, 1000 rows | 8.1 per row |
| raw SQL inserts (what `insert_nodes_bulk` does) | 14 |

Callgrind counts 26.5 `sqlite3_prepare_v2` calls and one `sqlite3_exec`
per CREATE. The sources:

- `cypher_schema_set_node_property` (`src/backend/executor/cypher_schema.c:731`)
  deletes the key from all five typed tables before every insert, even for
  a node created in the same statement. Five prepares per property.
- `cypher_schema_create_node` (`:668`) uses `sqlite3_exec`, so SQLite
  re-parses `INSERT INTO nodes DEFAULT VALUES` each time.
- The comment at `:209` explains that persistent prepared statements were
  removed because they blocked `sqlite3_close`. The schema manager is owned
  by the connection cache whose destructor already runs at close; keeping
  statements there and finalising them in `connection_cache_destroy`
  restores caching without the close problem (`sqlite3_close_v2` also
  tolerates it).

Design: a "known new entity" flag on the create path that skips the
cleanup deletes, plus prepared statements held by the schema manager.
That brings the per-node cost close to the raw path. `UNWIND` batching is
already 13x cheaper per row than per-statement CREATE and should be what
the bindings use for multi-row writes.

## Graph algorithms

CSR loading and the linear algorithms scale as expected in memory. The
45 s PageRank at 1M nodes recorded in `tests/performance/RESULTS.md` did
not reproduce here: uncached PageRank is 1.0 s at 250K, 2.6 s at 500K,
and 7.8 s at 1M nodes with 5M edges, of which CSR loading is 79% to 81%.
That result was most likely page-cache pressure on an on-disk database,
not an algorithmic cliff.

| Nodes | CSR load | PageRank cached | WCC cached | RSS after load | JSON output |
|-------|----------|-----------------|------------|----------------|-------------|
| 250K | 0.79 s | 0.23 s | 0.06 s | +36 MB | 15.5 MB |
| 500K | 2.03 s | 0.47 s | 0.13 s | +89 MB | 31 MB |
| 1M | 6.31 s | 1.67 s | 0.34 s | +67 MB | 63 MB |

### 3. Louvain is quadratic

`src/backend/executor/graph_algo_louvain.c:164` mallocs an `n`-sized
array and `:171` zeroes an `n`-sized array for every node in every
iteration, so each local-move pass is O(n²) regardless of edge count:
1.84 s at 10K nodes, 44.6 s at 50K, and hours at 1M. The fix is to
allocate once and clear only the communities touched by the node's
neighbourhood (track them in `neighbor_comms`, which already exists).
Expected: tens of milliseconds at 50K.

### 4. Node similarity re-sorts per pair

`jaccard_similarity` (`graph_algo_similarity.c:75`) calls
`get_neighbors_sorted` for both nodes on every pair, each doing a malloc
and an insertion sort. With the 5000-node guard the all-pairs mode still
takes 1.7 s at 5K nodes and 25K edges. Sorting each adjacency list once
up front (O(E log d)) removes the per-pair allocation entirely; the
hard cap could then move up an order of magnitude. `knn` has the same
pattern.

### Smaller items

- `csr_graph_load` scans `edges` twice and stores `user_ids` as one
  `strdup` per node; at 1M nodes that is 1M small allocations. A single
  arena for the id strings and one pass with a temporary edge array would
  halve load time and allocation count.
- `find_node_by_user_id` is a linear `strcmp` scan; every `dijkstra`,
  `bfs`, `astar`, `knn` call pays O(n) per endpoint. A hash on `user_ids`
  built at load time fixes it.
- Betweenness (`O(VE)`, 1.5 s at 5K/25K) and APSP (`n²` doubles, capped at
  5000 nodes) are inherently expensive; their costs are as designed.
- The cached CSR graph is never invalidated by writes on the same
  connection; algorithms silently run on stale data after a `CREATE`.
  This is a correctness concern rather than a performance one, but any
  statement cache from finding 8 should share the same invalidation hook.

## Memory

### 10. Result amplification

A result leaves the engine through this chain: SQLite materialises the
row text, `build_query_results` (`src/backend/executor/executor_match.c:424`)
`strdup`s every cell into `result->data`, builds an agtype tree per cell
(about ten allocations per property for entities), `extension.c`
serialises the tree once to size the buffer and once to fill it, hands the
buffer to `sqlite3_result_text` with `SQLITE_TRANSIENT` (another copy),
Python creates a `str`, and the binding calls `json.loads`.

| Query, 50K nodes | output | peak heap delta |
|---|---|---|
| `MATCH (n) RETURN n` | 6.6 MB | +44 MB |
| `MATCH (a)-[r:KNOWS]->(b) RETURN a.id, r.weight, b.id` (250K rows) | 11 MB | +83 MB |

Massif on the 20K-node `RETURN n` shows a 51 MB peak: 23 MB inside SQLite
(the correlated `json_object` subqueries and materialised rows), 6.7 MB of
`result->data` row arrays, 3 MB of `strdup`, and the remainder spread over
agtype nodes.

Findings 5 and 6 remove the agtype tree and the double serialisation,
which is most of the C-side amplification. The remaining copies are
inherent to `cypher()` being a scalar function that must return one
string. The interface change that fixes this properly is a table-valued
function (an eponymous virtual table such as
`SELECT * FROM cypher_rows('MATCH ...')`) that yields one SQLite row per
result row with native column types. That removes JSON encoding and
decoding on both sides, lets callers page with `LIMIT`, and keeps peak
memory at one row. The scalar `cypher()` can remain as a wrapper for
compatibility.

### Other memory observations

- `result->data`, `data_types` and `agtype_data` are each an array of
  per-row `malloc`s; for wide results a single flat allocation per array
  would cut allocation count by 3× rows.
- `static char` buffers used as return values in the transform layer
  (`transform_match.c:68`, `:981` (`anon_buf[64][32]`), `:1393`
  (`pool[16][80]`), `transform_with.c:535`, `query_dispatch.c:551`) are
  shared across connections. Two threads running `cypher()` on separate
  connections can corrupt each other's generated SQL. They are also hard
  limits (64 anonymous nodes, 16 pooled aliases) that fail silently.
- Fixed `char sql[2048]`/`[4096]`/`[8192]` stack buffers assembled with
  `snprintf` (`executor_result_project.c:455`, `executor_set.c:83`,
  `executor_call_subquery.c:545`) truncate silently for long identifiers
  or many properties; `dynamic_buffer` already exists and should replace
  them.
- The `property_key_cache` is direct-mapped with 1024 slots and evicts on
  collision, so key lookups regress to a `SELECT` for schemas with many
  keys hashing to the same slot. Chaining or open addressing is a small
  change.

## Correctness issue found while measuring

The non-agtype JSON formatter in `extension.c` escapes only `"` and `\`.
`RETURN 'a\nb' AS s` and `RETURN toUpper(n.s)` on a value containing a
newline or tab produce invalid JSON, and `EXPLAIN` output is unparseable
for the same reason. The agtype path escapes control characters correctly;
the text path needs the same treatment.

## Suggested order of work

1. Findings 1, 2, 3, 4: local fixes with large, easily tested payoffs.
   Add regression tests that assert `EXPLAIN QUERY PLAN` contains no
   `SCAN nodes` for a parameterized id match and that the varlen CTE
   anchor references the bound start node.
2. Findings 5 and 6: entity JSON pass-through and key-id resolution in
   the transform. Consolidate the eight copies of the node/edge JSON
   template first.
3. Findings 7, 8, 9: index-driven comparisons, a prepared-statement cache,
   and prepared statements in the schema manager.
4. Finding 10: design the table-valued result interface. This is the one
   item that changes the public surface and deserves its own design
   document before implementation.

Re-run `tests/performance/python/sweep.sh` after each phase; the
`RESULTS.md` bash suite depends on the `sqlite3` CLI and does not cover
parameterized queries, varlen paths, Louvain, or memory.
