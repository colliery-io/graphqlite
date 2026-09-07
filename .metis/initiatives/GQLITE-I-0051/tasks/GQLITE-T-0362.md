---
id: f6-resolve-property-key-ids-at
level: task
title: "F6: resolve property key ids at transform time instead of joining property_keys per access"
short_code: "GQLITE-T-0362"
created_at: 2026-09-07T01:25:09.935843+00:00
updated_at: 2026-09-07T10:56:49.047646+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F6: resolve property key ids at transform time instead of joining property_keys per access

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 6: `n.age` compiles to a five-way COALESCE where each branch joins `property_keys` by name. 70 ms vs 41 ms per 18K rows with resolved key ids. Prerequisite for F7.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

Give the transform a handle to the schema manager's key cache (or have the executor pre-resolve the keys referenced by the AST and pass a map), then emit `key_id = <n>` and drop the join.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).- 2026-09-07: implemented. `cypher_transform_property_key_id()` (per-context cache keyed by graph prefix + key name, negative results cached) and `append_prop_branch()` in transform_expr_ops.c; `transform_property_access` (node/edge, comparison/projection variants, startNode()/endNode() base) now emits a (id, key_id) primary-key probe per typed table and keeps the property_keys name join only for unknown keys. Release 20K: props projection 51.5 -> 46 ms, 3-prop edge projection 369 -> 336 ms, range filter 13.5 -> 10.6 ms. Golden diff 0; unit 950/950 (needed `angreal dev clean` after the header struct change), functional, Python 400, Rust 302, TCK 3788. Other name-join copies remain in transform_with.c, transform_expr_predicate.c, transform_func_aggregate.c (follow-up).