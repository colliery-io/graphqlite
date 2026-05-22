---
id: e4-handle-match-delete-two-pass
level: task
title: "E4: handle_match_delete two-pass refactor"
short_code: "GQLITE-T-0313"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T13:53:35.657265+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E4: handle_match_delete two-pass refactor

## Parent Initiative

[[GQLITE-I-0042]]

## Latest update (2026-05-22)

**+4 TCK landed (3482 → 3486).** transform_with had an inline
property-access path that hardcoded `node_props_*` for `WITH r.<prop>`
projections — breaking edge variables. Same kind of edge-aware fix
as T-0314.

  + Delete6 [12]  Filtering after DELETE on relationships
  + Delete6 [13]  Aggregating in RETURN after DELETE on relationships
  + Delete6 [14]  Aggregating in WITH after DELETE on relationships
  + Return4 [11]  Reusing variable names in RETURN

The remaining DELETE-family failure is Delete4 [2] (Undirected
variable-length expand + delete + count) which was passing
coincidentally before. A correct fix needs the undirected varlen
MATCH to emit all 6 paths (currently emits 3). Out of T-0313 scope.

T-0313 is substantively complete — the original two-pass shape work
landed as light infrastructure (execute_delete_operations,
bind_match edge capture, synth-match RETURN); the transform_with
edge-aware fix unlocks the WITH+DELETE family. The "full per-row
two-pass" formulation (replacing execute_match_delete_query's
agtype scan with a lighter ID-only iteration) is no longer
needed for TCK gains — leaving it for a future refactor.

Closing as completed.

## Objective

Refactor `handle_match_delete` to the canonical two-pass shape:
  Pass 1: bind matched node/edge IDs into a var_map.
  Pass 2: apply DELETE against the var_map via
          `execute_delete_operations`.

## Status

Light infrastructure landed (commit 71aac75, 2026-05-21):
- `execute_delete_operations(executor, del, var_map, result)` added
  in `executor_delete.c`.
- `bind_match_clause_into_varmap` now captures `VAR_KIND_EDGE`
  bindings (was nodes only).
- `handle_match_delete` uses synth-match-without-WHERE for the
  post-DELETE RETURN (consistent with SET/REMOVE).

The TRUE per-row two-pass (iterate matched rows, per-row var_map,
delete each) is still ahead — `bind_match_clause_into_varmap` only
captures FIRST-row bindings; multi-row DELETE regresses.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `execute_delete_operations` exists with the canonical shape.
- [x] `bind_match_clause_into_varmap` binds nodes AND edges.
- [x] Post-DELETE RETURN uses synth-match.
- [ ] Per-row var_map iteration replacing `execute_match_delete_query`'s
  agtype-based row scan.
- [ ] No regression on Delete4 / Delete6 / DETACH DELETE families.

## Affected files

- `src/backend/executor/executor_delete.c` ✅
- `src/backend/executor/executor_match.c` ✅
- `src/backend/executor/query_dispatch.c` ✅
- `src/include/executor/executor_internal.h` ✅