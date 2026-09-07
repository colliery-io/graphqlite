---
id: f1-parameterized-inline-property
level: task
title: "F1: parameterized inline property match compiles to a full node/edge scan"
short_code: "GQLITE-T-0357"
created_at: 2026-09-07T01:25:02.491707+00:00
updated_at: 2026-09-07T01:46:56.265916+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F1: parameterized inline property match compiles to a full node/edge scan

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 1: `MATCH (n {id: $id})` compiles to an OR of four correlated `EXISTS` subqueries that SQLite plans as `SCAN nodes`; the literal form drives from the value index. Every binding convenience method uses `$id`, so `get_node`, `has_node`, `get_neighbors`, `delete_node` and `upsert_node` degrade linearly with graph size.

## Validation

Validated 2026-09-06 on macOS with a release build via `tests/performance/python` (harness patched for macOS in H1). Literal lookup 0.09 ms vs `$id` 9.9 ms at 10K nodes; edge inline `{weight: $w}` 56 ms.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Node and edge inline parameter filters emit an `IN (... UNION ALL ...)` semi-join over the typed tables.
- [ ] `EXPLAIN QUERY PLAN` shows no `SCAN nodes` and uses `idx_node_props_*_key_value` (Python test `test_param_inline_match_is_index_driven`).
- [ ] CUnit `test_param_typed_inline_filters`: text/int/real/bool params, two-pair maps, no-label form, edge real param, and non-matching values.
- [ ] Functional `40_json_escaping.sql` 40.4 asserts the SQL shape.

## Status Updates **[REQUIRED]**

- 2026-09-06: created, validated, implemented in `transform_match.c` (node + edge sites). Unit 948/948 before new tests.
- 2026-09-06: CUnit `test_param_typed_inline_filters` + Python `test_param_inline_*` + functional 40.4 green; EXPLAIN QUERY PLAN confirms `idx_node_props_text_key_value` drives the lookup and no `SCAN nodes`.
- 2026-09-06: Release-build comparison vs a worktree build of v0.7.0 at 10K nodes: `$id` lookup 7.7 ms -> 0.08 ms, `$id` SET 15.9 -> 0.17 ms. The edge form initially REGRESSED (43 -> 125 ms): with an anonymous source node scanned first, SQLite used the IN-list as an inner rowid term and looped over ~500 matching edge ids per outer node. Fixed by emitting `+alias.id IN (...)` for edges (materialised once, checked per row: 3.3 ms); nodes keep the IN-driver form because it is the fastest when the node is the outermost table (0.01 ms vs 0.19 ms with `+`).