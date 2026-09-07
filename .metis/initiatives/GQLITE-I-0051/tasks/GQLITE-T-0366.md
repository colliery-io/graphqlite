---
id: f10-design-a-table-valued-result
level: task
title: "F10: design a table-valued result interface (cypher_rows) to remove result amplification"
short_code: "GQLITE-T-0366"
created_at: 2026-09-07T01:25:16.154866+00:00
updated_at: 2026-09-07T01:25:16.154866+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F10: design a table-valued result interface (cypher_rows) to remove result amplification

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 10: a 6.6 MB result peaks at +44 MB; each value is copied 6-8 times through `build_query_results`, the agtype tree, double serialisation, `SQLITE_TRANSIENT`, and the binding's `json.loads`.

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

Design (ADR first, then implement) an eponymous virtual table `SELECT * FROM cypher_rows('MATCH ...')` yielding one SQLite row per result row with native column types; keep scalar `cypher()` as a compatibility wrapper. Also: flat per-array allocations for `result->data`, replace shared `static char` buffers in the transform layer (thread-safety), replace fixed `char sql[N]` buffers with `dynamic_buffer`, and fix the direct-mapped `property_key_cache` eviction.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
