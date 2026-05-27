---
id: p5-match-merge-re-match-inline
level: task
title: "P5: MATCH+MERGE re-match inline property preservation"
short_code: "GQLITE-T-0338"
created_at: 2026-05-27T02:49:41.681293+00:00
updated_at: 2026-05-27T19:19:46.747917+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


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

### 2026-05-27 — Root-caused precisely; clean fix blocked by SET's dependency on the same AST mutation

Traced Merge5 [12]/[13] (`MATCH (a {id:2}),(b {id:1}) MERGE (a)-[r:KNOWS]-(b)
RETURN r` → 2 rows, expected 1) end-to-end:

- MERGE does **not** create a duplicate edge (rc=0, edge count stays 1) — so
  it's not a MERGE write bug.
- `handle_match_merge` builds a combined synth pattern (MATCH ∪ MERGE) and
  re-matches via `execute_match_return_query`. Dumping that synth's SQL shows
  the node **property filters `{id:2}`/`{id:1}` are gone**:
  `FROM nodes AS a JOIN nodes AS b ON 1=1 CROSS JOIN edges WHERE ((src=a AND
  tgt=b) OR (src=b AND tgt=a)) AND type='KNOWS'`. With a,b unconstrained, the
  undirected OR matches the single edge in *both* (a,b) assignments → 2 rows.
- **Root cause: AST mutation.** `generate_node_match` does
  `first_pair->key = NULL` (transform_match.c:1438/1536/1900) to mark the
  first inline property "handled" (it seeds the FROM table for selectivity).
  The MERGE execution transforms the MATCH first (nulling the keys), then the
  RETURN synth re-transforms the **same** AST nodes — now with null keys → the
  `{id:…}` filters vanish. The standalone equivalent (`MATCH … , (a)-[r]-(b)`)
  works because it's transformed once.

**Attempted fix (reverted):** stop nulling the key (the constraint pass then
re-emits it as a redundant-but-correct EXISTS). This fixed Merge5 [12]/[13]
(+2) but **regressed Set4 [2]/[3]/[4] and Set5 [4]** (−4, net −2): the SET path
(`MATCH (n:X {name:'A'}) SET n = {…}`) depends on the mutation for its own
transform reuse. Reverted.

**Conclusion:** the right fix is "don't mutate shared AST" — but it must be
done **together** with making the SET path independent of the mutation (e.g.
pass the FROM-consumed property index through the transform context instead of
nulling the AST, and update both the property-constraint pass and SET). That's
a coordinated transform refactor, deferred. Alternatively, deep-copy the
patterns in `handle_match_merge` before the synth re-match (localized, avoids
the SET entanglement) — a viable narrower follow-up.

Merge5 [12]/[13] left failing; analysis captured for the follow-up.