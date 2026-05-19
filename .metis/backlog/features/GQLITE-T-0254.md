---
id: constraintverificationfailed
level: task
title: "ConstraintVerificationFailed: DeleteConnectedNode (DELETE w/o DETACH)"
short_code: "GQLITE-T-0254"
created_at: 2026-05-19T11:12:43.647420+00:00
updated_at: 2026-05-19T11:12:43.647420+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
initiative_id: NULL
---

# ConstraintVerificationFailed: DeleteConnectedNode (DELETE w/o DETACH)

## Objective

`DELETE n` (without `DETACH`) on a node that still has connected
relationships must raise `ConstraintVerificationFailed: DeleteConnectedNode`
at runtime per the openCypher spec. We currently either silently cascade
or leave the graph in an inconsistent state, and 1 TCK scenario
(`a ConstraintVerificationFailed should be raised at runtime:
DeleteConnectedNode`) is reported as skipped because the step has no
matcher beyond the generic `*Error|*Failure` — which would convert it to
"fail" if added without the engine actually producing the error.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [x] P3 - Low (when time permits)

### Business Justification
- **User Value**: openCypher correctness — prevents accidental
  cascade-deletes.
- **Effort Estimate**: S (one check at DELETE-without-DETACH time).

## Acceptance Criteria

- [ ] Executing `DELETE n` (no DETACH) where `n` has any incident edge
      raises `ConstraintVerificationFailed: DeleteConnectedNode` and
      leaves the graph unchanged.
- [ ] `DETACH DELETE n` continues to remove `n` and its edges (current
      behaviour preserved).
- [ ] The 1 skipped scenario converts to pass.

## Source Evidence

`build/tck-results.json` skipped:
- 1 × `a ConstraintVerificationFailed should be raised at runtime: DeleteConnectedNode`

## Implementation Notes

### Touch points
- `src/backend/executor/executor_delete.c` — pre-check incident edges
  when `detach == false`.
- Error code constant + `src/extension.c` error code list.

### Dependencies
Independent of [[runtime-entitynotfound]] but adjacent (both touch the
delete error path).

## Status Updates

*To be added during implementation*
