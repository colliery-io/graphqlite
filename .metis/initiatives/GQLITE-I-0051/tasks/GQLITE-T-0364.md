---
id: f8-per-connection-prepared
level: task
title: "F8: per-connection prepared-statement cache keyed by Cypher text"
short_code: "GQLITE-T-0364"
created_at: 2026-09-07T01:25:12.930848+00:00
updated_at: 2026-09-07T11:18:42.989242+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F8: per-connection prepared-statement cache keyed by Cypher text

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 8: 91% of per-query instructions are `sqlite3_prepare_v2` on the generated 1-3 KB statement (~80 us fixed cost, ~12K point queries/s per connection).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

Per-connection LRU from Cypher text to (generated SQL, prepared statement, column metadata) with reset + rebind on hit; finalised from `connection_cache_destroy`. Requires SQL independent of parameter values (F1 chose that form). Also: stop calling `graphqlite_register_helper_udfs` per query and `cypher_schema_initialize` twice per connection.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).- 2026-09-07: implemented. `stmt_cache_entry` (text, AST, transform ctx, prepared stmt, RETURN clause) on the executor, LRU of 64; populated via a capture protocol armed only in `cypher_executor_execute` (text path) and honoured only by `execute_match_return_query` and `handle_generic_transform` when there is a RETURN and no pre-exec DML (dispatch disarms it for every other handler so a nested read under CALL cannot be mis-associated). Lookup happens before parsing; hits reset/clear_bindings/rebind/build_query_results/reset; any execution error evicts the entry; `in_use` guards re-entrant use of the same text. Statements are finalised from a `SQLITE_TRACE_CLOSE` hook (fires before SQLite's unfinalized-statement check, so sqlite3_close v1 works: verified by CUnit `test_stmt_cache_allows_v1_close` and the CLI suite) and by executor free. Transform never reads parameter values (verified by grep), so text-keyed caching is exact. Also: executor creates transform contexts with `cypher_transform_create_context_ex(db, false)` (22 sites) so the 51 helper UDFs are not re-registered per query. Release 20K: point lookup 65.8 -> 8.0 us hit, 51 us miss. Unit 951/951, functional, Python 408, Rust 302, CLI 19/19, golden 0 diffs.