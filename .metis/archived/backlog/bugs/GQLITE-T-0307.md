---
id: label-predicate-ignored-when
level: task
title: "Label predicate ignored when variable already bound via WITH ((a:X) after WITH a)"
short_code: "GQLITE-T-0307"
created_at: 2026-05-21T11:28:12.562592+00:00
updated_at: 2026-05-21T11:28:12.562592+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"
  - "#bug"


exit_criteria_met: false
initiative_id: NULL
---

# Label predicate ignored when variable already bound via WITH

## Reproducer

```sql
.load build/graphqlite.dylib
SELECT cypher('CREATE ()-[:T]->()');
SELECT cypher('MATCH (a1)-[r]->() WITH r, a1 MATCH (a1:X)-[r]->(b2) RETURN a1, r, b2');
-- Got: 1 row (node a1 returned without enforcing the X label)
-- Expected: 0 rows (a1 has no labels, so :X filters everything out)
```

## Root cause hypothesis

When a variable is already bound (via WITH or earlier MATCH) and a
subsequent MATCH adds new constraints to it — like a label predicate
`(a1:X)` — `transform_match.c` skips the variable's table/JOIN because
it's already attached. But it also skips emitting the new label
constraint as a WHERE. The label is silently dropped.

The fix is in the same area as T-0306: the WITH-bound variable
re-binding path should still emit additional predicates (labels,
property maps) as WHERE/JOIN-ON conditions, not skip them entirely.

## Affected TCK scenarios (at least)

- `Match3 [25]` — Matching twice with an additional node label

Other tests with rebinding + label/property constraint after WITH
likely have the same issue but mask it because no rows match either
way (rebinding semantics are mostly null/empty paths).

## Affected files

- `src/backend/transform/transform_match.c` — the WITH-bound-variable
  re-attachment path (around lines 626–680 where `is_from_with` sets
  `need_from_clause = false`)

## Acceptance Criteria

## Acceptance Criteria

- [x] Reproducer returns 0 rows (label predicate enforced)
- [x] No regression on existing WITH-rebinding scenarios (944/944 unit)
- [x] Match3 [25] flips from fail → pass (TCK 3422 → 3423)

## Status Updates

**2026-05-21** — Completed in commit 8f63888.

Added label-predicate emission to the `is_from_with` branch in
transform_match.c. For each label on the re-bound node pattern, emit
`EXISTS (SELECT 1 FROM node_labels WHERE node_id = <alias> AND label = '<label>')`
into the WHERE clause. The alias is the existing column reference
(e.g. `_with_0.a1`) which holds the node id.

Open follow-on: property-predicate rebinding (`(a1 {prop: x})` after
WITH a1) likely has the same drop pattern; not addressed in this
commit.

## Discovered

2026-05-21 during iteration 13 of the open-work queue after auditing
"row count: expected 0 got 1" failures.

## Related

- T-0306 — same surface (WITH-bound variable re-attachment) but for
  alias-as-table-name bug; fixed for non-varlen target.