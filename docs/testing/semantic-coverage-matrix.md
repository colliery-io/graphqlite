# Semantic Coverage Matrix

**Purpose.** Track end-to-end write-then-read-back correctness for every
combination of Cypher write shape × target entity × value source × scalar
type × read-back shape. Complements `docs/cypher-coverage-matrix.md`
(which tracks *syntax* coverage only).

**Why.** GitHub issue #61 surfaced 7 bugs where the parser accepted the
query and the execution returned no error — but the round-trip data was
wrong. Five of the seven were "node path works, relationship path silently
NULL" or "source endpoint works, target endpoint silently NULL" — i.e.
sibling paths the syntax suite never exercised. This matrix makes every
sibling cell visible so gaps are filed instead of shipped.

**How.** Each cell is either:

- `tests/functional/FILE.sql:TEST` — a named test verifying the round-trip
- `— N/A —` with a one-line reason
- `GAP: short description` — known uncovered cell, file a ticket

Cells update via `docs/testing/update-matrix.sh` (TBD) or manually in
the PR that adds the test.

---

## Axes

| Axis | Values |
|------|--------|
| **Write shape** | `CREATE`, `MERGE`, `MATCH+CREATE`, `MATCH+MERGE`, `MATCH+MATCH+CREATE`, `MATCH+MATCH+MERGE`, `MERGE ON CREATE SET`, `MERGE ON MATCH SET`, `SET n.k = v`, `SET n += {..}`, `SET n += $p` |
| **Target entity** | **node variable**, **relationship variable** |
| **Value source** | **literal**, **`$param`**, **UNWIND-bound**, **map-entry (`n += map`)**, **nested property access** |
| **Scalar type** | TEXT, INTEGER, REAL, BOOLEAN, JSON/MAP, LIST |
| **Read-back shape** | `MATCH (n) RETURN n.k`, `MATCH (s)-[r]->(t) RETURN s.k, r.k, t.k`, `MATCH (…)-[r]->(t) RETURN t.*`, `RETURN DISTINCT … ORDER BY …`, multi-hop `(a)-[r1]->(b)-[r2]->(c)` |

The full cross-product is ≈ 5 000 cells; this matrix targets the
**pairwise-interesting subset** (~ 80 cells). Each cell exercises **one**
pairwise interaction that a real application would hit.

---

## 1. Write shape × Target × Value source

Read-back held constant at `MATCH (…) RETURN x.k` (scalar property read-back
on the written variable). Scalar type × read-back permutations are broken
out in sections 2 and 3.

| Write shape | Target | Literal | `$param` | UNWIND-bound | Map-entry |
|-------------|--------|---------|----------|--------------|-----------|
| `CREATE (n {k:v})` | node | `39:…` ✓ (many) | `26:Section 6` ✓ | `26:11.2` ✓ | n/a |
| `CREATE (a)-[:R {k:v}]->(b)` | rel | `03:…` ✓ | `26:7.1`, `39:#61.2` ✓ (T-0186) | GAP — T-0183 | n/a |
| `CREATE (n) SET n.k = v` | node | `39:T-0194a` ✓ | `39:T-0194c` ✓ | GAP | n/a |
| `CREATE (n) SET n += {..}` | node | `39:T-0194b` ✓ | `39:T-0194c` ✓ | GAP | `39:T-0194b` ✓ |
| `MERGE (n {k:v})` | node | `40:…` ✓ | `26:…` ✓ | GAP | n/a |
| `MERGE (a)-[r:R {k:v}]->(b)` | rel | `39:T-0187` ✓ | `39:T-0187` ✓ (T-0186/7) | GAP | n/a |
| `MERGE (n) SET n.k = v` | node | `39:T-0195a` ✓ | GAP | GAP | n/a |
| `MERGE (n) SET n += {..}` | node | `39:T-0195b` ✓ | GAP | GAP | `39:T-0195b` ✓ |
| `MATCH (a) CREATE (a)-[:R]->(b)` | new rel | `10:…` ✓ | GAP | GAP | n/a |
| `MATCH (a) CREATE (a)-[:R]->(b) SET b.k = v` | new node | `39:T-0198c` ✓ | GAP | GAP | n/a |
| `MATCH (a) MATCH (b) CREATE (a)-[:R]->(b)` | rel | `39:T-0197a` ✓ | `39:T-0197c` ✓ | GAP | n/a |
| `MATCH (a) MATCH (b) MERGE (a)-[r]->(b) SET r.k = v` | rel | `39:T-0196` ✓ | `39:T-0196` ✓ | GAP | n/a |
| `MATCH (a) MATCH (b) SET a.k = v` | node | `39:T-0198d` ✓ | GAP | GAP | n/a |
| `MERGE ... ON CREATE SET r.k = v` | rel | `39:T-0195c` ✓ | `39:T-0187` ✓ | GAP | n/a |
| `MERGE ... ON MATCH SET r.k = v` | rel | `39:T-0202a` ✓ | `39:T-0202b` ✓ | GAP | n/a |
| `MERGE ... SET r += {..}` | rel | `39:T-0202c` ✓ | `39:T-0202d` ✓ | GAP | `39:T-0202c` ✓ |
| `UNWIND [..] AS item CREATE (n {k:item.f})` | node | GAP | GAP | — T-0183 — | n/a |
| `UNWIND [..] AS item MERGE (n {k:item.f})` | node | GAP | GAP | — T-0183 — | n/a |
| `UNWIND [{..}] AS item MATCH (n {k:item.f})` (read) | — | `39:T-0185a` ✓ | `39:T-0185b` ✓ | n/a | n/a |
| `FOREACH (x IN [..] \| CREATE ...)` | node | `28:…` ✓ | GAP | GAP | n/a |

**Column legend:** `NN:…` = `tests/functional/NN_*.sql` covers this cell; `✓` = verified write + read-back assertion; ticket IDs (T-NNNN) reference originating bug.

---

## 2. Target endpoint property access in traversals

Read pattern `MATCH (s)-[r]->(t) RETURN s.k, r.k, t.k` — previously a
blind spot (GQLITE-T-0190 / issue #61.6).

| Scalar type | `s.k` | `r.k` | `t.k` | Notes |
|-------------|-------|-------|-------|-------|
| TEXT    | `39:T-0197b` ✓ | `39:T-0196` ✓ | `39:T-0197b` ✓ | |
| INTEGER | `39:T-0201 INTEGER` ✓ | ✓ | ✓ | |
| REAL    | `39:T-0201 REAL` ✓ | ✓ | ✓ | |
| BOOLEAN | `39:T-0201 BOOLEAN` ✓ | ✓ | ✓ | read-back as `true`/`false` |
| JSON    | `39:T-0201 JSON` ✓ | ✓ | ✓ | read-back is JSON text |
| LIST    | `39:T-0201 LIST` ✓ | ✓ | ✓ | read-back is JSON-array text |

Multi-hop `(a)-[r1]->(b)-[r2]->(c) RETURN a.k, r1.k, b.k, r2.k, c.k`:
TEXT covered by `39:T-0203a` ✓ (plus T-0203b for parameterized filters
and T-0203c for DISTINCT+ORDER BY). Other scalar types still GAP —
follow-up to extend T-0203 per type if regressions surface.

---

## 3. Parameterized vs literal symmetry

For every write path that accepts both literal and `$param` RHSes, a
parallel-passing test for each. Marks were reshuffled when #61.2/3/4/5
shipped — remaining gaps below.

| Pair | Literal | `$param` | Status |
|------|---------|----------|--------|
| CREATE rel inline prop | ✓ | ✓ (`39:T-0186`) | |
| MERGE rel inline prop | ✓ | ✓ (`39:T-0187`) | |
| ON CREATE SET rel | ✓ | ✓ (`39:T-0187`) | |
| ON MATCH SET rel | `39:T-0202a` ✓ | `39:T-0202b` ✓ | |
| Trailing SET after MERGE, rel | ✓ | GAP | file follow-up |
| Trailing SET after MATCH+MERGE, rel | ✓ | GAP | file follow-up |
| `SET r +=` on rel var | `39:T-0202c` ✓ | `39:T-0202d` ✓ | |

---

## Out-of-scope (tracked separately, **not** gap-tracked here)

- **Cypher syntax acceptance** (parser/grammar): `docs/cypher-coverage-matrix.md`.
- **Performance / scale** testing: future initiative.
- **openCypher TCK** integration: `tests/tck/` future work.

---

## Process

1. **When fixing a bug**, add a regression test to `tests/functional/39_issue_regression_tests.sql` using the `T-NNNN` naming pattern and link the cell here.
2. **When landing a feature**, add one matrix row and fill every applicable cell before merge (or explicitly mark GAP with a follow-up ticket).
3. **During PR review**, the reviewer checks that the matrix reflects the change. A CI lint (future: GQLITE-T-0204) will block PRs that touch `src/backend/transform/` or `src/backend/executor/` without updating this doc.
4. **Rotation**: once every six months, audit the matrix for rot. Move completed GAPs to "covered" and file follow-ups for any new blind spot.

---

## Current gap census (2026-04-18)

- 38 cells marked covered (linked to tests).
- 22 cells marked GAP (ready for follow-up tickets).
- 6 cells N/A.

This matrix ships with the GQLITE-I-0035 initiative. Task breakdown:

- **GQLITE-T-0200** — This document (matrix scaffolding + initial census).
- **GQLITE-T-0201** — Fill node/rel symmetry gaps (integer/real/bool/json/list traversal read-back).
- **GQLITE-T-0202** — Fill ON MATCH SET + `SET n +=` on rel gaps.
- **GQLITE-T-0203** — Fill multi-hop `(a)-[r1]->(b)-[r2]->(c)` read-back gaps.
- **GQLITE-T-0204** — CI lint: require matrix diff on transform/executor PRs.

---

## Coverage update (2026-05-26) — I-0044 Phase-B conformance push

This PR (#73) lands the I-0044 TCK push (94.9% executable). Round-trip cells
that flipped from broken/GAP to covered, verified through the openCypher TCK
harness (`angreal test tck`, full pass-set diff, zero regressions) rather than
new per-cell functional SQL:

- **`CREATE (n {k: LIST/JSON})` → read-back `n.k`** — list/map literal write
  now round-trips (previously silently NULL). Backed by the new `*_props_json`
  property columns in `query_dispatch.c` / `executor_result_project.c`.
- **`CREATE … RETURN n` with SET-clause + general-expression values** —
  `handle_create_return` runs the SET loop and an `executor_eval_value`
  fallback, so computed properties round-trip in the same statement.
- **`MATCH+MERGE … RETURN`** — handles map/param values and SET, including
  relationship-variable property read-back (`fetch_edge_prop_*`).
- **`UNWIND … MERGE … RETURN`** — pure-aggregate WITH collapse + edge cells.

Cross-cutting read-back semantics (orderability total order over
node/rel/list/map/scalar, list `=`/`IN`, `min`/`max`) also landed; their
remaining deep tail is tracked in initiatives [[GQLITE-I-0046]] /
[[GQLITE-I-0047]] / [[GQLITE-I-0048]].

## Coverage update (2026-05-27) — I-0047 P1: undirected variable-length paths

GQLITE-T-0334 (initiative [[GQLITE-I-0047]]). Verified through the openCypher
TCK harness (`angreal test tck`, full pass-set diff, zero regressions; unit
944/944; functional clean):

- **`MATCH (a)-[*]-(b)` (undirected variable-length)** now traverses *both*
  edge orientations. `generate_varlen_cte` previously emitted only the
  `source_id → target_id` direction, so an undirected varlen MATCH undercounted
  (3 directed rows where openCypher matches 6). The recursive CTE base case now
  UNIONs both orientations and the recursive step advances to the edge's *other*
  endpoint (`source_id = end OR target_id = end`); the outer endpoint binding
  stays directional. Fixes Match9 [1]/[3].
- **`MATCH … DETACH DELETE a,b RETURN count(*)`** — `count()` is now computed
  from the live pre-delete MATCH (`handle_match_delete` pre-capture) instead of
  the accumulated delete count. The old delete-count path only *coincidentally*
  matched the expected aggregate for undirected-varlen shapes the MATCH
  undercounted; with the MATCH now correct it would have over-counted. Fixes
  Delete4 [1] (single-hop undirected, +1 bonus) and keeps Delete4 [2] correct.

- **`MATCH (n) WHERE (n)-[:R*..]-(m)` (varlen inside a WHERE existential
  pattern predicate)** — the `AST_NODE_PATH` EXISTS emitter
  (`transform_expr_predicate.c`) previously treated every rel as a single fixed
  hop, ignoring `rel->varlen`. New `emit_exists_varlen_path` emits a correlated
  recursive-CTE reachability check (same bidirectional traversal as the MATCH
  CTE; endpoint labels honored; inline endpoint properties fall back). Fixes
  Pattern1 [10]/[17]/[18].

Net TCK delta: +6 (Match9 [1]/[3], Delete4 [1], Pattern1 [10]/[17]/[18]), zero
regressions. Remaining I-0047 targets Match6 [14] (multi-segment fixed+varlen
named path) and Match6 [17] (zero-length named path) are distinct generators
tracked under P2 (GQLITE-T-0335) / P4 (GQLITE-T-0337).

## Coverage update (2026-05-27) — I-0047 P2: varlen relationship-property predicate

GQLITE-T-0335. Verified via the TCK harness (full pass-set diff, zero
regressions; unit 944/944; functional clean):

- **`MATCH (a)-[:T* {k: v}]->(b)` (inline relationship property predicate on a
  variable-length rel)** — `generate_varlen_cte`'s per-edge filter now folds in
  inline rel property predicates (an `edge_props_*` EXISTS per `{k: v}` pair)
  alongside the type constraint, applied to every edge in the base and
  recursive steps. Previously the property map was ignored, so the path matched
  regardless of edge properties. Fixes Match4 [5] (+1).

Investigation note: the Match5 [25]/[26]/[28]/[29] "multi-segment chain"
failures turned out **not** to be a path-matching bug — multi-segment
fixed+varlen chains already match correctly. They fail because their setup
(`MATCH (d:D) CREATE …`) only processes the first matched row — a write-path
multiplicity bug filed as GQLITE-T-0339, out of this initiative's scope.

## Coverage update (2026-05-27) — I-0047 P3: OPTIONAL MATCH row preservation

GQLITE-T-0336 (partial). Verified via the TCK harness (full pass-set diff,
zero regressions; unit 944/944; functional clean):

- **OPTIONAL node label constraints no longer drop the preserved anchor row.**
  An OPTIONAL node's label was emitted as an INNER `node_labels` join (for
  varlen-deferred targets and for every non-first optional node), which inner-
  joins away the NULL-seed row when the optional doesn't match. Labels now fold
  into the node's LEFT JOIN ON as a correlated EXISTS. Fixes Match7 [15]
  (OPTIONAL varlen + nulls) and Aggregation5 [2] (OPTIONAL MATCH + collect).

- **Variable-length paths now enforce relationship-uniqueness, not
  node-uniqueness.** `(s)-[:REL]->(b)-[:LOOP]->(b)` is a valid varlen path (two
  distinct edges) even though it revisits `b`; the CTE previously blocked node
  revisits. Fixes Match7 [12].
- **WITH-projected NULL node/edge variables render as SQL NULL in RETURN.** A
  node/edge bound via OPTIONAL and carried across WITH, when NULL, was projected
  as a bogus `{id:null,…}` object; the post-WITH projection now guards with
  `CASE WHEN id IS NULL THEN NULL`. Fixes Match7 [21], [27].

P3 net: +5 (Match7 [12]/[15]/[21]/[27], Aggregation5 [2]). Remaining OPTIONAL
work (multi-rel combined-EXISTS join ordering → derived-table rewrite;
bound-rel reverse optional → deferred-constraint plumbing) tracked under
GQLITE-T-0336.

## Coverage update (2026-05-27) — I-0047 P4: forward a path variable through WITH

GQLITE-T-0337. Verified via the TCK harness (rigorous pass-set diff vs prior
HEAD, zero regressions; unit 944/944; functional clean):

- **`MATCH p = (…) WITH p … RETURN p` now carries the path across the WITH
  boundary.** WITH's item loop bypassed `transform_expression` (which hydrates
  path vars), so a forwarded path emitted a bare `p` column → "no such column".
  WITH now emits the path-hydration SQL `AS p`, preserves the path metadata
  across the scope reset, and re-registers `p` as a path var on the CTE column;
  RETURN emits that column directly and the executor hydrates it. Fixes
  With1 [4].
- **OPTIONAL variable-length relationship list returns NULL (not `[]`) on a
  no-match.** The varlen edge-list projection used `json_group_array` over
  `json_each('[' || elem_ids || ']')`, which yields `'[]'` when `elem_ids` is
  NULL (OPTIONAL miss). Wrapped it in `CASE WHEN elem_ids IS NULL THEN NULL`.
  Fixes Match9 [9].

P4 (GQLITE-T-0337) complete: +2 (With1 [4], Match9 [9]).

## Coverage update (2026-05-28) — T-0339: MATCH+CREATE multi-row + general expressions

GQLITE-T-0339 (backlog bug). Verified via the TCK harness (full pass-set diff,
zero regressions; unit 944/944; functional clean):

- **`MATCH … CREATE …` now runs the CREATE for every matched row.** The
  legacy single-MATCH path took only the first matched row (hard-coded
  `break` in `bind_match_clause_into_varmap`). Now `execute_multi_match_create_query`
  materializes all matched rows into memory BEFORE running CREATE (running
  CREATE inside `sqlite3_step` invalidates the iterating SELECT), allocates a
  fresh `var_map` per row so CREATE-introduced bindings don't bleed across
  rows, and iterates every CREATE clause's patterns per row.
- **`CREATE (n {prop: <expr>})` evaluates general expressions** referencing
  MATCH-bound variables (e.g. `{name: d.name + '0'}`). Previously the prop
  handler only knew LITERAL / FUNCTION_CALL / MAP/LIST / PARAMETER values; a
  general-expression fallback now calls `executor_eval_value` with the current
  `var_map`.

Together these fix Match5 [25]/[28]/[29] (the setups that build a per-D `E`
layer with computed names). Multi-MATCH MATCH+CREATE keeps the legacy
first-row behavior pending a separate fix.

**Part 3 — `MATCH … DELETE … CREATE …` now runs the CREATE.**
`handle_match_delete` dropped the CREATE clause entirely. It now runs CREATE
first (per matched row via `execute_multi_match_create_query`) with the live
pre-delete bindings, then proceeds with the delete. Fixes Match5 [26].
Match4 [4] (UNWIND/collect setup) needs a distinct follow-up.

## Coverage update (2026-05-28) — validate: pattern expressions in SET RHS

`transform_validate.c`. Verified via the TCK harness (zero regressions; unit
944/944; functional clean):

- **A path pattern on the SET RHS is now a compile-time `SyntaxError`**
  (e.g. `SET n.prop = head(nodes(head((n)-[:REL]->()))).foo`). The existing
  RETURN/WITH validators do only a top-level type check; a path can be nested
  inside function calls / property access / subscripts. New recursive helper
  `expr_contains_path_pattern` walks the expression tree and the SET validator
  invokes it. Fixes Pattern1 [24].

## Coverage update (2026-05-28) — validate+match: rel-uniqueness + dedupe edges-table emission

Architectural correctness fix (zero TCK delta; zero regressions; unit 944/944;
functional clean):

- **New `validate_rel_uniqueness_in_match` validator** — a relationship
  variable may not appear twice within the same MATCH pattern
  (`MATCH (a)-[r]->()-[r]->(a)` → `SyntaxError:
  RelationshipUniquenessViolation`). Cross-clause re-use is allowed
  (binding-then-reuse) and unchanged.
- **Dedupe `edges AS <alias>` emission across MATCH clauses** — re-emitting an
  already-joined alias was the masquerading SyntaxError that Match3 [29]
  relied on; the validator above is now the explicit check. Match4 [7] no
  longer errors (still fails on a distinct over-counting bug, follow-up).

## Coverage update (2026-05-28) — path-wide rel-uniqueness across varlen and bound rels

`transform_match.c`. Verified via the TCK harness (full pass-set diff, zero
regressions; unit 944/944; functional clean):

- **Varlen segments may not re-use a path's bound non-varlen relationship.** The
  existing path-wide rel-uniqueness block already paired non-varlen edge aliases
  (`e_i.id <> e_j.id`). It now also enforces `<varlen>.visited NOT LIKE
  '%,<bound_rel.id>,%'` against each non-varlen rel in the same path — both
  table-aliased (`<alias>.id`) and post-WITH (`_with_0.r`) forms. The constraint
  is only emitted for **non-OPTIONAL** paths; OPTIONAL needs the constraint at
  the LEFT JOIN ON level to preserve the anchor row (deferred). Cross-varlen
  disjointness is also deferred (no failing scenario requires it). Fixes
  Match4 [7].

## Coverage update (2026-05-28) — REMOVE on null/unbound variable is a no-op

`executor_remove.c`. Verified via the TCK harness (3705 -> 3706, zero
regressions; unit 944/944; functional clean):

- **REMOVE on a null variable yields no side effect** (matches Cypher semantics
  and SET parity). `MATCH (n) OPTIONAL MATCH (n)-[r]->() REMOVE r.num` previously
  errored with `"Unbound variable in REMOVE: r"` because `is_variable_edge()`
  returns false for absent map entries (the OPTIONAL preserves the row but
  doesn't bind `r`), so the rel fell through to the node path's `entity_id < 0`
  error. Both `entity_id < 0` branches in `execute_remove_operations` now log a
  debug line and `continue`, treating the REMOVE item as a no-op. The existing
  "property not found on this entity" path was already silent — this aligns
  null-variable behavior with that. Fixes Remove1 [6].

## Coverage update (2026-05-28) — accept bidirectional bracketed rel pattern `<-[...]->`

`cypher_gram.y`. Verified via the TCK harness (3706 -> 3706 pass; one scenario
moves from `error` -> `fail` as parse now succeeds; full unit + functional clean):

- **Grammar accepts `<-[...]->` form** as equivalent to undirected `-[...]-`
  (Match5 [27] and any future scenario using both arrows on a bracketed rel).
  Five rules added to mirror the existing five `-[...]-` undirected forms
  (var-only, IDENTIFIER, BQIDENT, non_reserved_kw type, multi-type list); each
  passes `false/false` for `(left_arrow, right_arrow)` to `make_rel_pattern_varlen`.
  Bare bidirectional `<-->` was already supported. Match5 [27] still fails
  downstream on bidirectional varlen execution semantics; that fix is deferred.

## Coverage update (2026-05-28) — RETURN * skips named-path anon element prefixes

`executor_match.c` + `transform_return.c`. Verified via the TCK harness
(3706 -> 3707, zero regressions; unit 944/944; functional clean):

- **`RETURN *` after `MATCH p = (a)-->(b)` exposes only `a, b, p`**, not the
  synthesized rel alias. When a path is named (`p = ...`), anonymous path
  elements get synthesized variable names with prefixes `_pv_n<base>_<j>`
  (nodes) and `_pv_e<base>_<j>` (rels). The two `RETURN *` expansion sites
  (`executor_match.c` and `transform_return.c`) already skipped the older
  `_gql_default_alias_` and `__unnamed_rel_` prefixes; both now also skip
  `_pv_n` / `_pv_e`. Fixes Return7 [1].

## Coverage update (2026-05-28) — four-label conjunction in expression context

`cypher_gram.y`. Verified via the TCK harness (3707 -> 3708, zero regressions;
unit 944/944; functional clean):

- **`WHERE a:L1:L2:L3:L4` parses** as the conjunction `(a:L1) AND (a:L2) AND
  (a:L3) AND (a:L4)` (Graph5 [4], e.g. `a:C:A:A:C`). The `expr`-context label
  rules already covered 1/2/3 labels; the four-label form raised a parse error.
  Added a mirror rule chaining four `make_label_expr` conjuncts. Repeated labels
  are harmless — each conjunct is an independent EXISTS check. No bison conflicts
  (still `%expect 15` / `%expect-rr 3`).
