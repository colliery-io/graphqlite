# Changelog

All notable changes to GraphQLite are documented here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [0.8.0] — 2026-09-07

A performance release implementing all ten findings of the performance review
(Metis initiative GQLITE-I-0051, PR #119). Query results, TCK pass count and
the `cypher()` output format are unchanged; the release adds one SQL surface,
the `cypher_rows` table-valued function, and its binding wrappers. Headline
numbers (release builds, 10K–20K nodes): parameterised point lookups
7.75 ms → 0.07 ms, anchored variable-length paths 1.3 s → 0.35 ms, Louvain
1.5 s → 0.17 s, `RETURN n` over 20K nodes 409 ms → 89 ms, repeated point
queries 66 µs → 8 µs, `CREATE` 81 µs → 14.5 µs.

### Performance (performance review, Metis GQLITE-I-0051)

- **Parameterized inline property matches use the value indexes** (F1).
  `MATCH (n {id: $id})` compiled to four correlated `EXISTS` subqueries that
  SQLite planned as a full node scan; it now compiles to an `IN` semi-join
  over the typed property tables driven by the `(key_id, value, id)` covering
  indexes. Point lookups through `$param` go from ~8 ms to ~0.1 ms at 10K
  nodes, which is every binding convenience method (`get_node`, `has_node`,
  `upsert_node`, `get_neighbors`, ...). Edge inline parameter filters use the
  same shape as a post-filter (43 ms → 3 ms).
- **Variable-length paths are anchored at the bound start node** (F2). The
  recursive CTE used to seed a walk from every edge of the type and filter
  `start_id` afterwards; when the pattern's start node carries an inline
  property map (literal or parameter, one or more pairs) the base case is now
  restricted to those nodes. `(a {id: 'x'})-[:T*1..3]->(b)` at 10K nodes /
  50K edges: 1.2 s → 0.4 ms.
- **Louvain local-move pass is O(E) instead of O(n²)** (F3): 1.5 s → 0.17 s at
  10K nodes, 44.6 s → 0.84 s at 50K.
- **nodeSimilarity / knn no longer sort adjacency lists per pair** (F4), and
  all-pairs mode with `threshold > 0` enumerates candidates through shared
  neighbors instead of every pair, with a growable result list instead of an
  n²/2 preallocation. The node cap for that mode is raised from 5,000 to
  50,000; `threshold = 0` keeps the 5,000 cap because its output is O(n²).
  Ties in the similarity ordering now break on `(node1, node2)`.

- **One entity-JSON template, built from the typed tables** (F5). `RETURN n`
  used to scan all of `property_keys` per node and probe ten indexes per key,
  then parse the JSON back into an agtype tree and serialise it twice. The
  property object is now a UNION ALL over the five typed tables keyed by the
  entity id, defined once (`src/include/entity_json_sql.h`) instead of at
  fourteen sites, and the executor passes entity JSON through verbatim.
  20K nodes: `RETURN n` 409 ms → 89 ms, `RETURN r` 366 ms → 62 ms. Edge JSON
  now uses `startNode`/`endNode` keys everywhere (previously `startNodeId`
  in some SQL paths, normalised by the agtype round trip).
- **Property key ids are resolved at transform time** (F6). `n.age` filters
  each typed table on `key_id = N` instead of joining `property_keys` by
  name in every branch (falls back to the name join for keys that do not
  exist yet).
- **WHERE comparisons use the value indexes** (F7). A top-level WHERE
  conjunct of the form `n.prop <op> literal` (either order, `=`, `<`, `<=`,
  `>`, `>=`) compiles to `id IN (SELECT ... FROM typed_table WHERE key_id = N
  AND value <op> literal)` driven by the `(key_id, value, id)` indexes,
  instead of a UDF over five correlated subqueries per row. Three-valued
  semantics are preserved: the rewrite applies only where NULL and FALSE
  are equivalent (WHERE conjuncts, including AND chains, OPTIONAL MATCH and
  WITH ... WHERE), never under NOT, OR, CASE or in RETURN. 20K nodes:
  `WHERE n.age > 85` 10.6 ms → 1.7 ms, `WHERE n.name = 'x'` 6.9 ms → 0.08 ms.

- **Per-connection statement cache for read queries** (F8). Parsing,
  transforming and preparing a 1–2 KB statement was ~90% of a point query.
  The executor now keeps up to 64 pure read queries (MATCH … RETURN and the
  generic read pipeline, never writes, CALL or algorithms) keyed by exact
  Cypher text, with their AST, transform state and prepared statement, and
  re-executes them with fresh bindings. Cached statements are finalised from
  SQLite's `SQLITE_TRACE_CLOSE` callback so `sqlite3_close()` still succeeds;
  an application that installs its own `sqlite3_trace_v2` hook afterwards
  replaces that callback and should close with `sqlite3_close_v2()`. Set
  `GQL_STMT_CACHE=0` in the environment to disable. The executor also stops
  re-registering all 51 helper UDFs on every query. 20K nodes:
  `MATCH (n {id: $id}) RETURN n.name` 66 µs → 8 µs on a hit, 51 µs on a miss.

- **Write path keeps its statements prepared** (F9). The schema manager
  compiled ~26 statements per created node (`sqlite3_exec` for the node
  insert, and five cleanup deletes plus an insert per property). It now
  holds lazily prepared statements for node/edge/label/key inserts and the
  cleanup deletes, reset after every use and finalised through the same
  close hook as the statement cache; properties of an entity created by the
  same CREATE clause skip the cleanup deletes entirely. 10K-node graph,
  µs per operation: `CREATE (n:Person {4 props})` 81 → 14.5 (raw SQL 8.8),
  `MERGE` 66 → 29, `SET` 43 → 23, `UNWIND $rows CREATE` 5.6 → 2.4 per row.

- **O(1) endpoint lookup for graph algorithms.** `dijkstra`, `astar`, `bfs`,
  `dfs`, `knn` and the pair form of `nodeSimilarity` resolved user ids with a
  string scan of every node; the CSR graph now carries a hash from user id to
  index built at load.

- **`cypher_rows` table-valued interface** (F10, ADR GQLITE-A-0006).
  `SELECT ... FROM cypher_rows(query [, params])` exposes a Cypher result as
  SQL rows: `row` is the JSON object `cypher()` would emit for that row,
  `cols`/`ncols` describe the projection, and `c0`…`c31` carry the values
  with native SQLite types (integers, reals, booleans, text; entities, lists
  and maps as JSON text). SQLite steps the table one row at a time, so peak
  memory is one row instead of the whole JSON string, `LIMIT` stops the scan,
  and results compose with `WHERE`, joins and `json_each`. Write queries
  without RETURN yield one statistics row; errors carry the same structured
  message as `cypher()`. The table shares the connection's executor and
  statement cache with `cypher()`. Bindings: Python `Connection.iter_rows()`
  / `Graph.iter_query()` generators, Rust `Connection::cypher_rows_each()`.
  50K-node `MATCH (n) RETURN n` consumed from Python: peak RSS growth
  51 MB → 8 MB, first row in 165 ms instead of 198 ms; end-to-end time is
  unchanged because this phase still materialises the result in C before
  streaming it (stepping the statement inside the table is the follow-up).

- **Smaller items from the review.** The property-key cache chains entries
  on hash collision instead of evicting (two hot keys sharing one of the
  1,024 slots used to miss alternately). A CSR graph loaded with
  `gql_load_graph()` is rebuilt automatically before the next algorithm call
  when a write has run since, instead of serving stale topology. Transform
  and executor scratch buffers that were `static` are now thread-local, so
  connections on different threads no longer share them. Fixed-size SQL
  buffers (`char sql[2048…8192]`, silently truncating) in SET-with-function,
  MERGE lookups, CALL subquery evaluation, aggregation JOINs and entity
  refetch are built on the heap; the pattern-comprehension collect buffer
  and the CALL evaluation scratch were stack arrays handed to a growable
  buffer API, which could `realloc` a stack pointer for large projections
  such as `[(a)-->(b) | {x: b, y: b, z: b, w: b}]`. The per-row `strdup`
  copies in `build_query_results` are left alone: at three small
  allocations per row they were not measurable next to the items above.

### Fixed

- **Rust binding re-extracts the bundled extension when its content changes.**
  The extracted copy was reused whenever the file size matched, so a rebuilt
  library of identical size (any development build, or a patched release of
  the same version) kept loading the stale extract. It is now compared
  byte-for-byte.
- **`cypher()` emits valid JSON for strings containing control characters.**
  The text result path escaped only `"` and `\\`; newlines, tabs and other
  control characters (and therefore all `EXPLAIN` output) produced invalid
  JSON. They are now escaped as `\\n`, `\\t`, ... or `\\u00XX`.

### Tooling

- `tests/performance/python/` harness runs on macOS (no `/proc`; falls back
  to `ru_maxrss`) and picks the platform library name by default.

## [0.7.0] — 2026-09-05

A bindings-correctness release closing the GitHub issue batch #104–#116. Core
changes are limited to `nodeSimilarity` argument handling and the shape of the
`cypher()` return value for write queries. TCK pass count is unchanged at
3788 / 3876; 948/948 unit tests; Python 386 / Rust 302 binding tests.

### Breaking

- **`cypher()` returns a JSON object for write queries** (#116). A modification
  query without `RETURN` now yields
  `{"nodes_created":N,"relationships_created":N,"nodes_deleted":N,"relationships_deleted":N,"properties_set":N}`
  instead of the `"Query executed successfully - nodes created: N, ..."` string.
  The first byte distinguishes a statistics object (`{`) from a result set
  (`[`). Both bindings surface it as a single row keyed by those five fields.
  `DETACH DELETE` now reports the cascaded edge count in
  `relationships_deleted` (it was always 0).
- **Python `GraphManager.query(cypher, graphs, params=None)`** (#112): `graphs`
  is required and must be non-empty (`ValueError` otherwise). The documented
  "auto-detected from the query" behaviour never existed. Rust `query` /
  `query_sql` return `Error::InvalidArgument` on an empty slice.
- **Python `Graph(db_path, extension_path=None)`** (#113): the dead `namespace`
  parameter is gone from `Graph` and `graph()`; passing it raises `TypeError`.
  Callers passing `extension_path` positionally must update.
- **Python `Graph.get_node_edges()` returns dicts** (#114) with `source`,
  `target`, `r` keys, matching `get_edges_from` / `get_edges_to`, instead of
  `(source, target, props)` tuples.

### Fixed

- **Algorithm results always empty in the bindings** (#104, #105, #106):
  `astar`, `node_similarity`, `knn` (Python + Rust) and `bfs`, `dfs`, `apsp`
  (Rust) never unwrapped the core's `column_0` wrapper. The unwrap now lives in
  one place per binding (`extract_algo_array` / `extract_algo_object`;
  `algo_rows` / `algo_object`) and every algorithm goes through it.
- **`nodeSimilarity(threshold, topK)` ignored `threshold`** in the core (#107).
- **`node_similarity(top_k=N)` ignored `top_k`** unless `threshold` was also
  set, in both bindings (#108).
- **`upsert_node` let `node_data["id"]` hijack identity** (#109): a caller
  supplied `id` no longer overrides `node_id` on create or renames the node on
  update, in both bindings.
- **Identifiers are validated before interpolation** (#110): labels, property
  keys, and A* coordinate property names must match
  `^[A-Za-z_][A-Za-z0-9_]*$`. Python raises `ValueError`; Rust returns the new
  `Error::InvalidIdentifier`. `assert_identifier` / `is_identifier` are
  exported. Relationship types keep the existing `sanitize_rel_type` contract.
- **`GraphManager` accepted path traversal in graph names** (#111): names are
  validated with the same identifier rule and must resolve inside `base_path`.
  Python raises `ValueError`; Rust returns the new `Error::InvalidGraphName`.
- **Two divergent `sanitize_rel_type` implementations in Python** (#115):
  `BulkMixin._sanitize_rel_type` is removed; bulk inserts store the same
  relationship type as the Cypher path for reserved words and empty input.

### Docs

- `sql-interface.md` documents the statistics object; `python-api.md` and
  `rust-api.md` reflect the new `GraphManager.query` and `Graph` signatures.
## [0.6.1] — 2026-08-25

Patch release fixing three reported issues (#95, #96, #97). 947/947 unit
tests pass; functional tests clean (including the new hard-assertion
regression suite `tests/functional/39_issue_regression_tests.sql`);
openCypher TCK per-scenario diff shows zero regressions.

### Fixed

- **Relationship inline property filters with parameters** (#96) —
  `MATCH ()-[r:TYPE {prop: $param}]->()` silently skipped the parameter
  and matched *every* edge of the type, which was dangerous for `SET`/
  `DELETE` scoped by such a filter. Parameter values are now matched
  against the edge property tables exactly like literals. Node patterns
  and `WHERE` clauses were unaffected.
- **`RETURN` of a relationship created between MATCH-bound nodes** (#95) —
  `MATCH (x), (y) CREATE (x)-[r:T]->(y) RETURN r` raised
  `Unknown variable: r` *after* the `CREATE` had committed. The
  MATCH+CREATE+RETURN path now projects CREATE-introduced variables
  (bare `r`, `r.prop`, aggregates, `SKIP`/`LIMIT`) with one result row
  per matched row, for both comma-pattern and multi-`MATCH` shapes.
- **MERGE with parameter-valued inline properties** — the MERGE match
  phase ignored `$param` inline properties for both nodes and edges
  (matching any node of the label / any existing edge on the triple),
  and node creation dropped them entirely. All three paths now resolve
  parameters correctly.

### Added

- **Caller-assigned edge ids for `upsert_edge`** (#97) — Python
  `upsert_edge(..., edge_id=None)` and Rust `upsert_edge_with_id(...)`
  merge on an `id` relationship property instead of the
  `(source, target, rel_type)` triple, so parallel edges between the
  same two nodes with the same type are individually addressable and
  upsertable in place. Omitting the id keeps the existing
  merge-on-triple behavior.
- Documentation: concurrent multi-process access inherits SQLite's own
  locking/WAL guarantees — stated explicitly in the architecture page.

## [0.6.0] — 2026-06-03

A large openCypher conformance release. TCK pass rate moves from **91.5%**
(3549 executable scenarios) to **97.7%** (3788 / 3876), **+239 scenarios** with
zero regressions among previously-passing scenarios. 944/944 unit tests pass;
functional tests clean.

### Added — openCypher coverage

- **Pattern & path matching completeness** — variable-length path hydration,
  `relationships()` / `nodes()` on varlen and null paths, path-wide relationship
  uniqueness, multi-rel OPTIONAL MATCH with full-pattern (all-or-none) semantics,
  bidirectional bracketed relationship patterns, and four-label conjunctions in
  expression context.
- **Temporal** — closed the non-DST temporal cluster: duration arithmetic and
  ISO round-trip normalization (months not normalized across days; 1 month =
  30.436875 days), `duration.between`, component accessors, `date`/`time`/
  `datetime`/`localdatetime` construction and selection, named-zone handling, and
  UTC-instant ordering. Cross-type ordering now uses an orderability comparator
  with a NaN sentinel and an overflow-safe `(seconds, nanoseconds)` temporal
  comparison (far-future years no longer wrap int64 epoch-nanoseconds).
- **Quantifiers** — `all` / `any` / `none` / `single` over list pipelines now
  evaluate consistently; non-deterministic CTEs (`rand()`/`RANDOM()` in WITH
  projections) are emitted `AS MATERIALIZED` so a multiply-referenced list is
  evaluated once, fixing algebraic-identity scenarios.
- **WITH / ORDER BY / aggregation** — `ORDER BY` on a non-aggregating WITH now
  flows to a downstream aggregating WITH (`… ORDER BY x WITH collect(y)` collects
  in sorted order); computed (non-aggregate) WITH keys (e.g. `a.n % 3 AS m`) are
  now GROUP BY keys; `LIMIT`/`OFFSET` inline into the WITH CTE body.
- **Existential subqueries** — `EXISTS { pattern }` brace form.
- **Validation** — compile-time `SyntaxError` diagnostics (pattern expressions in
  `SET` RHS, relationship-uniqueness, label/type 3-valued-logic predicates, and
  more).

### Fixed

- Multi-row `MATCH … CREATE` / `MATCH … DELETE … CREATE` runs the write per
  matched row; inter-pattern variable references in `CREATE` (`{x: a.id}`)
  resolve against earlier patterns; `RETURN` is pre-captured before `DELETE`.
- `REMOVE` on a null/unbound variable is a no-op; `RETURN *` excludes synthetic
  anonymous aliases.
- General-expression projection in `CREATE … RETURN` and `SET` (list/JSON props).

### Notes

Remaining gaps (~2.3%) concentrate in DST-aware timezone arithmetic, nested
existential subqueries, multi-row MERGE binding, and boolean subtype preservation
(SQLite has no native boolean type). See
[`docs/testing/semantic-coverage-matrix.md`](docs/testing/semantic-coverage-matrix.md).
Windows extension tests (`test_*_timestamp*`) remain non-blocking pending an
MSYS2/MinGW `julianday('now')` regression (GQLITE-T-0205).

## [0.5.0] — 2026-05-22

Significant TCK conformance release plus the first-time Windows extension
loading fix. TCK pass rate moves from 88% → **91.5% executable** (3486 → 3549,
+63 scenarios). 944/944 unit tests pass; functional tests clean; no
regressions among previously-passing scenarios.

### Added — CALL test.* procedures (harness-side; GQLITE-T-0252)

OpenCypher TCK Call1-Call6 declare procedures via the gherkin step
`there exists a procedure name(args) :: (yields)`. Previously 54
scenarios were skipping with "unknown step". The TCK runner now:

- Parses procedure declarations into per-scenario `_ProcedureFixture`
  records (name, arg names + types, yield names, fixture rows).
- Intercepts plain `CALL <proc>(...)` queries against registered
  procedures and synthesizes a `QueryResult` from the fixture table.
- Supports YIELD AS, WITH AS, RETURN *, RETURN col rename chains.
- Supports implicit args from `state.parameters` when CALL is paren-less.
- Validates arg count and falls through to backend so expected
  `InvalidNumberOfArguments` errors surface.
- Detects duplicate YIELD destination names → falls through for the
  expected `VariableAlreadyBound` error.
- Recognizes `ParameterMissing` as an expected error class.
- Strips embedded no-yield CALL invocations (`MATCH (n) CALL test.doNothing()
  RETURN n`) so the surrounding query runs cleanly.

Reduces skipped scenarios 54 → 4; net +42 TCK.

### Added — OPTIONAL MATCH JOIN restructure (GQLITE-T-0320)

For OPTIONAL MATCH paths with one bound + one new endpoint, the new
endpoint is now deferred to the rel handler and emitted as a
`LEFT JOIN nodes AS X ON X.id = edge.<src|tgt>_id` (or `cte.start_id/
end_id` for varlen) **after** the edge LEFT JOIN. This correlates X
through the edge instead of the previous `LEFT JOIN nodes AS X ON 1=1`
(which returned all nodes), and resolves SQLite's "ON clause
references tables to its right" restriction for OPTIONAL+varlen.

Two enhancement layers:

- **EXISTS-based collapse** for no-rel-variable / no-named-path patterns:
  replaces the edge+node LEFT JOIN cascade with a single
  `LEFT JOIN nodes AS c ON EXISTS (... edges WHERE ... AND _e.<col> = c.id)`.
  Produces one row per outer × matching c, or one row with c=NULL.
- **Defer-pair WHERE rewrite** for rel-variable cases: records
  `(edge_alias, deferred_alias, endpoint_col)` tuples at rel emission;
  at WHERE-clause handling, rewrites the WHERE SQL (replacing
  `<deferred>.id` with `<edge>.<endpoint_col>`) and injects it into
  the edge JOIN's ON. Pushes WHERE filter pre-LEFT-JOIN so non-matching
  inner rows don't multiply outer rows.

All `near 'AND': syntax error` failures eliminated. Wins: Match7 [3],
Match7 [14], Match7 [19], MatchWhere6 [1]/[2]/[3], WithWhere1 [4].
Net +7 TCK.

### Fixed — transform_match SQL emission (GQLITE-T-0261)

- `generate_node_match` end-of-buffer alias detection: the duplicate-
  detection needle required trailing whitespace, but `sql_join` doesn't
  add it. Aliases at end-of-buffer slipped through, producing duplicate
  `CROSS JOIN nodes AS X` and `ambiguous column name` errors. Now also
  matches the alias at the end of the buffer.
- Varlen `target_already_added` check: the variable-length rel handler
  emitted `CROSS JOIN nodes AS X` unconditionally even when X was
  already in scope from a prior MATCH. Mirrors the existing check from
  the non-varlen path.
- OPTIONAL + varlen LEFT JOIN with ON-clause constraints: the
  start/end/depth constraints now go in the LEFT JOIN's ON instead of
  WHERE, so unmatched outer rows are preserved with null inner vars.

Wins: Match4 [6], Match5 [19]/[21]/[23], Match7 [13]/[20], Match9 [8].
Net +7 TCK.

### Fixed — Windows extension loading

`sqlite3_graphqlite_init` is now exported via `__declspec(dllexport)` on
Windows builds. Previously the symbol existed in `graphqlite.dll` but
wasn't in the export table, so `.load build/graphqlite` reported
"The specified procedure could not be found." This was the first error
hit by every Windows functional/integration test since project start.
With the fix, **the full Windows test suite (`full-windows-tests`) now
passes** for the first time.

### Fixed — smaller items

- `transform_with.c`: LIMIT/SKIP non-literal expressions (e.g.
  `LIMIT toInteger(ceil(1.7))` or `$param`) were `atoi`-ed to 0,
  producing `LIMIT 0` which dropped all rows. Now routes through
  `sql_limit_expr` to inline the expression verbatim. (+1 TCK,
  WithSkipLimit3 [2].)
- `cypher_gram.y`: backtick-quoted identifiers (`\`name\``) are now
  accepted as `RETURN expr AS \`alias\`` and `UNWIND expr AS \`alias\``.
  (+1 TCK, Call1 [4].)
- `transform_func_aggregate.c`: `count(r)` on a variable-length-bound
  rel variable now resolves to `alias.start_id` instead of the
  non-existent `alias.id` on the recursive-CTE alias. (+1 TCK,
  Match9 [5].)

### Internal

- New `cypher_transform_context.optional_defer_pairs` array tracks
  T-0320 defer pairs across the rel handler → WHERE handler boundary.
- New buffer-level helper inserts text into a specific LEFT JOIN's ON
  by finding `' AS <alias>'` and the next JOIN keyword.
- All-time TCK conformance: 3486 → 3549 (+63 vs. 0.4.4); 91.5% of
  executable scenarios pass. Skipped 54 → 4.

### Known gaps filed as follow-ups

- `GQLITE-T-0320` (active) — multi-rel OPTIONAL MATCH needs combined-
  EXISTS for full-pattern semantics; Match7 [8]/[9]/[12]/[27] family.
- `GQLITE-T-0205` — Windows `timestamp()` returns 0 in some MATCH+SET
  / MERGE paths.
- `GQLITE-I-0043` — `transform_expression` rewrite (parent of the
  S7-S19 sql_builder migration series).

## [0.4.4] — 2026-04-18

Patch release resolving every sub-bug in GitHub issue #61 ("Cypher-to-SQL
translation bugs") and landing structural improvements in the cross-clause
dispatch layer so future sibling-path regressions are caught by tests.

### Fixed — data loss and incorrect results

- **#61.1 / GQLITE-T-0185**: `UNWIND [{id:"b"}] AS item MATCH (n:L {k: item.id}) RETURN n` returned every matching-label row instead of the UNWIND-bound one. UNWIND's list handler now serializes nested maps as JSON literals; the MATCH inline-property filter resolves property-access RHSes (e.g. `item.id`) through `var_ctx` to `json_extract(alias, '$.field')`; and UNWIND+MATCH+RETURN now routes to the transform pipeline instead of the MATCH-only handler.
- **#61.2 / GQLITE-T-0186**: `CREATE (a)-[:REL {prop: $p}]->(b)` silently stored NULL for `$param` relationship properties. Added `AST_NODE_PARAMETER` handling in the rel-create property loop mirroring the node path.
- **#61.3 / GQLITE-T-0187**: `MERGE … ON CREATE SET r.k = $p` / `ON MATCH SET r.k = $p` silently stored NULL on relationship variables. Removed the `not yet implemented` stub; `execute_set_items` was already edge-aware and is now wired from the MERGE entry points. Triggers `ON CREATE` when the MERGE produced a new edge or a new target endpoint.
- **#61.4 / GQLITE-T-0188**: `CREATE (n) SET n += {map}` / `MERGE (n) SET n += {map}` silently dropped SET. The SET code path was correct; the dispatcher was discarding the write-clause var_map before SET ran. Fix in GQLITE-I-0036.
- **#61.5 / GQLITE-T-0189**: `MATCH (a) MATCH (b) MERGE (a)-[r]->(b) SET r.x = v` raised `Unbound variable in SET: r`. Same dispatcher gap as #61.4.
- **#61.6 / GQLITE-T-0190**: `MATCH (a) MATCH (b) CREATE (a)-[:R]->(b)` produced a phantom anonymous target node; subsequent `MATCH (s)-[r]->(t) RETURN t.name` returned NULL. `execute_match_create_query` now iterates every MATCH clause in the query and unions bindings before CREATE runs.
- **#61.7 / GQLITE-T-0191**: `MATCH (n:L {k1:v1, k2:v2, k3:v3})` returned empty because the transform reused a single `_prop_<alias>` join for all three properties, producing a contradictory WHERE (`value = 'v1' AND value = 'v2' AND value = 'v3'`). Multi-property inline filters now emit a per-property `EXISTS` subquery keyed by each pair's own `pk.key`.

### Added — dispatcher plumbing (GQLITE-I-0036)

- New `execute_merge_clause_with_varmap`, `execute_match_merge_query_with_varmap`, `execute_multi_match_create_query_with_varmap`, `execute_multi_match_create_query`, and `bind_match_clause_into_varmap` helpers in `executor_internal.h`. Enable write-clause handlers to expose their `variable_map` so trailing `SET` / subsequent clauses can use the bindings.
- Dispatcher (`query_dispatch.c`) now threads var_maps through `CREATE + SET`, `MERGE + SET`, `MATCH+MERGE + SET`, `MATCH+CREATE + SET`, and `MATCH+MATCH+SET`. Pattern table `MATCH+SET` now forbids `MERGE`/`CREATE`/`WITH` so compound write patterns route correctly.
- Multi-MATCH binding aggregation: every `AST_NODE_MATCH` in the query is resolved and bindings are unioned (last-wins on same-name rebind) before the write clause runs.

### Added — test infrastructure (GQLITE-I-0035)

- `docs/testing/semantic-coverage-matrix.md`: write × target × value-source × scalar-type × read-back matrix with coverage census (~45 covered, handful of intentional gaps). Complements the existing syntax-coverage matrix.
- `tests/functional/39_issue_regression_tests.sql`: ~25 new regression entries (`#61.1` through `#61.7`, `T-0194` through `T-0198`, `T-0201` through `T-0203`) locking in end-to-end behaviour.
- `scripts/check-coverage-matrix.sh` + `coverage-matrix-check` CI job: blocks PRs that modify `src/backend/transform/` or `src/backend/executor/` without updating tests or the matrix. Override via `skip-coverage-matrix` label.
- `.github/pull_request_template.md`: PR checklist referencing the matrix.

### Changed

- `MATCH+RETURN` dispatch pattern now forbids `CLAUSE_UNWIND`; `UNWIND+MATCH+RETURN` routes to the generic transform pipeline so UNWIND is actually evaluated.
- First property pair of `MATCH (n {k1:v1, ...})` is still baked into the FROM-side JOIN; subsequent pairs now emit per-property `EXISTS` subqueries.

### Internal

- All 7 issue #61 sub-bugs tracked as `GQLITE-T-0185..T-0191`; all closed.
- GQLITE-I-0035 (Semantic Coverage Matrix) completed.
- GQLITE-I-0036 (Cross-clause variable-map threading) completed.

### Known gaps filed as follow-ups

- `GQLITE-T-0183` — UNWIND `$param` in CREATE/MERGE/SET write paths (pre-existing; distinct from #61.1 which was the read path).
- `GQLITE-T-0100` — Capability metadata API (issue #17).
- `GQLITE-T-0192` — Structured parse diagnostics + `validate(query)` API (issue #16).
- `GQLITE-T-0181` / `T-0182` / `T-0184` — pre-existing bugs, out of scope this release.

## [0.4.3] — 2026-04-17

- Spec compliance: address 5 Cypher gaps (merged from fix/59-spec-compliance-gaps).
- Internal: CALL subquery parameter binding, return-code checks, dead-code removal.

## [0.4.2] and earlier

See git history (`git log v0.4.2..v0.4.3`).
