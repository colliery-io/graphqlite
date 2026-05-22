---
id: e5-handle-match-set-true-two-pass
level: task
title: "E5: handle_match_set true two-pass (Set6 family acceptance)"
short_code: "GQLITE-T-0314"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T02:51:08.672188+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0310]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E5: handle_match_set true two-pass (Set6 family acceptance)

## Status Updates

**2026-05-21 — substantially complete.** All Set6 [5], [7], [19],
[20], [21] (the original acceptance criteria) now pass. Plus
Remove3 [20], [21] and WithSkipLimit1 [2] / WithSkipLimit2 [4]
unlocked as bonus from the same fixes. TCK 3468 → 3477 (+9 net
across T-0310 + T-0314).

Fixes landed across multiple iterations:

1. **transform_set.c** (generate_property_update):
   - Edge-aware table routing: `edge_props_*` + `edge_id` when
     `transform_var_is_edge()` is true.
   - Typed value-expression lookup: temporarily set
     `ctx->in_comparison=true` so property reads stay typed
     (avoids `r.num + 1` → `"1" || "1"` → `"11"` string concat).
   - Cross-table cleanup: INSERT OR REPLACE writes new value, then
     DELETEs from the OTHER 4 typed prop tables for the same
     (entity_id, key_id) pair. Without this, old `node_props_int`
     value shadowed new `node_props_text` value via the
     downstream COALESCE priority.

2. **transform_func_aggregate.c** (transform_aggregate_with_property):
   - Edge-aware LEFT JOINs: `edge_props_*` + `edge_id` when the
     aggregated variable is an edge. `sum(r.num)` over edges was
     returning NULL because the JOIN tables didn't match.

3. **cypher_transform.c** (cypher_transform_query):
   - Guard widened from `INSERT OR REPLACE` (prefix-only) to
     `strstr "INSERT OR REPLACE"` (anywhere in DML buffer). The
     cross-table DELETEs from item (1) now precede the INSERT, so
     the guard needs to recognize the compound shape.

4. **executor_set.c** (evaluate_ast_with_context):
   - Edge variables now register via `transform_var_register_edge`
     and add `FROM edges AS alias` instead of `FROM nodes`. Pre-
     fix, the per-row handle_match_set path returned "Failed to
     evaluate SET expression" for any edge property arithmetic.
   - Sets `ctx->in_comparison=true` for typed property lookup,
     fixing the same `r.num + 1` → string-concat issue for the
     per-row evaluator.

## Remaining (out of T-0314 scope)

- **Set1 [6]/[7]** — list concatenation `SET a.numbers = a.numbers + [4,5]`.
  CREATE+SET emission for list properties; transform_create
  doesn't currently store the list in `node_props_json`.
- **Remove3 [10]/[11]/[12]/[13]/[14]** — REMOVE label changes the
  matched set; the post-write SELECT re-MATCHes and finds nothing.
  Needs the snapshot-id-capture pattern (T-0297-style).
- **Delete6 [12]/[13]/[14]** — same post-write re-MATCH issue
  for DELETE.
- **Set6 [other]** — all pass now.

These remaining failures are about post-write re-MATCH semantics
(Cypher's WITH-binds-pre-write-id contract), which is the C12
var_map capture work — a separate task from T-0314's "true two-
pass" formulation.

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

True two-pass `handle_match_set`: pre-SET MATCH captures the
var_map; SET applies; RETURN projects from the captured ids (CTE-
snapshot or pin-by-id pattern from the I-0042 doc, option (c)).

Already shipped a "light" version in iteration 29 (commit f1c9ee9):
the post-SET RETURN uses synth-match-without-WHERE so the stale
property predicate doesn't drop the row. That flipped Set1 [1],
Set1 [2], Set2 [2] to pass. The hard cases — MATCH+SET+WITH+
RETURN — still fail because the SET is silently dropped at the SQL
assembly layer (the actual bug).

## Blocked by

T-0310 — sql_builder DML/SELECT split. Iteration 49's E5 attempt
traced 5 chained issues that all stem from raw_output and SELECT
sharing one buffer:

  1. transform_with_clause's sql_builder_reset wipes raw_output
  2. sql_builder_to_string appends raw_output AFTER the SELECT
  3. sqlite3_prepare_v2 only consumes one statement
  4. prepend_cte_to_sql writes WITH at the start of the whole buffer
  5. cypher_query_result carries a single prepared stmt

T-0310 lays out the proposed fix: split DML/SELECT at builder
boundary; result holder gets a `pre_exec_dml` field; executor runs
`sqlite3_exec(dml)` before stepping SELECT.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] T-0310 landed.
- [ ] Set6 [5] — Filtering after SET on nodes — passes
- [ ] Set6 [7] — Aggregating in WITH after SET on nodes — passes
- [ ] Set6 [19] — Filtering after SET on relationships — passes
- [ ] Set6 [21] — Aggregating in WITH after SET on relationships — passes
- [ ] No regression on Set1 / Set2 / Set3 / Set4 / Set5 / Set6 [1-4,6,8-18,20].
- [ ] T-0297 acceptance criteria all met.

## Affected files (depends on T-0310 shape)

- `src/backend/executor/query_dispatch.c` — handle_match_set
- `src/backend/transform/transform_set.c` — emission goes through
  the new DML buffer
- `src/backend/executor/executor_set.c` — variable map ops