---
id: pattern-path-matching-completeness
level: initiative
title: "Pattern & Path Matching Completeness"
short_code: "GQLITE-I-0047"
created_at: 2026-05-27T02:26:42.947075+00:00
updated_at: 2026-05-27T02:26:42.947075+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: L
initiative_id: pattern-path-matching-completeness
---

# Pattern & Path Matching Completeness

## Context **[REQUIRED]**

The structural pattern-matching tail left after [[GQLITE-I-0044]] — the
**B1 (multi-rel OPTIONAL)** and **C3 (named/variable-length path)** items I-0044
delegated, plus related MATCH/MERGE emission bugs found during the push. These
live in `transform_match.c`, the variable-length recursive CTE
(`cypher_transform.c::generate_varlen_cte`), and the OPTIONAL-MATCH join
emission. I-0044 landed the contained pieces (single-varlen named-path
hydration via the interleaved `elem_ids` column; `relationships()` on varlen
paths; RETURN * synthetic-alias filtering). What remains needs the CTE
traversal and the outer endpoint/JOIN emission changed *together*.

**~15-18 scenarios** across Match4/5/6/7/9, Pattern1, MatchWhere6, Merge5.

## Goals & Non-Goals **[REQUIRED]**

**Goals:**
- **G1**: Undirected variable-length (`-[r*]-`) traverses both edge
  orientations without double-counting.
- **G2**: Multi-segment / mixed fixed+varlen path chains
  (`(a)-[*]->(b)-[r]->(c)`, two varlen segments in one path).
- **G3**: Multi-rel OPTIONAL MATCH join ordering (the "ON clause references
  tables to its right" error) — combined-EXISTS for full-pattern semantics.
- **G4**: Named-path forwarded through WITH; OPTIONAL varlen named path returns
  null (not `[]`) on no-match.
- **G5**: MATCH+MERGE combined-pattern re-match preserves inline node
  properties (undirected MERGE no longer double-counts result rows).
- **G6**: Zero-regression per the [[GQLITE-I-0044]] verification recipe.

**Non-Goals:**
- **NOT** the sql_builder migration (I-0040/I-0043).
- **NOT** new pattern syntax (label disjunction etc. → [[GQLITE-I-0045]]).
- **NOT** pattern *comprehension* lowering (B5 / task GQLITE-T-0332).

## Detailed Design **[REQUIRED]**

### Failing clusters & root causes (from I-0044 investigation)

**P1 — Undirected variable-length (Match9 [1]/[3], Match6 [14], Pattern1
[10]/[17]/[18]).** `generate_varlen_cte` only traverses `source_id→target_id`,
so `-[r*0..1]-` misses the reverse direction (row count short). An I-0044
attempt made the CTE bidirectional (UNION both orientations + recursive
`e.source_id=end OR e.target_id=end`) — it fixed the count but the *outer*
undirected endpoint matching then double-counted, regressing Delete4 [2]
(reverted). **The CTE bidirectional traversal and the outer endpoint-pattern
emission must be made consistent together** (dedupe one against the other).

**P2 — Multi-segment / mixed fixed+varlen chains (Match5 [25]/[26]/[28],
Match4 [4]/[5], Match6 [17]).** A path with a varlen rel AND a fixed rel, or
two varlen segments, returns empty / wrong. The single-varlen `elem_ids`
hydration (I-0044) is the building block; chains need the segments stitched.
Match4 [4]'s failure is actually in its *setup* (CREATE rels between
list-subscript nodes) — a write-path dependency.

**P3 — Multi-rel OPTIONAL MATCH (MatchWhere6 [5]/[7], Match7, Match4 [7]).**
`OPTIONAL MATCH (x)-[:E1]->(y)-[:E2]->(z) WHERE ...` errors with "ON clause
references tables to its right" — the LEFT JOIN ON references a not-yet-joined
table. Needs a combined-EXISTS shape so the full pattern is null-or-all
(I-0044 B1, residual of T-0320/T-0330).

**P4 — Named path through WITH / OPTIONAL (With1 [4], Match9 [9]).**
`MATCH p=... WITH p ...` errors "no such column: p" (path var not carried
through the WITH CTE); OPTIONAL varlen named path returns `[]` instead of null.

**P5 — MATCH+MERGE combined-pattern property drop (Merge5 [12]/[13]).**
handle_match_merge's RETURN re-matches a synthetic (MATCH ∪ MERGE) pattern;
inline node properties `{id:2}` are dropped from the generated SQL, so an
undirected MERGE matches all node pairs → doubled result rows. (Undirected
edge-find itself was fixed in I-0044; this is the re-match property drop.)

## Alternatives Considered **[REQUIRED]**

- **Post-hoc DISTINCT on undirected results** to mask double-counting.
  Rejected — wrong semantics (MATCH doesn't dedup; would hide real duplicate
  rows). The CTE/endpoint emission must be correct, not deduped.
- **Drop the combined-pattern re-match in MATCH+MERGE** and project from the
  MERGE var_map directly. Viable for P5 but the var_map currently binds only
  one row ("just take the first match"); revisit if the property-drop fix
  proves harder than expected.

## Implementation Plan **[REQUIRED]**

Decompose at pickup. Suggested tasks (roughly independent):
1. **T: Undirected variable-length** (P1) — CTE + outer endpoint emission
   together; guard against the Delete4 [2] double-count regression.
2. **T: Multi-segment / mixed varlen chains** (P2).
3. **T: Multi-rel OPTIONAL combined-EXISTS** (P3).
4. **T: Path variable through WITH / OPTIONAL null** (P4).
5. **T: MATCH+MERGE re-match property preservation** (P5).

## Acceptance Criteria

- [ ] Match4/5/6/7/9, Pattern1, MatchWhere6, Merge5 [12]/[13] failing
  scenarios move to pass.
- [ ] Zero TCK regressions (full pass-set diff each PR); unit 944/944 +
  functional clean. **Specifically re-check Delete4 [2]** (the undirected
  varlen canary).
- [ ] Each task logs its TCK delta here.

## Status Updates

### 2026-05-27 — Created

Filed from the I-0044 push (94.9%). Discovery phase. Contained pattern/path
wins already landed under I-0044; this owns the structural CTE + OPTIONAL +
combined-pattern remainder. Awaiting human review before decomposition.