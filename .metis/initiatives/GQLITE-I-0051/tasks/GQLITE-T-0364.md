---
id: f8-per-connection-prepared
level: task
title: "F8: per-connection prepared-statement cache keyed by Cypher text"
short_code: "GQLITE-T-0364"
created_at: 2026-09-07T01:25:12.930848+00:00
updated_at: 2026-09-07T01:25:12.930848+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F8: per-connection prepared-statement cache keyed by Cypher text

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 8: 91% of per-query instructions are `sqlite3_prepare_v2` on the generated 1-3 KB statement (~80 us fixed cost, ~12K point queries/s per connection).

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

Per-connection LRU from Cypher text to (generated SQL, prepared statement, column metadata) with reset + rebind on hit; finalised from `connection_cache_destroy`. Requires SQL independent of parameter values (F1 chose that form). Also: stop calling `graphqlite_register_helper_udfs` per query and `cypher_schema_initialize` twice per connection.

Source: `docs/internal/performance-review.md` (PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
