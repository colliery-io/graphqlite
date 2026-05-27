---
id: p3-multi-rel-optional-match
level: task
title: "P3: Multi-rel OPTIONAL MATCH combined-EXISTS join ordering"
short_code: "GQLITE-T-0336"
created_at: 2026-05-27T02:49:35.864661+00:00
updated_at: 2026-05-27T13:49:25.624884+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


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

### 2026-05-27 — P3 triage + two OPTIONAL row-preservation fixes (+2)

P3 is several distinct OPTIONAL-MATCH bugs, not one. Triaged via SQL-dump +
execution harness:

**Landed (+2, zero regressions, 3691 → 3693):**
1. **OPTIONAL + varlen anchor preservation** (commit 6b45ff7) — for
   `OPTIONAL MATCH (a)-[:T*]->(c:Label)` with a deferred target, the target
   label was an INNER `node_labels` join (referencing the not-yet-joined
   target) and the varlen start/depth constraints landed on it. Now the label
   folds into the deferred target's LEFT JOIN ON as an EXISTS. → Match7 [15].
2. **OPTIONAL node labels inline into LEFT JOIN ON** (commit 1b2c218) —
   `generate_node_match` emitted every non-first OPTIONAL node's label as an
   INNER join, dropping the anchor row (e.g. a second `OPTIONAL MATCH
   (b:Missing)`). Now appended to the node's LEFT JOIN ON as EXISTS.
   → Aggregation5 [2]; unblocks chained-OPTIONAL row preservation generally.

**Remaining P3 (each a distinct, deeper bug — not yet done):**
- **MatchWhere6 [7]** (the canonical "ON clause references tables to its
  right"): multi-rel OPTIONAL emits the full-pattern combined-EXISTS onto
  *each* unbound endpoint's LEFT JOIN, so the first references aliases joined
  later. Needs a **derived-table** rewrite (LEFT JOIN a subquery that
  materializes the whole (x,y,z) pattern, keyed to the anchor) — sizable.
- **Match7 [4]** / **MatchWhere6 [5]**: bound rel `r` reused in a reverse
  OPTIONAL (`OPTIONAL MATCH (a1)<-[r]-(b2)`) → 0 rows (should preserve a1,r
  with b2 null).
- **Match7 [21]**: chained `OPTIONAL…OPTIONAL…WITH…OPTIONAL` now yields the
  right row count but binds the null vars to node id 0 instead of null — a
  WITH null-var binding issue.
- **Match7 [12]**: varlen optional row count (exp 4 got 3).
- **Match7 [27]**: correlated optional between optionally-matched entities.

### 2026-05-27 — Bound-rel reverse OPTIONAL: attempted, reverted (needs deeper change)

Root cause confirmed for Match7 [4] / MatchWhere6 [5]
(`MATCH (a1)-[r]->() WITH r,a1 LIMIT 1 OPTIONAL MATCH (a1)<-[r]-(b2)`): the
bound-rel handler (`transform_match.c`, `rel_is_bound` branch) emits the
endpoint constraints (`a1 = r.target AND b2 = r.source`) via `sql_where`. For
OPTIONAL these belong in the fresh target node's LEFT JOIN ON — as WHERE they
filter the anchor row away (a1=A ≠ r.target=B → 0 rows instead of 1 with b2
null).

Two attempts to redirect the constraint to the LEFT JOIN ON
(`sql_join_append_on`) **both regressed** other bound-rel-OPTIONAL shapes
(Match7 errors): `sql_join_append_on` appends to *the last emitted join*, which
is not reliably the target node's LEFT JOIN — other shapes (both endpoints
bound, target bound, or extra joins between) get a malformed `... ON ... AND
<cond>` tail or attach to the wrong join. Even a narrow guard
(`src in scope && tgt fresh`, dot-in-alias heuristic) still mis-targeted.

**Conclusion:** the constraint must be emitted *as part of* the target node's
LEFT JOIN ON at the moment that join is created (e.g. generate_node_match
returns/accepts a pending-ON, or the bound-rel handler emits the target node
join itself with the constraint inline), not appended post-hoc. That's a deeper
plumbing change than a `sql_where`→`append_on` swap. Reverted to preserve the
clean +9 state; left for a focused follow-up.