---
id: f9-create-compiles-26-statements
level: task
title: "F9: CREATE compiles 26 statements per node — known-new flag and schema-manager prepared statements"
short_code: "GQLITE-T-0365"
created_at: 2026-09-07T01:25:14.572139+00:00
updated_at: 2026-09-07T01:25:14.572139+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F9: CREATE compiles 26 statements per node — known-new flag and schema-manager prepared statements

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 9: CREATE issues ~26 prepare/finalize pairs per 4-property node (109 us vs 14 us raw SQL). `cypher_schema_set_node_property` deletes the key from all five typed tables before every insert; `cypher_schema_create_node` uses `sqlite3_exec`.

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

A known-new-entity flag on the create path that skips the cleanup deletes, plus prepared statements owned by the schema manager and finalised in `connection_cache_destroy` (restores the caching removed because it blocked `sqlite3_close`). Bindings should batch multi-row writes with UNWIND (13x cheaper per row).

Source: `docs/internal/performance-review.md` (PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
