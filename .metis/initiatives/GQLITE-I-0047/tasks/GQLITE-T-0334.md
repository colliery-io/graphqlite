---
id: p1-undirected-variable-length
level: task
title: "P1: Undirected variable-length paths (CTE + outer endpoint)"
short_code: "GQLITE-T-0334"
created_at: 2026-05-27T02:49:29.588563+00:00
updated_at: 2026-05-27T12:26:59.280587+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P1: Undirected variable-length paths (CTE + outer endpoint)

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

Make undirected variable-length patterns (`-[r*]-`) traverse **both** edge
orientations without double-counting result rows. The variable-length
recursive CTE (`cypher_transform.c::generate_varlen_cte`) currently only walks
`source_id → target_id`, so `(a)-[*0..1]-(b)` misses reverse hops and returns
too few rows. A prior I-0044 attempt made the CTE bidirectional but the *outer*
undirected endpoint matching then double-counted, regressing **Delete4 [2]**
(reverted). The CTE traversal and the outer endpoint-pattern emission must be
changed **together** and deduped against each other.

Target scenarios: **Match9 [1]/[3], Match6 [14], Pattern1 [10]/[17]/[18]**
(~6 scenarios).

## Acceptance Criteria

- [x] Match9 [1]/[3] and Pattern1 [10]/[17]/[18] move to pass. (Match6 [14]
  reassigned to P2/[[GQLITE-T-0335]] — multi-segment named path, distinct.)
- [x] **Delete4 [2] stays passing** (the undirected-varlen double-count canary);
  Delete4 [1] also fixed as a bonus.
- [x] Zero TCK regressions (full pass-set diff); unit 944/944; functional clean.
- [~] Functional regression coverage: the functional harness is execution-only
  (no output assertions), so the undirected `-[*]-` round-trip is recorded in
  `docs/testing/semantic-coverage-matrix.md` and gated by the TCK suite instead.
- [x] TCK delta logged in Status Updates and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

1. Reproduce: isolate the failing scenarios with
   `angreal test tck --filter Match9` (and Match6/Pattern1) to capture the
   exact row-count deltas before any change.
2. Make `generate_varlen_cte` traverse both orientations for undirected rels
   — UNION the `source_id→target_id` and `target_id→source_id` seed/step, and
   in the recursive step match on `e.source_id = frontier OR e.target_id =
   frontier`, carrying the "other endpoint" forward.
3. Simultaneously fix the **outer** endpoint-pattern emission so the
   undirected endpoints are matched once, not once per orientation — dedupe
   the CTE rows against the outer pattern (likely a `DISTINCT` keyed on the
   path's elem_ids, or an orientation-canonicalization in the CTE so each
   undirected walk is emitted in one canonical direction).
4. Only apply bidirectional logic when the rel pattern is undirected; leave
   directed varlen untouched.

### Dependencies

Builds on the single-varlen `elem_ids` hydration column landed in I-0044.
Independent of P2–P5.

### Risk Considerations

The prior reverted attempt proves the CTE and outer emission are coupled: a
correct CTE with a naive outer match double-counts. Verify Delete4 [2] on
*every* iteration, not just at the end. Run `angreal dev clean` if any
function signature in the CTE generator changes.

## Status Updates

### 2026-05-27 — Root cause confirmed; P1 is entangled with a documented count(*) hack

Reproduced and traced via a throwaway SQL-dump + execution harness (built
against `*.cov.o`, since the TCK harness doesn't surface generated SQL).

**Confirmed root cause (CTE side):** `generate_varlen_cte`
(`cypher_transform.c:1173`) only handles `left_arrow` reversal; **undirected**
(`!left_arrow && !right_arrow`) falls through to the default
`source_id → target_id` single orientation. So `MATCH (a)-[*]-(b)` over
`(1)-[:R]->(2)-[:R]->(3)` yields only the 3 directed rows `(1,2),(1,3),(2,3)`;
real Cypher wants 6 (both orientations). Match9 [1]'s missing `(b,a)` row is the
same defect. The outer endpoint binding is strictly directional
(`cte.start_id = a.id AND cte.end_id = b.id`, **no** OR) — so making the CTE
bidirectional and leaving the outer binding directional is the correct single
fix for the *match* count (verified by hand against Match9 [1]/[3]).

**The Delete4 [2] entanglement (the canary):**
`MATCH (a)-[*]-(b) DETACH DELETE a,b RETURN count(*)` expects **6** and
**passes today — by coincidence, not correctness.** With the directed CTE the
MATCH yields 3 rows. `handle_match_delete` (`query_dispatch.c:797`) routes
`count(*)` through `synthesize_delete_return`, which returns the *accumulated
delete count*: `execute_match_delete_query` increments `deleted_nodes` once per
(row × delete-var) with **no dedup** → 3 rows × 2 endpoints = 6. The dispatch
comment (lines 810-818) explicitly documents this: the delete count
"by historical coincidence" matches expected aggregates "for shapes our MATCH
undercounts (e.g. undirected varlen). Pre-MATCHing those would regress them."

**Consequence:** fixing the CTE → 6 match rows → `synthesize_delete_return`
would report 6 × 2 = **12**, regressing Delete4 [2]. This is exactly the prior
reverted attempt's failure mode.

**Therefore P1 requires TWO coordinated changes:**
1. Bidirectional undirected varlen CTE (base case UNIONs both orientations;
   recursive step extends on `source_id = end OR target_id = end` carrying the
   other endpoint forward; cycle-prevention + elem_ids use the chosen endpoint).
2. Decouple `count(*)`/literal RETURN in MATCH+DELETE from the delete-count
   coincidence — pre-MATCH the RETURN (incl. count) against the now-correct
   match instead of `synthesize_delete_return`. **Risk:** the hack also covers
   *other* shapes our MATCH still undercounts; removing it could regress those
   unless their MATCH is also correct. Needs the full-suite diff to bound.

**Scope decision pending with human** (see initiative): full two-part P1 fix
(A: CTE + B: decouple delete-count), vs. land A behind a narrower guard, vs.
reorder to a lower-risk cluster (P5/P3) first.

### 2026-05-27 — Parts A + B implemented; +3, zero regressions

Human chose the full two-part fix. Both landed:

- **Part A** (`cypher_transform.c` `generate_varlen_cte`): added an
  `undirected` branch. Base case UNIONs the reverse orientation; recursive step
  joins `e.source_id = cte.end_id OR e.target_id = cte.end_id` and advances to
  the edge's *other* endpoint via a `CASE` expr (used consistently for end_id,
  path_ids, visited, elem_ids, and node-based cycle prevention). Directed varlen
  is untouched. Also refactored the rel-type predicate into one reusable
  `tpred` buffer (was inlined 2×; now 3 use sites).
- **Part B** (`query_dispatch.c` `handle_match_delete`): `count()` now triggers
  the pre-delete MATCH pre-capture instead of `synthesize_delete_return`, so
  `count(*)` reflects matched rows, not the (un-deduped) delete count. Literal
  RETURNs still use synthesize. Comment updated to document the rationale.

**Verification (full TCK pass-set diff vs. pre-change baseline of 3684):**
- Newly passing: Match9 [1], Match9 [3], **Delete4 [1]** (single-hop undirected
  `count(*)`, a bonus from Part B).
- **Regressed: none.** Delete4 [2] (the canary) stays correct (6, not 12).
- Total: 3684 → **3687 (+3)**. Unit 944/944, functional clean.

**Not fixed (distinct generators, deferred):**
- Pattern1 [10]/[17]/[18] — undirected varlen inside WHERE `EXISTS`-pattern
  predicates (`MATCH (n) WHERE (n)-[:REL1*2]-()`). Different code path than the
  top-level MATCH CTE; needs the pattern-predicate emitter taught the same
  bidirectional logic. Tracked here as P1 residual.
- Match6 [14] (multi-segment undirected fixed+varlen named path → P2 /
  [[GQLITE-T-0335]]) and Match6 [17] (zero-length named path hydration → P4 /
  [[GQLITE-T-0337]]).

Committing the +3 verified increment; P1 residual (EXISTS-predicate varlen)
continues next on the same branch.

### 2026-05-27 — P1 residual scoped: varlen in WHERE EXISTS-pattern predicates

Traced Pattern1 [10]/[17]/[18]. The WHERE pattern-predicate emitter
(`transform_expr_predicate.c:51+`, the `AST_NODE_PATH` EXISTS branch) only
handles **fixed single-hop** relationships: it emits one `edges AS eN` table
per rel and joins `eN.source_id/target_id` to the node aliases, with **no
handling of `rel->varlen`**. So `MATCH (n) WHERE (n)-[:REL1*2]-()` collapses the
`*2` to a single REL1 edge match → returns A,B,D instead of B,D.

Fixing this means emitting (or referencing) a recursive varlen CTE *inside* the
EXISTS subquery and binding the outer node to its `start_id`/`end_id` — a
distinct, non-trivial sub-feature separate from the top-level MATCH CTE just
landed. Scoped as the next P1 sub-task (this emitter is also where undirected
fixed predicates already work, so the bidirectional logic can be shared). Not
attempted in this increment.

### 2026-05-27 — EXISTS-predicate varlen landed; +3 more, zero regressions

Added `emit_exists_varlen_path` (`transform_expr_predicate.c`): a guarded branch
in the `AST_NODE_PATH` EXISTS emitter that handles the `[node, varlen-rel, node]`
shape with an inline correlated recursive CTE
(`EXISTS (WITH RECURSIVE _epv_N(start_id, end_id, depth, visited) AS (…)
SELECT 1 … WHERE start_id = <left>.id [AND end_id = <right>.id] AND depth
BETWEEN min AND max)`). Same bidirectional traversal as the MATCH CTE
(directed honors arrows; undirected walks both orientations; node-based cycle
prevention). The left endpoint must be outer-bound to correlate; the right is
bound when it's an outer var, else free (the `(n)-[*]-()` anon case).

Endpoint **label** constraints are honored against the CTE's start/end ids
(`(a)-[:T*]->(b:MissingLabel)`); inline endpoint **properties** bail to the
legacy emitter (unchanged pre-existing behavior, no new regression).

First pass regressed MatchWhere4 [2] / WithWhere4 [2] ("disjunctive multi-part
predicates including patterns") because the helper ignored the `:MissingLabel`
endpoint label → the EXISTS wrongly matched. Adding endpoint-label handling
fixed both back to passing.

**Verification:** full TCK pass-set diff vs. the P1-core baseline →
newly passing Pattern1 [10]/[17]/[18], **zero regressions**. 3687 → **3690**.
Unit 944/944, functional clean.

**P1 status:** undirected varlen now correct in both the top-level MATCH CTE
and WHERE EXISTS-pattern predicates. Session total for T-0334: **+6**
(Match9 [1]/[3], Delete4 [1], Pattern1 [10]/[17]/[18]). Remaining acceptance
items (Match6 [14] multi-segment, Match6 [17] zero-length named path) belong to
P2 ([[GQLITE-T-0335]]) / P4 ([[GQLITE-T-0337]]).