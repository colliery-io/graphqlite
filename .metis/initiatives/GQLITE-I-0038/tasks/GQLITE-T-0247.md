---
id: e13-typeconversion-list-graph
level: task
title: "E13: TypeConversion + List/Graph small-function gaps"
short_code: "GQLITE-T-0247"
created_at: 2026-05-18T17:10:00+00:00
updated_at: 2026-05-18T17:10:00+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E13: TypeConversion + List/Graph small-function gaps

Parent initiative: [[GQLITE-I-0038]] · Clusters **TypeConversion4 + List1 + List6 + Graph5** · Current count: **~35 scenarios**

## Objective

Four small clusters, each fixable in a few hundred lines:

1. **`toString()` on booleans / Any (TypeConversion4 [2]/[3]/[4]/[5]).**
   `toString(true)` returns `"1"`, spec wants `"true"`. Same for
   property-stored booleans and the polymorphic Any case.

2. **`toString()` argument validation (TypeConversion4 [10]).**
   `toString(node)`, `toString(rel)`, `toString(list)` should
   TypeError; today they return wrong-shape values.

3. **List index-type validation (List1 [7]/[9]).**
   `[1,2,3][n]` where `n` is a string (passed as parameter) should
   TypeError; today it returns null.

4. **`size()` on pattern predicates (List6 [6]).**
   Pre-Cypher-9 `size((a)-->(b))` was a path counter; current spec
   removes it. Eight examples expect a SyntaxError; today we run them.

5. **Conjunctive label expression (Graph5 [3]/[4]).**
   `MATCH (n) WHERE n:Label1:Label2` — grammar rejects the second
   colon. Needs a grammar rule update for label conjunction in
   WHERE/RETURN positions.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

-- toString on boolean
SELECT cypher('RETURN toString(true)');     -- expect "true"
SELECT cypher('RETURN toString(false)');    -- expect "false"

-- toString on invalid type
SELECT cypher('CREATE (n:N) WITH n RETURN toString(n)'); -- expect TypeError

-- List index with non-int
SELECT cypher('WITH [1,2,3] AS l RETURN l[''x'']');  -- expect TypeError

-- size on pattern predicate
SELECT cypher('MATCH (a) RETURN size((a)-->())');  -- expect SyntaxError

-- Conjunctive label expression
SELECT cypher('MATCH (n) WHERE n:Label1:Label2 RETURN n');  -- expect parse OK
EOF
```

## Target files

- `src/backend/transform/transform_func_string.c` (or wherever
  `transform_tostring_function` lives) — fix boolean → "true"/"false";
  emit TypeError for node/rel/list args.
- `src/backend/transform/transform_validate.c` — add a check that
  rejects parameterised list index access where the parameter type
  is known to be non-integer (List1 [7]/[9]). May need a deferred
  runtime TypeError UDF since param types are dynamic.
- `src/backend/transform/transform_func_list.c::transform_size_function`
  — reject pattern-predicate args at transform time, emit
  SyntaxError "size() on pattern predicates is no longer supported".
- `src/backend/parser/cypher_gram.y` — accept `n:Label1:Label2` as a
  conjunctive label expression in WHERE / RETURN. Today only allowed
  in node-pattern positions.

## Expected delta

`+20` to `+30`.

Scenarios expected to flip:
- TypeConversion4 [2]–[5], [10] (~10)
- List1 [7], [9] (~9)
- List6 [6] (~8)
- Graph5 [3], [4] (~6)

## Verification

```sh
angreal build extension
angreal test tck --filter "TypeConversion4|List1|List6|Graph5" 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per reproducer.

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] `toString(true)` → `"true"`, `toString(false)` → `"false"`.
- [ ] `toString(<unsupported>)` raises TypeError.
- [ ] List index with non-integer parameter raises TypeError.
- [ ] `size(<pattern-predicate>)` raises SyntaxError.
- [ ] `WHERE n:Label1:Label2` parses and matches conjunctive labels.

## Risks

- Adding parser rules can introduce S/R conflicts (the existing
  grammar has `%expect 9` S/R and `%expect-rr 3` R/R). Adjust the
  expectation counts after the change.
- The toString TypeError fix needs to keep `toStringOrNull()` returning
  null for the same inputs.

## Status updates

*To be added during implementation*
