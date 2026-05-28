---
id: pattern-path-matching-completeness
level: initiative
title: "Pattern & Path Matching Completeness"
short_code: "GQLITE-I-0047"
created_at: 2026-05-27T02:26:42.947075+00:00
updated_at: 2026-05-27T02:51:33.491805+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/active"


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

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

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

### 2026-05-27 — Decomposed and P1 (T-0334) completed

Human-approved decomposition into [[GQLITE-T-0334]] (P1 undirected varlen),
[[GQLITE-T-0335]] (P2 multi-segment chains), [[GQLITE-T-0336]] (P3 multi-rel
OPTIONAL), [[GQLITE-T-0337]] (P4 path-through-WITH / OPTIONAL null),
[[GQLITE-T-0338]] (P5 MATCH+MERGE re-match). Work on branch
`i0047-pattern-path`.

**P1 (T-0334) COMPLETE — +6 TCK, zero regressions** (3684 → 3690):
- Undirected varlen now traverses both edge orientations in the top-level MATCH
  CTE (`generate_varlen_cte`) → Match9 [1]/[3].
- `count(*)` in MATCH+DELETE decoupled from the accumulated delete-count
  coincidence (`handle_match_delete` pre-captures the live MATCH) → Delete4 [1]
  fixed, Delete4 [2] canary held at 6 (the prior reverted attempt's 12 avoided).
- Varlen inside WHERE existential pattern predicates (`emit_exists_varlen_path`,
  endpoint labels honored) → Pattern1 [10]/[17]/[18].

Match6 [14] (multi-segment fixed+varlen named path) reassigned to P2; Match6
[17] (zero-length named path) to P4. Remaining: P2/P3/P4/P5 still todo.

### 2026-05-27 — P2 done (+1), P3 substantial progress (+5); session +12

- **P2 ([[GQLITE-T-0335]], done):** varlen relationship property predicate
  (Match4 [5]). Disproved the multi-segment premise (chains already match);
  filed MATCH+CREATE first-row-only write-path bug as [[GQLITE-T-0339]].
- **P3 ([[GQLITE-T-0336]], partial +5):** OPTIONAL row-preservation (Match7
  [15], Aggregation5 [2]); varlen relationship-uniqueness (Match7 [12]);
  null-guard for WITH-projected NULL node/edge in RETURN (Match7 [21], [27]).
  Match7 now 30/31. Remaining: bound-rel reverse OPTIONAL (Match7 [4],
  MatchWhere6 [5] — needs deferred-constraint plumbing) and MatchWhere6 [7]
  (multi-rel combined-EXISTS → derived-table rewrite).

**Session total: 3684 → 3696 (+12), zero regressions, unit 944/944, functional
clean.** Branch `i0047-pattern-path`. Still todo: P4 ([[GQLITE-T-0337]]),
P5 ([[GQLITE-T-0338]]), the two deferred P3 refactors, and [[GQLITE-T-0339]].

### 2026-05-27 — P5 ([[GQLITE-T-0338]]) root-caused; deferred (AST-mutation/SET entanglement)

Merge5 [12]/[13] (undirected MERGE re-match doubling) traced to an **AST
mutation gotcha**: `generate_node_match` nulls `first_pair->key` to mark an
inline property consumed, so the MATCH+MERGE RETURN re-match (which
re-transforms the same AST) loses the `{id:…}` filters → undirected double
match. The "stop mutating" fix works (+2) but **regresses Set4/Set5** (the SET
path depends on the same mutation). Proper fix = a coordinated transform
refactor (non-destructive consumed-marker, or deep-copy patterns in
`handle_match_merge`). Deferred with full analysis in the task.

P3/P5 deeper refactors now clearly enumerated as follow-ups; no further TCK
movement this round (net stays +12, zero regressions).

### 2026-05-27 — P4 ([[GQLITE-T-0337]]) partial: path-through-WITH (+1, session +13)

`MATCH p=… WITH p RETURN p` (With1 [4]) now forwards the path across WITH:
WITH emits the path-hydration SQL and re-registers `p` as a path var on the CTE
column; RETURN/executor hydrate it. Tightly scoped (path-through-WITH only),
**zero regressions** (rigorous pass-set diff), unit 944/944, functional clean.
Match9 [9] (OPTIONAL varlen named path → null) remains. **Session total: +13
(3684 → 3697).**

### 2026-05-27 — P4 ([[GQLITE-T-0337]]) COMPLETE: +2 (session +14)

Match9 [9] also fixed: the OPTIONAL varlen relationship list returned `[]`
instead of NULL on a no-match (`json_group_array` over `json_each(NULL)` yields
`'[]'`); wrapped in a `CASE WHEN elem_ids IS NULL` guard. P4 done (With1 [4] +
Match9 [9]), zero regressions. **Session total: +14 (3684 → 3698).** Remaining
deferred refactors: P3 bound-rel-reverse, P3 multi-rel combined-EXISTS, P5
(AST-mutation/SET), and write-path [[GQLITE-T-0339]].

### 2026-05-28 — Branch pushed; P3 bound-rel reverse FIXED on top (+1, session +15)

Branch `i0047-pattern-path` pushed; **PR #76** opened. The earlier +14 push is
fully green on CI (build-and-test, coverage gate, all platforms × Python
matrix, Rust, Windows, tck-conformance).

On top, the bound-rel reverse OPTIONAL (Match7 [4]) landed via a deferred-flush
mechanism: a new `ctx->pending_optional_on` stash + a flush onto the last LEFT
JOIN's ON after the path loop, guarded to fire only when exactly one endpoint
is in outer scope. **+1, zero regressions** (full pass-set diff vs HEAD). 3698 →
**3699**.

**Session total: +15 (3684 → 3699).** Remaining deferred: MatchWhere6 [5]/[7]
(both-endpoints-fresh / multi-rel combined-EXISTS → derived-table rewrite); P5;
T-0339.

### 2026-05-27 — P2 (T-0335) completed; premise corrected

Investigation overturned P2's premise: **multi-segment / mixed fixed+varlen
chains already match correctly** (verified on clean graphs). The Match5
[25]/[26]/[28]/[29] failures are a **MATCH+CREATE write-path bug** (only the
first matched row's CREATE runs) — filed as backlog [[GQLITE-T-0339]], out of
this initiative's pattern-matching scope; that cluster + Match4 [4] are blocked
on it.

The genuine in-scope P2 fix landed: `generate_varlen_cte` now applies inline
**relationship property predicates** (`[:T* {k:v}]`) to every edge in the path.
**+1** (Match4 [5]), zero regressions, 3690 → **3691**. Match4 [7] (bound rel in
multi-varlen path) and [8] (`-[rs*]->` bound rel-list) deferred as niche varlen
features. Session running total for I-0047: **+7**.