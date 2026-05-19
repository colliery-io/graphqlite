---
id: runtime-entitynotfound
level: task
title: "Runtime EntityNotFound: DeletedEntityAccess error"
short_code: "GQLITE-T-0253"
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

# Runtime EntityNotFound: DeletedEntityAccess error

## Objective

openCypher specifies that accessing a property or relationship on an entity
that has been deleted earlier in the same query must raise the runtime
error `EntityNotFound: DeletedEntityAccess`. We currently either return
NULL or silently produce a stale value, so 3 TCK scenarios with the step
`Then 'a EntityNotFound should be raised at runtime: DeletedEntityAccess'`
end up unmatched.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [x] P3 - Low (when time permits)

### Business Justification
- **User Value**: openCypher correctness — protects against silent
  reads from rows an earlier write removed.
- **Effort Estimate**: M (executor tracking of deleted-entity ids per
  transaction; error class plumbing).

## Acceptance Criteria

- [ ] Reading any property of a node/relationship that was DELETEd earlier
      in the same Cypher statement raises
      `EntityNotFound: DeletedEntityAccess`.
- [ ] `Then 'a EntityNotFound should be raised at runtime: DeletedEntityAccess'`
      is recognised by the TCK runner (the generic `*Error|*Failure` regex
      already catches it once the engine emits the right text).
- [ ] All 3 currently-skipped scenarios convert to pass or fail (not skip).

## Source Evidence

`build/tck-results.json` skipped:
- 3 × `a EntityNotFound should be raised at runtime: DeletedEntityAccess`

## Implementation Notes

### Touch points
- `src/backend/executor/executor_delete.c` — record deleted node/edge ids.
- `src/backend/executor/query_dispatch.c` — check deleted-set before
  projecting properties.
- Error code constant + `src/extension.c` error code list.

### Dependencies
Independent.

## Status Updates

*To be added during implementation*
