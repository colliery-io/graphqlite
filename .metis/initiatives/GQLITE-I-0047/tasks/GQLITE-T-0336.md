---
id: p3-multi-rel-optional-match
level: task
title: "P3: Multi-rel OPTIONAL MATCH combined-EXISTS join ordering"
short_code: "GQLITE-T-0336"
created_at: 2026-05-27T02:49:35.864661+00:00
updated_at: 2026-05-27T02:49:35.864661+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P3: Multi-rel OPTIONAL MATCH combined-EXISTS join ordering

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

Fix `OPTIONAL MATCH (x)-[:E1]->(y)-[:E2]->(z) WHERE ...`, which currently
errors **"ON clause references tables to its right"** — the emitted LEFT JOIN's
ON condition references a table that hasn't been joined yet. A multi-rel
OPTIONAL pattern must be all-or-null as a whole, which calls for a
combined-EXISTS shape rather than chained LEFT JOINs.

Target scenarios: **MatchWhere6 [5]/[7], Match7, Match4 [7]**. Residual of
I-0044's B1 (T-0320 / T-0330).

## Acceptance Criteria

- [ ] MatchWhere6 [5]/[7], Match7 and Match4 [7] failing scenarios move to pass.
- [ ] No "ON clause references tables to its right" errors on multi-rel OPTIONAL.
- [ ] Single-rel OPTIONAL MATCH semantics unchanged (no regressions).
- [ ] Zero TCK regressions; unit 944/944; functional clean.
- [ ] TCK delta logged here and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

The full OPTIONAL pattern should be emitted so its endpoints are null-or-all:
generate the multi-rel pattern as a single correlated subquery (EXISTS /
LEFT JOIN against a derived table that materializes the whole pattern), so the
inner joins resolve their own table ordering and the outer query only sees the
combined result. This mirrors the combined-EXISTS approach already used for
single-rel OPTIONAL in I-0044. Changes in the OPTIONAL-MATCH emission path of
`transform_match.c`.

### Dependencies

Independent of P1/P2. Shares the OPTIONAL emission code with P4's OPTIONAL
varlen null case — coordinate if both touch the same join builder.

### Risk Considerations

Correlated-subquery rewrite can change result column provenance; verify WHERE
predicates that reference OPTIONAL-bound vars still null-propagate correctly.

## Status Updates

*To be added during implementation*
