---
id: p4-named-path-through-with
level: task
title: "P4: Named path through WITH / OPTIONAL varlen null"
short_code: "GQLITE-T-0337"
created_at: 2026-05-27T02:49:38.770497+00:00
updated_at: 2026-05-27T02:49:38.770497+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P4: Named path through WITH / OPTIONAL varlen null

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

Two related path-variable bugs:
1. **Named path through WITH** — `MATCH p=... WITH p ...` errors
   **"no such column: p"** because the path variable is not carried through the
   WITH CTE projection.
2. **OPTIONAL varlen named path** — an OPTIONAL variable-length named path that
   doesn't match returns `[]` instead of SQL `null`.

Target scenarios: **With1 [4], Match9 [9]**.

## Acceptance Criteria

- [ ] With1 [4] and Match9 [9] move to pass.
- [ ] `MATCH p=... WITH p RETURN p` projects the path through the WITH boundary.
- [ ] OPTIONAL varlen named path with no match returns null, not `[]`.
- [ ] Zero TCK regressions; unit 944/944; functional clean.
- [ ] TCK delta logged here and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

The path variable is currently a synthetic column produced during pattern
hydration but not added to the WITH CTE's projection list. Carry the path's
backing column(s) (elem_ids + hydration expr) through the WITH projection so
downstream clauses can reference `p`. For the OPTIONAL-null case, distinguish
"matched empty path" from "no match" — emit null when the OPTIONAL pattern
produced no row rather than an empty-list hydration. Changes in
`transform_with.c` (projection threading) and the path-hydration emission.

### Dependencies

Path hydration shares code with P1/P2's varlen work; sequence after P1 if the
elem_ids column shape changes. OPTIONAL-null overlaps P3's OPTIONAL emission.

### Risk Considerations

WITH-projection threading is used by many scenarios; a regression here is
broad. Diff the full WITH-family TCK features specifically.

## Status Updates

*To be added during implementation*
