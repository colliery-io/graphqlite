---
id: f7-index-driven-where-comparisons
level: task
title: "F7: index-driven WHERE comparisons instead of _gql_order_cmp over COALESCE"
short_code: "GQLITE-T-0363"
created_at: 2026-09-07T01:25:11.494202+00:00
updated_at: 2026-09-07T01:25:11.494202+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F7: index-driven WHERE comparisons instead of _gql_order_cmp over COALESCE

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 7: `WHERE n.age > 85` compiles to `_gql_order_cmp(<COALESCE>, 85, '>')`, a UDF wrapper that makes the `(key_id, value, node_id)` indexes unusable. 24 ms vs 1.6 ms range filter at 20K nodes.

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

For `<var>.<prop> <op> <literal-or-param>` with a resolvable key id, emit an index-driven EXISTS/semi-join against the typed table matching the literal's type; keep `_gql_order_cmp` as the fallback for mixed-type or computed operands. Depends on F6.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
