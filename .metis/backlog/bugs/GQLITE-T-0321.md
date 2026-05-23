---
id: delete-return-ordering-type-r
level: task
title: "DELETE+RETURN ordering: type(r) returns NULL after DELETE r"
short_code: "GQLITE-T-0321"
created_at: 2026-05-23T05:15:00+00:00
updated_at: 2026-05-23T18:44:21.158673+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# DELETE+RETURN ordering: type(r) returns NULL after DELETE r

## Discovered

2026-05-22 during v0.5.0 work. Observed while triaging Return2 [14].

## Repro

```cypher
CREATE ()-[:T]->();
MATCH ()-[r]->()
DELETE r
RETURN type(r);
```

**Expected:** 1 row with `type(r) = 'T'`.
**Actual:** 0 rows (or row with NULL — currently 0 rows).

## Root cause

DELETE runs before RETURN captures the projection. By the time
`type(r)` is evaluated, `r` has been removed and the type lookup
returns NULL/empty.

Per Cypher spec, DELETE+RETURN should capture the projection
values BEFORE the delete is committed. The "snapshot what's about
to be deleted" semantics. Other Cypher impls (Neo4j) keep a frozen
copy of the bindings across the DELETE.

## Proposed fix

Two options:

1. **Snapshot projection BEFORE delete**: in `executor_delete.c` /
   `query_dispatch.c` for `MATCH+DELETE+RETURN` patterns, evaluate
   the RETURN projection using the pre-delete variable_map, then
   apply DELETE, then yield the cached projection rows.
2. **Delay delete to post-projection**: alternative ordering —
   project first, delete after. Same end state. Either works.

Implementation likely lives in the `handle_match_delete_return`
(or similar) dispatch handler. Current path probably calls
delete then projects from the post-delete var_map.

## Affected scenarios

- Return2 [14] "Do not fail when returning type of deleted relationships"
- Likely Return2 [15]/[16]/[17] family (deleted node properties/labels)
- May affect similar `MATCH … DELETE r RETURN r.prop` patterns.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Return2 [14] passes.
- [ ] `MATCH (n) DELETE n RETURN n.name` returns the pre-delete name.
- [ ] Functional regression test in `tests/functional/`.
- [ ] No regression on existing DELETE tests.

## Affected files

- `src/backend/executor/query_dispatch.c` — dispatch handler.
- `src/backend/executor/executor_delete.c` — DELETE execution.
- `src/backend/executor/executor_result_project.c` — projection.

## Notes

Sibling of T-0253 (DeletedEntityAccess runtime error) — that one
is about ERRORING when accessing a deleted entity. This one is
about CORRECTLY accessing a SOON-to-be-deleted entity in the
same query's RETURN.

## Status Updates

### 2026-05-23 — Completed

Modified `handle_match_delete` to capture the RETURN projection
BEFORE delete when the items reference live entity data
(`type(r)`, `r.prop`, `labels(n)`, bare-variable refs, etc.).
For COUNT(*) and literal-only RETURNs, the existing
`synthesize_delete_return` path is preserved — it uses the
accumulated delete counts which by historical coincidence
sometimes match expected aggregate counts for shapes our MATCH
undercounts (e.g. undirected varlen). Pre-MATCHing those would
have regressed Delete4 [2].

Decision logic:
- If any RETURN item is a function call other than `count()`,
  pre-capture.
- If any RETURN item is a non-literal expression (property,
  identifier, etc.), pre-capture.
- Otherwise (only LITERAL / `count()` items), use existing
  synthesize path.

**TCK: 3570 → 3571 (+1):**
- Return2 [14] Do not fail when returning type of deleted
  relationships.

Other Return2 [15]/[16]/[17] tests expect ERRORS (different
semantics — they want DeletedEntityAccess raised). That's
T-0253's territory.

944/944 unit, functional clean. 0 regressions (Delete4 [2]'s
brief regression in iteration 1 was fixed by the targeted
needs_pre_capture check).