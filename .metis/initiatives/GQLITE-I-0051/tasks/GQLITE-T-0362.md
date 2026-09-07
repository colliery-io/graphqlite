---
id: f6-resolve-property-key-ids-at
level: task
title: "F6: resolve property key ids at transform time instead of joining property_keys per access"
short_code: "GQLITE-T-0362"
created_at: 2026-09-07T01:25:09.935843+00:00
updated_at: 2026-09-07T01:25:09.935843+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F6: resolve property key ids at transform time instead of joining property_keys per access

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 6: `n.age` compiles to a five-way COALESCE where each branch joins `property_keys` by name. 70 ms vs 41 ms per 18K rows with resolved key ids. Prerequisite for F7.

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

Give the transform a handle to the schema manager's key cache (or have the executor pre-resolve the keys referenced by the AST and pass a map), then emit `key_id = <n>` and drop the join.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
