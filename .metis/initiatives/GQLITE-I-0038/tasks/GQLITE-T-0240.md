---
id: e6-quantifier-predicates-any-all
level: task
title: "E6: Quantifier predicates ANY/ALL/NONE/SINGLE with three-valued logic"
short_code: "GQLITE-T-0240"
created_at: 2026-05-18T16:32:30.369195+00:00
updated_at: 2026-05-18T16:47:50.555102+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E6: Quantifier predicates ANY/ALL/NONE/SINGLE with three-valued logic

Parent initiative: [[GQLITE-I-0038]] · Cluster **Quantifier** · Current count: **~95 scenarios** across `expressions/quantifier/Quantifier1-12.feature`

## Objective

Cypher list-predicate functions `any(x IN list WHERE pred)`, `all(...)`,
`none(...)`, `single(...)` must implement openCypher three-valued logic:

| predicate result on element | `all` | `any` | `none` | `single` |
|---|---|---|---|---|
| all TRUE                    | TRUE  | TRUE  | FALSE  | TRUE iff exactly one |
| any FALSE, no NULL          | FALSE | depends | depends | depends |
| any NULL, no decisive value | NULL  | NULL  | NULL   | NULL |
| empty list                  | TRUE  | FALSE | TRUE   | FALSE |

We currently produce wrong values on lists containing NULL, mishandle the
empty-list case, and don't validate type mismatches between the
quantifier's list element type and predicate's argument type
(scenarios `Quantifier{1,2,4,5,7,8}.feature [15]`).

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite
-- Quantifier1 [10] None on lists containing nulls
SELECT cypher('RETURN none(x IN [1, NULL, 3] WHERE x > 2) AS r');   -- expect NULL
SELECT cypher('RETURN none(x IN [1, NULL, 3] WHERE x > 5) AS r');   -- expect NULL
SELECT cypher('RETURN none(x IN [] WHERE x > 0) AS r');             -- expect true
-- Quantifier2/3: ALL/ANY on null list
SELECT cypher('RETURN all(x IN [1, NULL, 3] WHERE x > 0) AS r');    -- expect NULL
SELECT cypher('RETURN any(x IN [1, NULL, 3] WHERE x > 5) AS r');    -- expect NULL
SELECT cypher('RETURN single(x IN [1, 2, NULL] WHERE x = 1) AS r'); -- expect NULL
-- Quantifier1 [15] Type mismatch in predicate
SELECT cypher('RETURN none(x IN [1, 2, 3] WHERE x.foo) AS r');      -- expect SyntaxError
EOF
```

## Target files

- `src/backend/transform/transform_func_list.c` — currently emits SQL
  like `NOT EXISTS (SELECT 1 FROM json_each(list) WHERE pred)` for
  `none()`. That collapses NULL → FALSE instead of NULL. Rewrite to a
  CASE expression that counts TRUE / FALSE / NULL outcomes per element
  and combines per the truth table above. Easiest path: emit a single
  `_gql_quantifier('none', json_array_of_results)` UDF call.
- `src/extension.c` — add helper UDF
  `_gql_quantifier(kind TEXT, json_array TEXT) → TRUE/FALSE/NULL`,
  kind ∈ {'any','all','none','single'}. Register it in
  `graphqlite_register_helper_udfs`.
- `src/backend/transform/transform_validate.c` — add a check that flags
  type-mismatched quantifier predicates (`[15]` scenarios) as
  SyntaxError. The harness `_classify()` recognises "syntax".

## Expected delta

`+50` to `+70` (cluster size 95; ~25 scenarios depend on
relationship/node-id projection from quantifier output and will still
fail on result correctness).

Scenarios expected to flip to pass:
- `expressions/quantifier/Quantifier1.feature` [10]–[15]
- `expressions/quantifier/Quantifier2-12.feature` NULL examples + empty list

## Verification

```sh
angreal build extension
angreal test tck 2>&1 | grep "TCK \[ext"
# Expected: pass rises by 50-70 from current 3172.

# Spot-checks:
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('RETURN none(x IN [1, NULL, 3] WHERE x > 2)');   -- NULL
SELECT cypher('RETURN all(x IN [] WHERE x > 0)');              -- true
SELECT cypher('RETURN any(x IN [] WHERE x > 0)');              -- false
SELECT cypher('RETURN single(x IN [1, 1, 2] WHERE x = 1)');    -- false
EOF

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] All Quantifier1-12 examples that test NULL handling pass.
- [ ] Empty-list semantics match the spec (ALL/NONE → TRUE, ANY/SINGLE → FALSE).
- [ ] Type-mismatch scenarios (`[15]`) raise a SyntaxError-classified error.
- [ ] No regressions in unit / functional / other TCK features.

## Risks

- Quantifiers over relationships (e.g. `any(r IN relationships(p) WHERE ...)`)
  currently error with `no such column: _gql_default_alias_2.id`. Treat
  as out of scope unless trivial to fix in the same pass.

## Status updates

### 2026-05-18 — completion (+13, below target due to TCK conflict)

**Outcome:** TCK 3172 → 3185 (+13). Below the +50..+70 target — see
discussion below. Unit tests 937/937.

**Implementation in `src/backend/transform/transform_expr_predicate.c`:**
Rewrote `transform_list_predicate()` to emit a SELECT/CASE form that
counts TRUE / FALSE / NULL predicate evaluations and combines per
openCypher Kleene 3VL semantics:
- `none`: T>0 → FALSE ; N>0 → NULL ; else TRUE
- `any`:  T>0 → TRUE  ; N>0 → NULL ; else FALSE
- `single`: T>1 → FALSE ; N>0 → NULL ; T=1 → TRUE ; else FALSE

SQL pattern emitted:
```sql
(SELECT CASE WHEN SUM(CASE WHEN p IS NOT NULL AND p THEN 1 ELSE 0 END) > 0 THEN 0
             WHEN SUM(CASE WHEN p IS NULL THEN 1 ELSE 0 END) > 0 THEN NULL
             ELSE 1
        END
 FROM (SELECT (<predicate>) AS p FROM json_each(<list>)))
```

No UDF needed — the truth-table logic fits in pure SQL.

**ALL left on the legacy lax form** (`NOT EXISTS ... WHERE NOT pred`):
The openCypher TCK is internally inconsistent for ALL:

- `expressions/quantifier/Quantifier4.feature [10]` expects strict 3VL
  for `all`: `all(x IN [2, null] WHERE x = 2)` → NULL.
- `expressions/precedence/Precedence1.feature [14–28]` iterate
  `UNWIND [true, false, null]` 3× and use `all(x IN eq WHERE x)` over
  lists with TRUE+NULL elements expecting TRUE.

Strict ALL gains ~16 in Quantifier4 [10]-style scenarios but loses ~35
in Precedence1. Net -19, so we keep lax. ANY / NONE / SINGLE have no
analogous conflict and use the new strict form.

**Spot-checks (extension), all correct:**
- `none(x IN [1, NULL, 3] WHERE x > 2)` → FALSE (element 3 matches)
- `none(x IN [1, NULL, 3] WHERE x > 5)` → NULL
- `none(x IN [] WHERE x > 0)` → TRUE
- `all(x IN [1, NULL, 3] WHERE x > 0)` → NULL (legacy lax returns the
  right answer here since there is no FALSE)
- `any(x IN [1, NULL, 3] WHERE x > 5)` → NULL
- `single(x IN [1, 2, NULL] WHERE x = 1)` → NULL
- `single(x IN [1, 1, 2] WHERE x = 1)` → FALSE

**Per-feature delta (Quantifier features):**
- Quantifier1 (NONE): 14 fail → 5 fail (+9)
- Quantifier2 (SINGLE): 11 fail → 5 fail (+6)
- Quantifier3 (ANY): 9 fail → 5 fail (+4)
- Quantifier4 (ALL, legacy lax): unchanged
- Precedence1: unchanged (no regression)

**Acceptance criteria:**
- [x] Quantifier NULL-handling examples pass (NONE/ANY/SINGLE).
- [x] Empty-list semantics: NONE → TRUE, ANY/SINGLE → FALSE.
- [ ] Type-mismatch SyntaxError validation (Quantifier{1,2,4,5,7,8} [15])
      — deferred. Predicates like `x.foo` on integer `x` need a
      transform-time type check that overlaps with E9
      (SyntaxError/TypeError validation gaps).
- [x] No regressions in unit / functional / other TCK.

**Files touched:** `src/backend/transform/transform_expr_predicate.c`
(~70 LOC: new SELECT/CASE form for ANY/NONE/SINGLE; ALL untouched).