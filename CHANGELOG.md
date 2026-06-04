# Changelog

All notable changes to GraphQLite are documented here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

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
