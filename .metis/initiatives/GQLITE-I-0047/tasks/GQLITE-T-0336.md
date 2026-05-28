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

### 2026-05-27 — Two more P3 wins (+2 → P3 total +5); bound-rel reverse re-confirmed deferred

3. **Variable-length relationship-uniqueness** (commit eaf19b4) — the varlen
   CTE's `visited` column tracked node ids and blocked node revisits, but
   openCypher requires *relationship* uniqueness (each edge once; nodes may
   repeat). `visited` now tracks edge ids; the cycle predicate forbids reusing
   an edge. So `(s)-[:REL]->(b)-[:LOOP]->(b)` (two edges, b revisited) is now a
   valid path. → Match7 [12]. Zero regressions (full diff).
4. **Null-guard WITH-projected node/edge RETURN projection** (commit cbf0026) —
   a node/edge bound via OPTIONAL and carried across WITH, when NULL, was
   projected as `json_object('id', <col>, …)` → a bogus `{id:null,…}` instead
   of SQL NULL. The `alias_is_id` (post-WITH) projection path lacked the
   `CASE WHEN id IS NULL THEN NULL` guard the direct-alias path already has.
   Added it to both node and edge post-WITH projections. → Match7 [21], [27].
   Zero regressions.

**Bound-rel reverse OPTIONAL — third attempt, reverted again.** Added a *safe*
guard (redirect to ON only when the joins tail is verifiably the target's
`LEFT JOIN … AS <tgt> ON 1=1`). It never fires for the failing case because
the relationship handler runs in the path loop *before* the target node's
LEFT JOIN is emitted — so at constraint-emission time there is no target join
to attach to. Confirms the fix must *defer* the bound-rel endpoint constraint
until the target node join is created (or have that join carry a pending ON).
Reverted; needs the deeper deferral plumbing.

**P3 net this initiative: +5** (Match7 [12]/[15]/[21]/[27], Aggregation5 [2]).
**Remaining (deeper refactors, deferred):** Match7 [4] / MatchWhere6 [5]
(bound-rel reverse — deferred-constraint plumbing); MatchWhere6 [7]
(multi-rel combined-EXISTS → derived-table rewrite).
### 2026-05-28 — Bound-rel reverse OPTIONAL FIXED (+1: Match7 [4])

Reattempted with the **deferred-flush** approach the prior analysis pointed to.
Added `ctx->pending_optional_on` (header struct field): the bound-rel handler
stashes its endpoint constraint there for OPTIONAL, and `transform_match_clause`
flushes it via `sql_join_append_on` after the path loop (so the fresh target
node's LEFT JOIN exists). Guarded to fire only when **exactly one** endpoint
is in outer scope and the other is fresh — both-fresh CROSS-multiplies the
early endpoint (caught Match7 [5] regressing before the guard).

**Verified: +1 (Match7 [4]), zero regressions** (full pass-set diff vs HEAD,
done by rebuilding HEAD in-place via stash/clean rebuild). Unit 944/944,
functional clean.

**MatchWhere6 [5] not fixed**: both endpoints fresh (a2, b2) + a WHERE
correlation `a1 = a2`; the guard correctly skips it (both-fresh case). It
needs the derived-table approach (same as MatchWhere6 [7]).

Note: adding `pending_optional_on` to the struct required `angreal dev clean`
(per the project's struct/enum change rule — incremental builds leave stale
objects with old layout, observed during dev as a catastrophic regression
recovered with a clean rebuild).

**T-0336 net so far: +6** (Match7 [4]/[12]/[15]/[21]/[27], Aggregation5 [2]).
Remaining P3: MatchWhere6 [5]/[7] (multi-rel/both-fresh combined-EXISTS →
derived-table rewrite).
