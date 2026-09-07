---
id: f2-anchor-the-variable-length-path
level: task
title: "F2: anchor the variable-length path CTE at the bound start node"
short_code: "GQLITE-T-0358"
created_at: 2026-09-07T01:25:03.870539+00:00
updated_at: 2026-09-07T01:46:57.474628+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F2: anchor the variable-length path CTE at the bound start node

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 2: `generate_varlen_cte` anchors on every edge of the type and only the outer query filters `start_id`, so `(a {id:'x'})-[:T*1..3]->(b)` enumerates every path in the graph.

## Validation

Validated 2026-09-06 on macOS with a release build via `tests/performance/python` (harness patched for macOS in H1). 1469 ms (literal) / 1968 ms (param) at 10K nodes / 50K edges for `*1..3`; the same CTE anchored at the start node runs in well under a millisecond.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `build_anchor_ids_sql()` derives a node-id set from the start node's inline map (literals, params, multiple pairs via INTERSECT).
- [ ] `generate_varlen_cte(..., anchor_ids_sql)` applies it to the directed base case, both undirected orientations, and the zero-hop base case.
- [ ] CUnit `test_varlen_anchored_start`: directed, reversed arrow, undirected, zero-hop, multi-pair (positive and negative), parameter, and unanchored control.
- [ ] Python `test_varlen_cte_is_anchored_*` assert the SQL shape; `test_varlen_results_unchanged_by_anchoring` asserts results; TCK pass count unchanged.

Known limitation (follow-up): a start node bound only by an earlier MATCH/WITH with no inline map in the varlen pattern is still unanchored.

## Status Updates **[REQUIRED]**

- 2026-09-06: created, validated, implemented (`transform_match.c`, `cypher_transform.c`, header).
- 2026-09-06: First implementation silently did nothing for literal maps: `generate_node_match` consumes the first literal pair into the driving JOIN and sets `first_pair->key = NULL` (three sites), so the pair was gone by the time the rel handler ran (params were unaffected). Fixed by `stash_consumed_anchor()` recording the fragment on the transform context (new `anchor_*` fields, freed in `cypher_transform_free_context`) and `build_anchor_ids_sql(ctx, alias, node, out)` merging stashed + remaining pairs. Verified via EXPLAIN on literal, literal+extra pair, undirected, zero-hop, and reversed forms; functional 40.5 and Python shape tests green; CUnit `test_varlen_anchored_start` green.