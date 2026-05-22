---
id: e4-handle-match-delete-two-pass
level: task
title: "E4: handle_match_delete two-pass refactor"
short_code: "GQLITE-T-0313"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T00:12:47.008348+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"

exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E4: handle_match_delete two-pass refactor

## Parent Initiative

[[GQLITE-I-0042]]

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
