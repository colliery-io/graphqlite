---
id: e5-handle-match-set-true-two-pass
level: task
title: "E5: handle_match_set true two-pass (Set6 family acceptance)"
short_code: "GQLITE-T-0314"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T00:12:47.008348+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0310]
archived: false

tags:
  - "#task"
  - "#phase/todo"

exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E5: handle_match_set true two-pass (Set6 family acceptance)

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
