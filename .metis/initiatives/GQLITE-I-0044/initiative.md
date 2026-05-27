---
id: tck-conformance-push-ii-91-5-95
level: initiative
title: "TCK Conformance Push II — 91.5% → 95%+ via clustered semantic fixes"
short_code: "GQLITE-I-0044"
created_at: 2026-05-23T04:07:55.417944+00:00
updated_at: 2026-05-23T10:53:00.058386+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/decompose"


exit_criteria_met: false
estimated_complexity: L
initiative_id: tck-conformance-push-ii-91-5-95
---

# TCK Conformance Push II

## Context

v0.5.0 (2026-05-22) shipped the first major TCK conformance push,
moving from 88% → **91.5% executable** (3486 → 3549, +63 scenarios).
That release closed out the SQL-emission alias collisions, the
OPTIONAL MATCH structural shape problems, and the CALL test.*
procedure surface; it also unblocked Windows for the first time.

**327 non-pass scenarios remain.** They cluster by feature area
rather than root cause — a few targeted fixes can each unlock 5-15
scenarios in their cluster, and the remainder need substantial
semantic implementation work that's worth its own decomposition.

This initiative organizes the next push: target **95% executable
pass rate** (≈3686 / 3880, a further +137 scenarios) via three
phases of progressively heavier work.

## Goals

- **G1**: Land **Phase A** quick wins (~15 TCK) in one or two PRs
  within the first week. High-confidence, low-risk fixes.
- **G2**: Land **Phase B** medium semantic improvements (~20-40
  TCK) across 3-5 PRs over 2-3 weeks. Each PR is a coherent
  semantic area.
- **G3**: Begin **Phase C** strategic work (I-0043 transform_expression
  migration + Temporal duration + Quantifier semantics) — this is
  multi-week per area and may overlap into a v0.6 → v0.7 sequence.
- **G4**: Each Phase-A/B PR reaches main with TCK delta > 0 and
  zero regressions; unit and functional suites clean.
- **G5**: Document remaining gaps post-push so the next initiative
  starts with a clear picture.

## Non-Goals

- **NOT** chasing 100% TCK conformance. The Temporal cluster (~50
  scenarios) and full Cypher spec compliance (calendar week math,
  duration arithmetic, full type coercion lattice) are explicit
  long-term goals tracked separately.
- **NOT** rewriting the executor architecture; the I-0042 follow-on
  work and I-0043 sql_builder migration are their own initiatives
  whose contribution is captured here via TCK delta but whose plan
  lives there.
- **NOT** adding new SQL backends or graph algorithm coverage.

## Phase A — Quick wins (Week 1, target +15 TCK)

Small, high-confidence fixes. Each lives in a single file, often a
single function. Low risk of regression.

### A1. Graph3 `labels()` empty-list (≈5 TCK)

`labels(n)` on a node with no labels returns `null` instead of `[]`.
Spec says empty list. Affects Graph3 [1]/[2]/[3]/[4]/[6] and likely
a few sibling scenarios.

- **Locus**: `transform_func_entity.c::transform_labels_function` (or
  wherever labels()'s SQL emission lives).
- **Fix shape**: wrap the `json_group_array(label)` with
  `COALESCE(..., json('[]'))` if any such guard is missing, OR
  ensure the subquery returns `[]` when the node has no labels
  instead of NULL.

### A2. Match4 [2]/[4] reserved-keyword rel types (≈4 TCK)

`MATCH (a)-[:CONTAINS]->(b)` and similar with STARTS/ENDS rejected
at parse time. Grammar accepts only IDENTIFIER and BQIDENT for
rel types; `non_reserved_kw` (which already exists for labels)
should also be allowed.

- **Locus**: `cypher_gram.y` — the 6+ rel-pattern productions
  (directed, incoming, undirected; each with non-varlen and varlen
  variants). May need a new `rel_type_name` nonterminal to keep
  productions tidy.
- **Risk**: Bison conflicts. Already counted `%expect 14` /
  `%expect-rr 3`; verify post-change.

### A3. Match6 undirected double-count (≈3-5 TCK)

`MATCH (a)-->(b)<--(a)` and similar undirected/bidirectional
patterns return 2× expected rows because each undirected edge
matches forward + backward. Match6 [8]/[10]/[12]/[13] family.

- **Locus**: `transform_match.c` rel handler — undirected
  emission. Currently uses OR of both directions in WHERE;
  needs a uniqueness constraint or DISTINCT-style dedup.

### A4. Embedded write-path WHERE filter (≈3 TCK)

`UNWIND [1,2,3,4,5] AS x CREATE (n:N {num:x}) WITH n WHERE n.num
% 2 = 0 RETURN n.num` returns 5 rows instead of 2 — the dispatcher
handler `handle_unwind_create_return` ignores the intermediate
WITH-WHERE. Affects Create6 [5]/[7]/[12]/[14].

- **Locus**: `query_dispatch.c::handle_unwind_create_return` —
  after CREATE produces `n_maps`, filter by the WITH-WHERE
  predicate before projection.
- **Dependency**: needs an externally-callable predicate
  evaluator. May refactor `evaluate_ast_with_context` from
  static-in-executor_set.c to public surface.

### A5. BQIDENT in LIMIT/SKIP / property-key sites (≈1-2 TCK)

Sweep any remaining grammar productions that take IDENTIFIER but
should accept BQIDENT, mirroring the v0.5.0 RETURN AS / UNWIND AS
work.

## Phase B — Medium semantic improvements (Weeks 2-4, target +25-40 TCK)

Each is its own coherent semantic area; each gets its own PR.

### B1. Multi-rel OPTIONAL combined-EXISTS (≈5-8 TCK)

T-0320 residual. Patterns like `(a)-->(b)-->(c)` in OPTIONAL MATCH
need a combined EXISTS over both rels so b is null when the FULL
pattern doesn't match, not when just the first rel matches.

- **Locus**: `transform_match.c` defer analysis + rel emission.
- **Affected**: Match7 [8]/[9]/[12]/[27], Match4 [7],
  Match9 [9].

### B2. Cross-type comparison null semantics (≈8-10 TCK)

`1 < 'a'` should yield `null`, not an error and not a coercion.
Affects Comparison2 [3]/[4]/[5]/[6] and possibly some
WithOrderBy ordering rules.

- **Locus**: `transform_expr_ops.c::transform_binary_op` —
  detect type-mismatched operands; wrap result in a runtime
  check that returns NULL when types are incomparable.
- **Helper**: probably a new `_gql_cmp` UDF that handles the
  full Cypher comparability lattice.

### B3. Quantifier null-aware semantics (≈10-15 TCK)

`any(x IN list WHERE pred)`, `all`, `none`, `single` — current
impl uses simple SQL EXISTS/NOT EXISTS over `json_each`. Cypher
requires three-valued logic: a single null predicate result
poisons all/any/none differently than false.

- **Locus**: `transform_func_list.c` or
  `transform_expr_predicate.c` (wherever quantifiers lower).
- **Affected**: Quantifier1-12 family (~50 scenarios).
- **Note**: this MAY be partially blocked on B2 since
  quantifier predicates use comparison.

### B4. Write-path WITH/WHERE/SKIP/LIMIT thread-through (≈3-5 TCK)

Bigger version of A4. The dispatcher handlers for write paths
(CREATE/MERGE/SET) need consistent WITH-WHERE-SKIP-LIMIT-ORDER
handling. WithSkipLimit1 [1]/[2], Unwind1 [12].

- **Locus**: `query_dispatch.c` — multiple `handle_*` handlers.
- **Likely produces a shared helper** (e.g.
  `apply_with_filters_to_var_maps`) that the dispatchers call.

### B5. Pattern comprehension parser + lowering (≈8-10 TCK)

`[(a)-->(b) | b.x]` parses as a list comprehension over a pattern.
Pattern2 [1]-[11]. New AST node + lowering to a correlated
subquery.

- **Locus**: `cypher_gram.y` (grammar), `cypher_ast.c` (AST type),
  `transform_func_list.c` / new file for lowering.

## Phase C — Strategic / multi-week (out of this initiative's primary scope)

These are tracked here for completeness but their plans live
elsewhere:

### C1. I-0043 transform_expression migration

Eliminates the `ctx->sql_buffer` scratchpad, unlocks the
`append_sql` deprecation. Plan in [[GQLITE-I-0043]]. Per-file
migration over 2-4 weeks. Indirect TCK benefit (cleaner code
unlocks future fixes).

### C2. Temporal duration type + arithmetic

`Temporal8` (~12), `Temporal3` (~10), `Temporal10` (~10) and
sibling failures all need a proper Duration data type +
operator support (add/subtract/multiply/divide). Substantial
new type system work. File as its own initiative when ready.

### C3. Named-path semantics (variable-length, undirected fixed)

Match6 [14]-[20] family. Path projection from varlen rels;
named-path-element ordering; relationship-uniqueness inside
varlen paths. Material work; should follow A3 (undirected
double-count) since they share emission code.

### C4. T-0205 Windows timestamp()

Pre-existing Windows-only `timestamp()` returns 0 in some
MATCH+SET / MERGE paths. Tracked as backlog T-0205.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

Per phase:

- **Phase A**: 4-5 PRs merged, each TCK delta > 0, total delta
  ≥ +15. All shipped in v0.5.x patch series.
- **Phase B**: 4-5 PRs merged, each in its own semantic area,
  total delta ≥ +25 (cumulative with Phase A ≥ +40). Likely
  shipped across v0.5.x and v0.6.0.
- **Phase C**: explicitly OUT of acceptance; tracked here for
  awareness only.
- **Overall**: TCK pass rate ≥ 95% executable when this
  initiative completes.

## Operational notes

- **PR cadence**: per the [[feedback-pr-continuation]] preference,
  related work goes on the same open PR branch — don't spin a new
  branch per task. Each Phase-A/B PR may bundle multiple sub-tasks.
- **Verification**: per [[feedback-tck-runs]] and
  [[feedback-test-via-harness]], every change runs through
  `angreal test functional` + `angreal test tck`. No ad-hoc
  chained-CREATE cypher() calls.
- **Decomposition**: each Phase-A item gets one Metis task when
  picked up (A1-A5 = up to 5 tasks). Phase B items each become
  one initiative-child task at decompose time. Phase C tasks
  stay in their parent initiatives.

## Decomposition queue (filed as tasks once decomposed)

Phase A targets:
- A1: Graph3 labels() empty-list
- A2: Reserved-keyword rel types (CONTAINS/STARTS/ENDS)
- A3: Match6 undirected double-count
- A4: Embedded write-path WHERE filter
- A5: BQIDENT sweep

Phase B targets:
- B1: Multi-rel OPTIONAL combined-EXISTS
- B2: Cross-type comparison null semantics
- B3: Quantifier null-aware semantics
- B4: Write-path WITH/WHERE/SKIP/LIMIT
- B5: Pattern comprehension

Phase C is delegated to existing/future initiatives.

## Status Updates

### 2026-05-23 — Created

Filed at v0.5.0 baseline (3549 / 3880 = 91.5%). Discovery phase
— awaiting human review before decomposing into tasks.

### 2026-05-26 — Session: +82 TCK (3590 → 3672, 94.6%), 26 commits
(+ Precedence2 [4]/[5]: unary minus on non-literal operands materialized as
0 - expr — `-(3^2)` was dropping the negation. Precedence2 26/26.
+ Graph5 [2]/[5]: label/type predicate 3VL — r:T checks edge type, null→null,
AST_NODE_LABEL_EXPR added to ast_yields_boolean. Grammar gap remains: conjunctive
label expression :A&B, Graph5 [4].)

### 2026-05-26 — Session: +77 TCK (3590 → 3667), 22 commits

All on PR branch `i0044-phase-b2-cross-type-cmp`, each verified
zero-regression via full-TCK pass-set diff + unit (944/944) +
functional. Now **3667 / 3880 = 94.5% executable**.
(+ Path2 [3]: relationships()/nodes() on null OPTIONAL path → null.
+ Path2 [1]/[2]: relationships() on a variable-length path — build the
rel-object list from the CTE's elem_ids (edges at odd positions). Path2 3/3.
Also fixed the RETURN <varlen rel var> projection to use elem_ids not the
buggy node-only path_ids. Reverted an undirected-varlen CTE attempt that
regressed Delete4 [2].)
(+ Graph6 [4]/[8]: `(list[1]).prop` via `_gql_dyn_prop`. + With1 [1]/[2] +
WithSkipLimit2 [3]: RETURN * excludes synthetic `_gql_default_alias_` /
`__unnamed_rel_` aliases — only user-named variables.)

Cluster fixes this session (commit → area → delta):
- Unwind1 [12] — UNWIND over `collect(node)` → trailing MATCH binds
  the unwound entity (collect hydrates to JSON objects; bind alias to
  `json_extract(value,'$.id')`). **B4.** +1
- WithOrderBy1 [32] — WITH property projection missing `*_props_json`
  (list/map props came back NULL). +2
- Comparison2 [4] — element-wise list ordering in `_gql_order_cmp`;
  maps/nodes/rels unorderable → null. **extends B2.** +4
- Quantifier5–8 — unique `json_each` alias so nested quantifiers
  (`single(x IN .. WHERE none(y IN x ..))`) don't shadow. **B3.** +24
- list ORDER BY [9]/[10]/[31] (+ ReturnOrderBy1) — order-preserving
  sort key for JSON lists in `_gql_order_key`. +7
- Quantifier4 [10] — strict 3VL for `all()` (stale "Precedence1
  conflict" comment was obsolete). **B3.** +5
- Create6 [10]-[13] — edge properties in UNWIND+CREATE result path
  (foreach-binding rel props, edge aggregates, edge prop projection).
  **B4.** +4
- Create6 [7]/[14] — aggregating WITH after CREATE (pure-aggregate
  collapse over per-iteration maps). **B4 — Create6 now 14/14.** +2
- Match6 [15]/[16]/[19]/[20] — full variable-length named-path
  hydration (interleaved `elem_ids` CTE column + position-parity
  build_path_from_ids). **This is Phase C / C3 — landed early.** +4
- List5 [21]/[29]/[31]/[34] — `IN` 3VL for nested-null list elements
  (gql_eq_json for containers, typed SQL `=` for scalars). +4
- Aggregation2 [9]/[11]/[12] — Cypher-orderability `min()`/`max()` via
  custom `_gql_min`/`_gql_max` aggregates (order-key total order, type
  preserved via sqlite3_value_dup). Aggregation2 now 12/12. +3
- WithOrderBy2 [21]-[24] + WithOrderBy4 [8] — ORDER BY referencing an
  input-scope var the WITH drops is now emitted INSIDE the CTE body
  (input tables still in FROM), not silently dropped. Narrow trigger
  (expr_refs_dropped_input_var); aggregating ORDER BY exprs excluded. +5
- Graph7 [1] + Merge6 [3]/[7] + Merge7 [5] — dynamic property lookup
  `n[expr]` / `n[$key]` on a node/edge via new `_gql_node_prop`/
  `_gql_edge_prop` EAV-by-runtime-key UDFs. Graph7 [2]/[3] (CREATE+RETURN,
  executor-evaluated) still open. +4
- 2 correctness-only commits (+0 TCK): boolean projection through WITH
  renders as JSON boolean; undirected MERGE matches either orientation.

Note: the order-key total order (object < list < text < bool < number
< null) now powers ORDER BY (_gql_order_key), list `=`/`IN`
(gql_eq_json), and `min()`/`max()` — a reusable orderability core.

Investigated WithOrderBy2 [21]-[24] (DESC examples) and Merge5 [12]/[13]
— both NOT the orderability core; both are scope/transform-ordering bugs:
- WithOrderBy2 [21]-[24] + WithOrderBy4 [8]: FIXED (commit 6bc2736) — ORDER BY
  on a dropped input-scope var now emitted inside the CTE body.
- Merge5 [12]/[13]: combined synth re-match in handle_match_merge drops
  inline node properties → undirected double-count. (still open)
- Undirected variable-length (Match9 [1]/[3], Match6 [14], Pattern1 [10]/[17]/
  [18], Delete4): tried making generate_varlen_cte bidirectional (both edge
  orientations) — gets the row COUNT closer but the outer undirected endpoint
  matching then double-counts (regressed Delete4 [2]); reverted. The CTE
  bidirectional traversal AND the outer endpoint-pattern emission must be made
  consistent together (dedupe one against the other). Not a CTE-only fix.
- With1 [4] / Match9 [9]: path variable forwarded through WITH ("no such
  column: p"); OPTIONAL varlen named path returns [] instead of null.

#### Next-session priorities (clean wins are exhausted; these are the tail)
Remaining 164 fails + 48 errors cluster into heavy interrelated areas:
1. **Variable-length variants** (~12+): undirected (Match9 [1]/[3], Match6
   [14], Pattern1 [10]/[17]/[18]), multi-segment / two-varlen-in-one-path
   (Match6 [17]), mixed fixed+varlen chains returning empty (Match5 [25]/[26]/
   [28], Match4 [4]/[5]). All touch generate_varlen_cte + the outer
   endpoint/rel emission together. The elem_ids interleaved column (added this
   session) is the foundation; the matching/dedupe is the remaining work.
2. **Temporal** (~40: Temporal2/3/6/8/10) — week/quarter date construction,
   timezone-name normalization, Duration type+arithmetic. Own initiative (C2).
3. **NaN** (Comparison1 [8], Comparison2 [5]) — 0.0/0.0 is SQLite NULL; needs a
   NaN sentinel threaded through division + comparison + ordering. Invasive.
4. **Niche**: Merge5 [12]/[13] (combined-pattern property drop), Merge1 [13]
   (path bound to MERGE), EXISTS{} subqueries (T-0139, unimplemented),
   path-through-WITH (With1 [4]).
Verification recipe for every change: `angreal test unit` + `angreal test
functional` + full `angreal test tck`, then diff the pass-set against the prior
run (script: print status+feature+scenario per record, comm -13/-23). SQL dump:
temp `getenv("GQLITE_DUMP_SQL")` fopen in handle_generic_transform (generic
path) or execute_match_return_query (MATCH+RETURN path). `angreal dev clean`
after any function-signature change (incremental builds corrupt).

Remaining leads (root causes documented in agent memory files):
- **Merge5 [12]/[13]** — combined synth re-match in handle_match_merge
  drops inline node properties → undirected double-count rows.
- **C3 residual**: Match6 [14] undirected varlen, [17] two-varlen
  segments in one path.
- **B5 (T-0332)** still active: Pattern2 [7]/[8]/[11] residual.
- Temporal (C2, ~43), Comparison2 [3]/[5] (entities-in-lists, NaN),
  Quantifier9/11 (rand/reverse/concat pipeline).