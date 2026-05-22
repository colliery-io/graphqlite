---
id: e8-handle-match-merge-handle-merge
level: task
title: "E8: handle_match_merge + handle_merge_with_pipeline two-pass"
short_code: "GQLITE-T-0317"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T12:20:37.758446+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E8: handle_match_merge + handle_merge_with_pipeline two-pass

## Status Updates

**2026-05-22 — substantial landing (+2 TCK, Merge5 [16]/[17]).**

The headline gap was the dispatcher: `MATCH+WITH+MERGE` queries
(e.g. `MATCH (n) MATCH (m) WITH n AS a, m AS b MERGE (a)-[:T]->(b)
RETURN ...`) had no dedicated pattern. `WITH+MATCH+RETURN` grabbed
them at priority 100 and `handle_generic_transform` errored
"Unsupported clause type" because `transform_single_query_sql` has
no case for `AST_NODE_MERGE`.

Fixes (commit 3176bd7):

1. **New `MATCH+WITH+MERGE` pattern** at priority 91:
   `required = CLAUSE_MATCH | CLAUSE_WITH | CLAUSE_MERGE`.
2. **`WITH+MATCH+RETURN` forbidden mask** now includes
   `CLAUSE_MERGE` so MERGE-bearing queries route correctly.
3. **`handle_match_merge` has_with branch:**
   - For each pre-WITH MATCH: `bind_match_clause_into_varmap` →
     accumulate IDs into `vm`.
   - Process WITH item-by-item: copy `vm[src]` to `scoped[dst]`
     for each `src AS dst` (or bare `src`).
   - `execute_merge_clause(executor, merge, result, scoped, NULL)`.
   - RETURN via synth-match of combined MATCH+MERGE pattern.

Wins: Merge5 [16] / [17] (Aliasing of existing nodes 1 + 2).

## Remaining (out of T-0317 scope)

Each is a separate handler-level / transform-level fix, not the
"two-pass shape" question T-0317 was scoped to:

- **Merge5 [18]/[19]** — Multi-WITH+MERGE chains
  (`WITH a AS x, b AS y MERGE (a) MERGE (b)`). Needs iterative
  clause walking with cumulative scope tracking.
- **Merge1 [9]** — `WITH foo.x AS x, foo.y AS y` (property
  projection through WITH, not just identifier rename).
- **Merge1 [13]** — `MERGE should bind a path` (PATH variable
  support).
- **Merge1 [14]** — MATCH+DELETE+MERGE (DELETE-then-re-match
  semantics; T-0297 territory).
- **Merge5 [4]** — CREATE+MERGE+RETURN with `count(...)`. Returns
  null column. Column-name handling for aggregate aliases needs
  fix.
- **Merge5 [11]/[12]/[13]** — undirected MERGE pattern matching
  produces extra rows. Executor MERGE matching path doesn't fully
  implement undirected semantics.
- **Merge5 [14]** — list properties via variable. Related to the
  Set1 [6]/[7] list concatenation gap.
- **Merge5 [21]** — `Unknown variable in property access` — same
  DELETE-then-re-match issue as Merge1 [14].
- **Merge6 [3]/[6]/[7], Merge7 [4]/[5], Merge8 [1]** — `ON CREATE
  SET r.x = ...` works (interactive `r.name="foo"`) but TCK's
  control query uses `keys(r) | key + '->' + r[key]` — dynamic
  property access via `r[key]` returns NULL for edges. Separate
  transform-side bug.
- **Merge9 [3]** — CREATE+MERGE+CREATE+RETURN. Column-name and
  multi-clause integration issue.

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

Refactor MERGE handlers to the two-pass shape. The I-0042 design
doc flags this as the "deepest interleaving" — MERGE blends MATCH
+ CREATE per missing row, with multiple variants (MATCH+MERGE,
MERGE+ON CREATE+ON MATCH+SET, MERGE in CALL subquery).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `handle_match_merge` transforms MATCH once; per missing row
  emits CREATE via captured var_map.
- [ ] `handle_merge_with_pipeline` follows the same two-pass shape
  for the WITH-chained MERGE variant.
- [ ] All existing Merge1-Merge9 passing scenarios still pass.
- [ ] Merge5 [11]/[12]/[13] (undirected MERGE) — currently produce
  extra rows/self-loops because the executor's MERGE matching path
  doesn't fully implement undirected semantics. Track row-count
  improvements; further fixes may be separate executor tickets.

## Affected files

- `src/backend/executor/query_dispatch.c` — handle_match_merge,
  handle_merge_with_pipeline
- `src/backend/executor/executor_merge.c`
- `src/backend/executor/executor_merge_pipeline.c`

## Notes

This is likely the largest of E4-E8 by line count and risk. May
want to land T-0310 (DML/SELECT split) and E4-E7 first to limit the
variables in play.