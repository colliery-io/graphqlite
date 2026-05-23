---
id: b1-multi-rel-optional-match
level: task
title: "B1: Multi-rel OPTIONAL MATCH combined-EXISTS for full-pattern semantics"
short_code: "GQLITE-T-0330"
created_at: 2026-05-23T18:05:33.527881+00:00
updated_at: 2026-05-23T18:17:21.592855+00:00
parent: GQLITE-I-0044
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0044
---

# B1: Multi-rel OPTIONAL MATCH combined-EXISTS

## Parent Initiative

[[GQLITE-I-0044]] — Phase B #1.

## Objective

For OPTIONAL MATCH paths with multiple rel patterns, the unbound
intermediate endpoint should be bound only when the **full path**
matches, not when just the first rel matches.

## Background

T-0320 partial work landed in v0.5.0 with the EXISTS-based collapse
shortcut for single-rel OPTIONAL paths. For multi-rel paths like
`(a)-->(b)-->(c)`, the current emission is:

  `LEFT JOIN nodes AS b ON EXISTS (some edges WHERE a→b)`
  `LEFT JOIN edges AS e2 ON e2.src = b.id AND e2.tgt = c.id`

`b` gets populated whenever ANY a→b edge exists, even if the
subsequent b→c rel doesn't match. The LEFT JOIN edges then
yields edge=NULL but `b` stays populated, giving rows with
partial-match bindings — wrong per Cypher OPTIONAL semantics
which says: bind ALL inner vars OR none.

## Repro

```cypher
CREATE (s:Single), (a:A), (b:B), (c:C);
CREATE (s)-[:REL]->(a), (s)-[:REL]->(b), (a)-[:REL]->(c), (b)-[:LOOP]->(b);

MATCH (a:A), (c:C)
OPTIONAL MATCH (a)-->(b)-->(c)
RETURN b;
```

**Expected:** 1 row with `b = null`. The full path A→b→C doesn't
exist (A→c exists but c is the target, not an intermediate;
A has no other outgoing edge).
**Actual:** 1 row with `b = (C)`. The first rel A→c matches with
b=c, but the second rel c→c doesn't exist; outer row preserved
but b stays populated.

## Affected scenarios

- Match7 [8] Longer pattern with bound nodes without matches.
- Match7 [9] Longer pattern with bound nodes (expected 1 got 2).
- Match7 [12] Variable length optional relationships (expected 4 got 3 — partly).
- Match7 [27] Handling optional matches between optionally matched
  entities.

Estimated **+5-8 TCK**.

## Implementation plan

### Approach: combined-EXISTS over the full pattern

Replace the per-rel EXISTS emissions with ONE combined EXISTS
that encodes the entire OPTIONAL pattern as a single existence
check:

```sql
LEFT JOIN nodes AS b ON EXISTS (
  SELECT 1 FROM edges e1, edges e2
  WHERE e1.source_id = a.id AND e1.target_id = b.id
    AND e2.source_id = b.id AND e2.target_id = c.id
    AND e1.id <> e2.id
)
```

For more rels, the EXISTS subquery chains more `edges` joins.

The intermediate node `b` is populated only when SOME assignment
of edge ids satisfies the FULL pattern.

### Steps

1. Detect multi-rel OPTIONAL MATCH paths where the intermediate
   nodes are anonymous OR unbound, no rel variables are used,
   and no named-path captures the pattern. Restrict to this
   "no projection of intermediate edges/nodes" shape for now —
   it's the common case.
2. In `transform_match.c::generate_relationship_match`, when the
   current rel is part of such a path, defer its emission. After
   processing all rels in the path, emit ONE combined EXISTS as
   the LEFT JOIN's ON for the unbound intermediate endpoint(s).
3. Add the relationship-uniqueness pairwise inequality
   (`e1.id <> e2.id`) inside the EXISTS subquery so spec-correct
   path uniqueness holds.

### Considerations

- **Rel variables / named paths**: skip the combined EXISTS
  shortcut; fall back to the existing per-rel emission. Rel
  variables need to be EXPOSED for projection / WHERE.
- **Varlen rels** in the chain: out of scope; varlen has its own
  CTE machinery. If the path mixes non-varlen + varlen, conservatively
  fall back.
- **Performance**: the combined EXISTS is O(|edges|^n) for n
  rels but SQLite's optimizer can usually push the constraints
  through indexes.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Match7 [8]/[9]/[12]/[27] pass (or at least Match7 [8] + [9];
  [12] is varlen and may need additional work).
- [ ] Single-rel OPTIONAL MATCH still works (no regression).
- [ ] Named-path / rel-variable cases fall through to existing
  emission unchanged.
- [ ] `angreal test unit && angreal test functional` clean.
- [ ] Zero regressions on currently-passing OPTIONAL MATCH
  scenarios.

## Affected files

- `src/backend/transform/transform_match.c` (primary).

## Effort

M — touches the OPTIONAL emission infrastructure that v0.5.0's
T-0320 already restructured. Risk of regression on T-0320's
wins, so guard the new path tightly.

## Status Updates

### 2026-05-23 — Completed

Implemented combined-EXISTS collapse for eligible multi-rel
OPTIONAL paths in `transform_match.c::transform_match_pattern`:

**Pre-loop eligibility check** (new `path_combined_exists` flag):
- OPTIONAL MATCH.
- Path has at least 5 elements (2+ rels).
- No varlen rels.
- No user-visible rel variables.
- No named path captures the pattern.
- Every INTERMEDIATE node has a user variable (anonymous
  intermediates fall back to per-rel emission — they can't be
  referenced as `<alias>.id` inside the combined EXISTS).

**When eligible**, before the path-element loop:
- Pre-register all node aliases (assigning gen aliases to new vars).
- Build one EXISTS clause referencing `edges _ce0, _ce1, ...`
  with per-rel `_cei.source_id = <src>.id AND _cei.target_id = <tgt>.id`
  conditions, type filters, and pairwise `_cei.id <> _cej.id`
  uniqueness.
- For each unbound intermediate node, emit one
  `LEFT JOIN nodes AS X ON <combined EXISTS>`.
- Skip per-rel emission and per-node emission for all elements
  in the path.

**Bug found and fixed**: `get_node_id_ref` returns a static
buffer. Calling it twice in quick succession (for src and tgt
IDs) caused the second call to overwrite the first. Fixed by
copying each result into a stack-local before the next call.

**TCK: 3566 → 3568 (+2):**
- Match7 [8] Longer pattern with bound nodes without matches.
- Match7 [9] Longer pattern with bound nodes.

Match7 [27] still fails (chained OPTIONAL MATCH between two
already-optional entities — separate root cause).

Match7 [12] (varlen optional) still fails — varlen has its own
emission path that wasn't restructured by this change. Tracked
in the original initiative status.

944/944 unit, functional clean. 0 regressions (one initial
regression for Match7 [7] which uses anonymous intermediates
was fixed by adding the `intermediates_named` eligibility check).