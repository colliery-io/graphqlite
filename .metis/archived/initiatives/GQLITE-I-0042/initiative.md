---
id: executor-finalize-sequencing
level: initiative
title: "Executor finalize-sequencing refactor — defer SQL serialization until all clauses transformed"
short_code: "GQLITE-I-0042"
created_at: 2026-05-20T14:19:21.169084+00:00
updated_at: 2026-05-22T13:58:44.600415+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: L
initiative_id: executor-finalize-sequencing
---

# Executor finalize-sequencing refactor

## Context

Discovered 2026-05-19/20 during the I-0039 sql_builder migration. The
per-clause executor handlers (`handle_match_set`, `handle_match_delete`,
`handle_match_remove`, `handle_match_create`, etc. in
`src/backend/executor/query_dispatch.c`) interleave **transform** and
**execute** at clause boundaries:

```
handle_match_set(query):
  1. transform_match_clause(ctx, match)         # populates unified_builder
  2. (transform_return_clause calls finalize    # only when RETURN present)
  3. transform_set_clause(ctx, set)             # writes via sql_raw → raw_output
  4. sqlite3_prepare_v2(ctx->sql_buffer, ...)   # executes the assembled SQL
```

The I-0039 migration moved DML emitters (set/delete/remove/create) to
`sql_raw → unified_builder->raw_output`. To keep the existing handlers
working, a "drain" step in `cypher_transform_query` lifts `raw_output`
into `sql_buffer` before prepare. The result: `finalize_sql_generation`
is called in TWO places (mid-flow inside transform_return, and at end
via the drain), and the SQL-assembly layer has implicit assumptions
that the I-0039 target ("no append_sql, all output through
unified_builder") can't honor.

The fix: change the executor contract to

> **All clauses transform first → finalize ONCE → execute.**

Today's interleaving prevents this because some handlers prepare/execute
intermediate SQL between transform stages (notably the pre-SET MATCH
in `handle_match_set` and the per-row CALL outer-MATCH in
`handle_call_subquery`).

## Goals

- **G1**: Every `handle_*` in `query_dispatch.c` (and the
  `executor_call_subquery.c`, `executor_merge_pipeline.c` satellites)
  follows the same contract: transform-all → finalize-once → execute.
  No interleaved transform/execute.
- **G2**: `finalize_sql_generation` is idempotent and called once at
  the transform→execute boundary. No transform function calls it
  internally.
- **G3**: The drain shim is removed. `unified_builder` is the sole
  assembly point; `sql_buffer` is filled exactly once by finalize.
- **G4**: Zero TCK regression. Current pass count (3468) is the floor.

## Non-Goals

- Migrating `transform_expression` and its dispatched function
  transforms (that's [[GQLITE-I-0043]]).
- Removing `ctx->sql_buffer` entirely (depends on I-0043).
- Changing the `sql_builder` API surface (additions are fine).

## Architecture: the SQL-assembly blocker

A naive E2 attempt (2026-05-20) crashed TCK 3422 → 854. Root cause:
multiple call sites (write-only queries, CALL subqueries, MERGE
pipelines, UNION branches) depend on finalize being called mid-flow.
A blanket relocate doesn't work in isolation.

Iteration-49 E5 attempt traced a second deeper issue: the SQL-
assembly layer can't represent "DML preceding SELECT" cleanly today.
`sqlite3_prepare_v2` only consumes one statement; raw_output and
SELECT share one buffer; `prepend_cte_to_sql` writes WITH at the
start of the assembled string. Both architecture fixes are now
[[GQLITE-T-0310]] (DML/SELECT split at builder boundary).

## Decomposition

This initiative was decomposed into single-task chunks 2026-05-22:

  [[GQLITE-T-0310]] — DML/SELECT split at builder boundary
                     (architectural unblocker for E5/E6)
  [[GQLITE-T-0297]] — Pre-existing C12 / pre-SET var_map capture (kept)
  [[GQLITE-T-0311]] — E2: relocate finalize in transform_single_query_sql
  [[GQLITE-T-0312]] — E3: same in cypher_transform_query + drain removal
  [[GQLITE-T-0313]] — E4: handle_match_delete two-pass (light fix shipped;
                     full pending)
  [[GQLITE-T-0314]] — E5: handle_match_set true two-pass
                     (Set6 family acceptance)
  [[GQLITE-T-0315]] — E6: handle_match_remove two-pass
  [[GQLITE-T-0316]] — E7: handle_call_subquery — hoist per-row transforms
  [[GQLITE-T-0317]] — E8: handle_match_merge + handle_merge_with_pipeline
  [[GQLITE-T-0318]] — E9: full TCK regression gate
  [[GQLITE-T-0319]] — E10: audit bind_match_clause_into_varmap helpers

**E1** (idempotent `sql_builder_to_string` + auto-unfinalize wrapper)
landed pre-decomposition in commit 3e93054 (2026-05-21).

## Dependency graph

```
T-0310 ──┬─→ T-0314 (E5) ─┐
         └─→ T-0315 (E6) ─┤
                          ├─→ T-0311 (E2) ─→ T-0312 (E3) ─→ T-0318 (E9)
T-0313 (E4) ──────────────┤
T-0316 (E7) ──────────────┤
T-0317 (E8) ──────────────┘
T-0319 (E10) — independent
```

E10 can land any time. T-0310 is the architectural unblocker.

## Completion gate

I-0042 transitions to completed when:
- All sub-tasks (T-0310 through T-0319, plus T-0297) are completed
  and archived.
- T-0318 (E9 regression gate) shows pass count ≥ 3468 baseline.
- No `append_sql` calls remain in `src/backend/` that route through
  the legacy sql_buffer scratchpad. (Final pass after I-0043 lands.)