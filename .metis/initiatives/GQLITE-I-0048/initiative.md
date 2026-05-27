---
id: collection-comparability-semantics
level: initiative
title: "Collection & Comparability Semantics"
short_code: "GQLITE-I-0048"
created_at: 2026-05-27T02:28:01.761753+00:00
updated_at: 2026-05-27T02:28:01.761753+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: L
initiative_id: collection-comparability-semantics
---

# Collection & Comparability Semantics

## Context **[REQUIRED]**

The value-semantics tail left after [[GQLITE-I-0044]]: heterogeneous
collections that contain graph entities, the full cross-type orderability
total order, NaN, and a couple of list/aggregation edge cases. I-0044 built
the **orderability core** — `_gql_order_key` (ORDER BY), `gql_eq_json`
(list `=`/`IN`), and `_gql_min`/`_gql_max` — all keyed on the total order
**object < list < text < bool < number < null**. This initiative finishes the
value model on top of that core.

**~15-20 scenarios** across Comparison1/2, WithOrderBy1, ReturnOrderBy1,
List12, Quantifier9/11.

## Goals & Non-Goals **[REQUIRED]**

**Goals:**
- **G1**: Entities inside heterogeneous lists — `UNWIND [n, r, p, 1.5, [..],
  {..}] AS x` and "sort distinct types". The UNWIND/list-build must carry the
  MATCH entity aliases into scope, and the sort must use the full cross-type
  orderability (map < node < rel < list < path < string < bool < number <
  null per the TCK).
- **G2**: List-comprehension / pattern-comprehension over collected entities
  (List12 [1]/[2]/[4]/[5] — `nodes(x)`/`x.id` over a collected/comprehended
  path element).
- **G3**: NaN — `0.0/0.0` must produce a NaN value (SQLite returns NULL), with
  comparison semantics: NaN <op> number = false, NaN = NaN = false. Needs a
  NaN sentinel threaded through division + comparison + ordering.
- **G4**: List-equality / quantifier-identity chains — WithOrderBy1 [45]
  (`orderedX = range(0,n-1)`), Quantifier9/11 [3]/[4]/[5]
  (`any(...) = NOT none(...)` over a rand/reverse/concat-built list).
- **G5**: Zero-regression per the [[GQLITE-I-0044]] verification recipe.

**Non-Goals:**
- **NOT** re-deriving the orderability total order — reuse the I-0044 core.
- **NOT** temporal value semantics ([[GQLITE-I-0046]]).
- **NOT** new collection functions — only fixing existing ones.

## Detailed Design **[REQUIRED]**

Core lives in `src/backend/runtime/udf_helpers.c` (`_gql_order_key`,
`gql_eq_json`, `gql_order_cmp_func`, `_gql_min`/`_gql_max`) +
`transform_expr_ops.c` (comparison/arithmetic emission) +
`transform_unwind.c` / list-build.

### Failing clusters & root causes (from I-0044 investigation)

**C1 — Entities in heterogeneous lists / sort distinct types (WithOrderBy1
[21]/[22], ReturnOrderBy1 [11]/[12], Comparison2 [3]).**
`MATCH p=(n)-[r]->() UNWIND [n, r, p, 1.5, ['x'], 'y', null, false, {a:1}] AS t
... ORDER BY t` errors "no such column: _gql_default_alias_0.id" — building the
list references the MATCH entity aliases, which are out of scope in the UNWIND
CTE. Two parts: (a) carry the MATCH entity aliases into the list-build/UNWIND
(extends the carry work from I-0044 Unwind1 [12]); (b) the sort then needs the
FULL cross-type order including entities (map<node<rel<list<path<scalars). The
`_gql_order_key` core already orders scalars/lists/objects; extend its object
branch to distinguish node/rel/path (currently all `{`-objects collapse). Also
Comparison2 [3]'s residual: booleans-in-lists coerce to int 1, paths encode as
arrays — entity type fidelity inside lists.

**C2 — List-comprehension / pattern-comprehension over entities (List12
[1]/[2]/[4]/[5]).** `[x IN collect(n) | ...]` and `nodes(x)` where x is a
collected/comprehended element — errors "no such column: _unwind_0.value.id"
and "nodes() argument must be a path variable, got: x".

**C3 — NaN (Comparison1 [8], Comparison2 [5]).** `0.0/0.0` → SQLite NULL, not
NaN. Cypher: NaN > 1 = false, NaN = NaN = false, NaN <> NaN = true. SQLite
coerces NaN doubles to NULL, so a **NaN sentinel** must be threaded through a
`_gql_div` UDF (emit the sentinel for 0.0/0.0) and recognized by
`gql_order_cmp_func` / `gql_eq_json`. Invasive — touches division, comparison,
ordering.

**C4 — List-equality / quantifier-identity chains.**
- WithOrderBy1 [45] (10 examples, ~5 non-temporal): `WITH ... collect(x) ...
  RETURN orderedX = range(0, n-1)` — ordered-collect + list `=`.
- Quantifier9/11 [3]/[4]/[5]: `any(...) = NOT none(...)` over a list built via
  `[y IN inputList WHERE rand()>0.5 | y]` + `reverse` + list-concat across 3
  UNWIND rounds — data-dependent on the rand/reverse/concat pipeline (the
  quantifier equality itself works on a plain list; see memory
  quantifier_equality_dropped).

## Alternatives Considered **[REQUIRED]**

- **NaN as a magic text sentinel** (e.g. `'NaN'`) vs trying to store a real
  IEEE NaN double. Sentinel is the pragmatic choice — SQLite drops real NaN to
  NULL. Must be hidden from user-visible output (renders as `NaN`).
- **Skip C3 (NaN) entirely** — only ~6 scenarios and the most invasive. Could
  be deferred/dropped if cost outweighs value; record the decision.

## Implementation Plan **[REQUIRED]**

Decompose at pickup. Suggested tasks:
1. **T: Entities in heterogeneous lists + full orderability** (C1) — highest
   value (~6); extends the carry + order-key core.
2. **T: List/pattern-comprehension over entities** (C2).
3. **T: List-equality / quantifier-identity chains** (C4) — reverse/concat/
   ordered-collect correctness.
4. **T: NaN sentinel** (C3) — most invasive; consider last or defer.

## Acceptance Criteria

- [ ] Comparison1/2, WithOrderBy1 [21]/[22]/[45], ReturnOrderBy1 [11]/[12],
  List12, Quantifier9/11 failing scenarios move to pass (NaN possibly deferred
  with a recorded decision).
- [ ] Zero TCK regressions (full pass-set diff each PR); unit 944/944 +
  functional clean.
- [ ] Each task logs its TCK delta here.

## Status Updates

### 2026-05-27 — Created

Filed from the I-0044 push (94.9%). Discovery phase. The orderability core
(_gql_order_key / gql_eq_json / _gql_min/max) already landed under I-0044;
this initiative builds the entity-in-collection, full-orderability, NaN, and
list-identity semantics on top. Awaiting human review before decomposition.