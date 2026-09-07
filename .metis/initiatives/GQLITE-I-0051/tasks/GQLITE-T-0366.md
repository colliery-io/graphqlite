---
id: f10-design-a-table-valued-result
level: task
title: "F10: design a table-valued result interface (cypher_rows) to remove result amplification"
short_code: "GQLITE-T-0366"
created_at: 2026-09-07T01:25:16.154866+00:00
updated_at: 2026-09-07T13:30:00.000000+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: true
initiative_id: GQLITE-I-0051
---

# F10: design a table-valued result interface (cypher_rows) to remove result amplification

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 10: a 6.6 MB result peaks at +44 MB; each value is copied 6-8 times through `build_query_results`, the agtype tree, double serialisation, `SQLITE_TRANSIENT`, and the binding's `json.loads`.

## Acceptance Criteria **[REQUIRED]**

- [x] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [x] Numbers recorded in the PR description / CHANGELOG (50K-node `RETURN n` from Python: RSS growth 51 MB → 8 MB, first row 198 → 165 ms, end-to-end unchanged).
- [x] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

Design (ADR first, then implement) an eponymous virtual table `SELECT * FROM cypher_rows('MATCH ...')` yielding one SQLite row per result row with native column types; keep scalar `cypher()` as a compatibility wrapper. Also: flat per-array allocations for `result->data`, replace shared `static char` buffers in the transform layer (thread-safety), replace fixed `char sql[N]` buffers with `dynamic_buffer`, and fix the direct-mapped `property_key_cache` eviction.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
- 2026-09-07: design written as ADR [[GQLITE-A-0006]] (eponymous virtual table `cypher_rows`, streaming one row at a time, reusing the F8 statement cache, `cypher()` kept as compatibility surface). Not implemented in PR #119 per the review's recommendation to design first.

- 2026-09-07: Implemented on PR #119 (branch `perf/review-phase1`) as `src/backend/runtime/cypher_rows_vtab.c` + `src/include/runtime/cypher_rows_vtab.h`, registered from `sqlite3_graphqlite_init` with a getter (`cache_get_executor`) that shares the per-connection executor with `cypher()`. Deviation from the ADR draft, recorded there: fixed schema (`row`, `cols`, `ncols`, `c0`..`c31`, hidden `query`/`params`) because an eponymous virtual table declares its columns at xConnect before the query text is known. Phase A: result still materialised in C, then streamed (no output buffer, no SQLITE_TRANSIENT copy of the whole payload, no whole-string JSON decode). Bindings: Python `Connection.iter_rows()` / `Graph.iter_query()`, Rust `Connection::cypher_rows_each()`. Tests: `tests/functional/41_cypher_rows.sql` (9 assertions: native types, row/cols parity with cypher(), params, LIMIT/WHERE/join, stats row, json_each, zero rows/NULL, repeat use), Python `TestCypherRows` (6), Rust `test_cypher_rows_each`. Docs: sql-interface.md, python-api.md, rust-api.md, CHANGELOG. Follow-up (not started): phase B, stepping the underlying statement inside xNext.
