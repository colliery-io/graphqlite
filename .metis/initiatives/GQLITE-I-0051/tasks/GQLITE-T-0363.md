---
id: f7-index-driven-where-comparisons
level: task
title: "F7: index-driven WHERE comparisons instead of _gql_order_cmp over COALESCE"
short_code: "GQLITE-T-0363"
created_at: 2026-09-07T01:25:11.494202+00:00
updated_at: 2026-09-07T10:56:49.174431+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F7: index-driven WHERE comparisons instead of _gql_order_cmp over COALESCE

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 7: `WHERE n.age > 85` compiles to `_gql_order_cmp(<COALESCE>, 85, '>')`, a UDF wrapper that makes the `(key_id, value, node_id)` indexes unusable. 24 ms vs 1.6 ms range filter at 20K nodes.

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

For `<var>.<prop> <op> <literal-or-param>` with a resolvable key id, emit an index-driven EXISTS/semi-join against the typed table matching the literal's type; keep `_gql_order_cmp` as the fallback for mixed-type or computed operands. Depends on F6.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).- 2026-09-07: implemented. `where_conjunct` context flag (set at MATCH/OPTIONAL MATCH/WITH WHERE roots; transform_expression clears it for non-binary nodes; AND keeps it); `plan_index_comparison`/`emit_index_comparison` in transform_expr_ops.c rewrite `<entity>.<prop> <op> <literal>` (either order; int/real literal -> int+real tables, string -> text, bool -> bool for `=` only) into `id IN (SELECT ... WHERE key_id = K AND value <op> lit)`; edges use the `+` post-filter form; unknown keys fall back. The AND handler skips its `_gql_bool()` wrapper for operands taking the index form so the planner sees the term. Release 20K: range filter 10.6 -> 1.7 ms, equality 6.9 -> 0.08 ms; plan drives from idx_node_props_int_key_value. 16 semantic probes incl. NOT/OR/RETURN/OPTIONAL/WITH/cross-type verified; golden 0 diffs; unit 950, functional, Python 400 (+6 F5-F7 tests), Rust 302, TCK 3788. Parameters as the literal side are deliberately not rewritten (typeof-guarded form is a follow-up).
