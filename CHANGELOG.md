# Changelog

All notable changes to GraphQLite are documented here. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

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
