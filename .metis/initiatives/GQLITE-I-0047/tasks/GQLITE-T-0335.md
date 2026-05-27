---
id: p2-multi-segment-mixed-fixed
level: task
title: "P2: Multi-segment / mixed fixed+varlen path chains"
short_code: "GQLITE-T-0335"
created_at: 2026-05-27T02:49:32.608411+00:00
updated_at: 2026-05-27T02:49:32.608411+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P2: Multi-segment / mixed fixed+varlen path chains

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

Stitch multi-segment paths that combine a variable-length rel with a fixed
rel, or chain two variable-length segments in one path
(`(a)-[*]->(b)-[r]->(c)`, `(a)-[*]->(b)-[*]->(c)`). These currently return
empty or wrong rows because the single-varlen `elem_ids` hydration (I-0044)
handles only one segment.

Target scenarios: **Match5 [25]/[26]/[28], Match4 [4]/[5], Match6 [17]**.
Note **Match4 [4]'s** failure is actually in its *setup* (CREATE rels between
list-subscript nodes) — a write-path dependency that may need to split out.

## Acceptance Criteria

- [ ] Match5 [25]/[26]/[28], Match6 [17], Match4 [5] move to pass.
- [ ] Match4 [4] either passes or its write-path blocker is filed separately.
- [ ] Zero TCK regressions; unit 944/944; functional clean.
- [ ] TCK delta logged here and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

Each path segment generates its own CTE/join fragment. The stitch point is the
shared intermediate node variable (`b` above): the fixed-rel join's endpoint
must bind to the varlen CTE's terminal node, and the varlen `elem_ids` must be
concatenated across segments for named-path hydration. Likely changes in
`transform_match.c` segment iteration + the varlen CTE wiring in
`cypher_transform.c`.

### Dependencies

Builds on P1 ([[GQLITE-T-0334]]) for the corrected varlen CTE shape; best
sequenced after P1 lands. Match4 [4] has a CREATE write-path prerequisite.

### Risk Considerations

Two varlen segments multiply CTE cost; watch for combinatorial row blowup.
Run `angreal dev clean` after CTE-generator signature changes.

## Status Updates

*To be added during implementation*
