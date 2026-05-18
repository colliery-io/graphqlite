---
id: e8-mixed-type-order-by-list-value
level: task
title: "E8: Mixed-type ORDER BY + list/value comparison ordering"
short_code: "GQLITE-T-0242"
created_at: 2026-05-18T16:32:30.369195+00:00
updated_at: 2026-05-18T16:53:20.874149+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E8: Mixed-type ORDER BY + list/value comparison ordering

Parent initiative: [[GQLITE-I-0038]] · Cluster **WithOrderBy1 + Comparison2 + Precedence3** · Current count: **~31 scenarios**

## Objective

openCypher defines a total ordering across all value types for ORDER BY
and comparison purposes (the "orderability" relation, distinct from
"comparability"). The chain is roughly:

```
MAP < NODE < RELATIONSHIP < LIST < PATH < STRING < BOOLEAN < NUMERIC < TEMPORAL < NULL
```

Within numerics, NaN sorts after positive infinity. Within lists,
ordering is lexicographic with same total order applied element-wise.

We already wrap ORDER BY columns in `_gql_order_key(...)`. The current
`_gql_order_key` returns a single ordering key that handles primitives
and NULL, but mishandles:

1. Lists (WithOrderBy1 [9]/[10]: "Sort lists in ascending/descending
   order" — `row mismatch`).
2. Mixed-type columns (WithOrderBy1 [21]/[22]: "Sort distinct types"
   — `no such column: _gql_default_alias_0.id`, suggesting the SQL
   pipeline doesn't materialise the value).
3. List-valued node properties (WithOrderBy1 [31]/[32]).
4. Comparison2 + Precedence3 — direct value comparison with NULL or
   list operands yielding wrong truth values.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite
CREATE (:N {v: [3, 1, 2]}), (:N {v: [1, 2, 3]}), (:N {v: [2, 2, 2]});
SELECT cypher('MATCH (n:N) RETURN n.v ORDER BY n.v ASC');
-- Expect: [1,2,3], [2,2,2], [3,1,2]

CREATE (:N {v: 1}), (:N {v: "a"}), (:N {v: true}), (:N {v: null});
SELECT cypher('MATCH (n:N) RETURN n.v ORDER BY n.v ASC');
-- Expect openCypher orderability total order across the four types.

-- Comparison2 example: list comparison
SELECT cypher('RETURN [1,2,3] = [1,2,3]');                    -- true
SELECT cypher('RETURN [1,2,3] < [1,2,4]');                    -- true
SELECT cypher('RETURN [1, NULL] = [1, NULL]');                -- NULL (NULL-anywhere)
EOF
```

## Target files

- `src/extension.c::gql_order_key_func` — extend to produce a sortable
  prefix encoding of the type class plus the value. Suggested encoding:
  `"<type_tag><lexically-sortable-value>"` where `type_tag` is a single
  digit ordered to match the openCypher chain.
  - For lists, emit `"7" || json_array_of_keys(elements)` then sort
    lexicographically as text; or invoke `_gql_order_key` recursively
    over each element when building the JSON.
- `src/extension.c::gql_eq_func` (and `_gql_in`) — list equality with
  NULL elements must return NULL when any pair contributes a NULL
  comparison. Verify the existing helper already does this for
  primitives; extend to lists.
- `src/backend/transform/sql_builder.c` — confirm that
  `_gql_order_key(col)` is emitted whether `col` is a JSON/value column
  vs an aliased projection.
- Investigate the `no such column: _gql_default_alias_0.id` failure on
  WithOrderBy1 [21]/[22] — likely a transform-side bug where mixed-type
  RETURN drops the projection alias.

## Expected delta

`+20` to `+30`.

Scenarios expected to flip to pass:
- `clauses/with-orderBy/WithOrderBy1.feature` [9], [10], [21], [22],
  [31], [32], plus the related null-mixing examples
- `expressions/comparison/Comparison2.feature` list-equality scenarios
- `expressions/precedence/Precedence3.feature` operands-with-NULL

## Verification

```sh
angreal build extension
angreal test tck --filter "WithOrderBy|Comparison2|Precedence3" 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks
sqlite3 :memory: <<'EOF'
.load build/graphqlite
CREATE (:N {v: [3, 1, 2]}), (:N {v: [1, 2, 3]});
SELECT cypher('MATCH (n:N) RETURN n.v ORDER BY n.v ASC');
SELECT cypher('RETURN [1,2,3] < [1,2,4]');
EOF

angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] Mixed-type ORDER BY produces the openCypher canonical order.
- [ ] List ordering is lexicographic with element-wise total order.
- [ ] List equality / comparison with NULL elements returns NULL.
- [ ] WithOrderBy1 [21]/[22] no longer hit the
      `no such column: _gql_default_alias_0.id` error.

## Risks

- The encoding-prefix approach can blow up the ORDER BY key size for
  large lists. If that surfaces, switch to a custom collation function
  (sqlite3_create_collation) keyed on a parsed value, avoiding the
  text-prefix workaround.

## Status updates

### 2026-05-18 — blocked / out-of-scope investigation

**Outcome:** TCK unchanged (3203). No code changes — investigation
showed every fail in this cluster needs infrastructure substantially
bigger than the task budget. Logged here for future planning.

**What I verified is already working:**
- `[1,2,3] = [1,2,3]` → true
- `[1,2,3] = [1,2,4]` → false
- `[1, null] = [1, null]` → null (NULL-anywhere semantics)
- `[1, null] = [2, null]` → false
- Lists of integers of equal length sort lexicographically by accident
  (JSON text representation happens to lex-sort in the same order):
  `[1,2,3]`, `[2,2,2]`, `[3,1,2]` sort correctly.

**What's broken — each needs its own task:**

1. **List `>=` / `>` / `<` / `<=` (Comparison2 [4], 4+ scenarios).**
   `[1, null] >= [1]` currently returns false; spec wants true. We
   have no element-wise list comparison — `>=` between lists falls
   through to SQLite's plain text comparison on the JSON, which is
   wrong for any case where elements aren't integers of equal width.
   *Required:* new `_gql_list_cmp(a, b, op)` UDF that walks two JSON
   arrays in lockstep, propagating NULL when comparable.

2. **NaN comparison (Comparison2 [5], 3 scenarios).** Today
   `0.0 / 0.0` returns SQL NULL (division by zero), not IEEE NaN.
   So `0.0/0.0 < 1` evaluates to NULL; openCypher expects FALSE.
   *Required:* change division to emit NaN on /0 instead of NULL,
   then teach `_gql_order_key` and comparison operators to recognise
   NaN and emit FALSE for ordering predicates.

3. **Heterogeneous list of node/rel/path/scalar (WithOrderBy1 [21]/[22]
   and Comparison2 [3]).** `UNWIND [n, r, p, 1.5, 'x', null]` produces
   `no such column: _gql_default_alias_0.id` — the transform assumes
   UNWIND elements are scalars and tries to project node properties
   from them. *Required:* generalise UNWIND so each row's type is
   inspected at execution time, not transform time. Same root cause
   as Quantifier "relationships(p)" failures.

4. **List of lists / sort by mixed-type lists (WithOrderBy1 [9]/[10]).**
   `[null, 2]` vs `[null, 1]` sort element-wise with the openCypher
   orderability total order. *Required:* rewrite `_gql_order_key` to
   produce a type-tagged sort key per element, then a list key
   concatenates element keys. Big rewrite of the existing text-only
   `_gql_order_key`.

**Recommendation:** Split this initiative item into three separate
tasks (list comparison; NaN/IEEE; heterogeneous UNWIND), each
attackable in isolation.

**Acceptance criteria:**
- [ ] Mixed-type ORDER BY — not achieved.
- [ ] List ordering lexicographic with element-wise total order —
      not achieved.
- [x] List equality with NULL elements → NULL — **already works**.
- [ ] WithOrderBy1 [21]/[22] alias error — not fixed; root cause is
      heterogeneous UNWIND, not the ORDER BY layer.

**Files touched:** none.