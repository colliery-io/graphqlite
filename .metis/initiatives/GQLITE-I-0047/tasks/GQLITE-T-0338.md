---
id: p5-match-merge-re-match-inline
level: task
title: "P5: MATCH+MERGE re-match inline property preservation"
short_code: "GQLITE-T-0338"
created_at: 2026-05-27T02:49:41.681293+00:00
updated_at: 2026-05-27T02:49:41.681293+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P5: MATCH+MERGE re-match inline property preservation

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

In MATCH+MERGE, `handle_match_merge`'s RETURN re-matches a synthetic
(MATCH ∪ MERGE) combined pattern. Inline node properties like `{id:2}` are
**dropped** from the generated re-match SQL, so an undirected MERGE matches all
node pairs instead of the intended one → result rows are doubled. (The
undirected edge-find itself was already fixed in I-0044; this is specifically
the re-match property drop.)

Target scenarios: **Merge5 [12]/[13]**.

## Acceptance Criteria

- [ ] Merge5 [12]/[13] move to pass.
- [ ] MATCH+MERGE re-match carries inline node properties into the generated SQL.
- [ ] Undirected MERGE returns the correct (non-doubled) row count.
- [ ] Zero TCK regressions; unit 944/944; functional clean.
- [ ] TCK delta logged here and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

Locate where `handle_match_merge` builds the synthetic re-match pattern and
ensure inline `{...}` property maps from the MERGE pattern are propagated into
the re-match's node-pattern property filters. Preferred fix is preserving the
properties on the re-match pattern; the documented fallback (per the
initiative's Alternatives) is to project from the MERGE var_map directly,
but the var_map currently binds only one row — revisit only if the
property-propagation fix proves harder than expected.

### Dependencies

Independent of P1–P4. Smallest, most isolated cluster — good standalone PR
candidate.

### Risk Considerations

Re-match SQL is shared by directed MERGE too; confirm directed MERGE+RETURN
scenarios don't regress.

## Status Updates

*To be added during implementation*
