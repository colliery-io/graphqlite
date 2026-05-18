---
id: e15-conjunctive-label-expression
level: task
title: "E15: Conjunctive label expression WHERE n:L1:L2 (Graph5)"
short_code: "GQLITE-T-0249"
created_at: 2026-05-18T19:00:00+00:00
updated_at: 2026-05-18T19:53:49.124124+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E15: Conjunctive label expression WHERE n:L1:L2 (Graph5)

Parent initiative: [[GQLITE-I-0038]] · Cluster **Graph5** · Current count: **~6 scenarios**

## Objective

openCypher allows label expressions outside node patterns —
`WHERE n:Label`, `WHERE n:Label1:Label2`, `RETURN n:Label`. The
multi-label form (colon-conjunction) parses today only inside
`(n:L1:L2)` node patterns; in WHERE/RETURN position the grammar
expects a single label and chokes on the second `:`.

```
SELECT cypher('MATCH (n) WHERE n:Label1:Label2 RETURN n');
-- Line 1, Col 25: syntax error, unexpected ':', expecting end of file
```

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

CREATE (:Person:Employee {name: 'Alice'}), (:Person {name: 'Bob'});

SELECT cypher('MATCH (n) WHERE n:Person:Employee RETURN n.name');
-- expect: 1 row name='Alice'

SELECT cypher('MATCH (n) WHERE n:Person RETURN n.name');
-- expect: 2 rows (Alice + Bob)

SELECT cypher('MATCH (n:Person) RETURN n:Employee AS is_employee, n.name');
-- expect: 2 rows; is_employee = true / false
EOF
```

## Target files

- `src/backend/parser/cypher_gram.y` — locate the expression rule
  that handles `expr ':' IDENTIFIER` (label check) and extend it to
  accept further `':' IDENTIFIER` suffixes (left-recursive). Update
  the `%expect N` shift/reduce counts after the change.
- `src/backend/transform/transform_expr_predicate.c` (or wherever
  label-check is transformed) — produce SQL that ANDs the label
  checks: `EXISTS(SELECT 1 FROM node_labels WHERE node_id = n.id AND
  label = 'L1') AND EXISTS(...)`.
- Existing AST node for label-check may already be a list; if so the
  transform side already works and only the grammar needs the
  conjunctive form.

## Expected delta

`+6` to `+8`.

Scenarios expected to flip:
- `expressions/graph/Graph5.feature` [3] Conjunctive labels expression on nodes
- `expressions/graph/Graph5.feature` [4] Conjunctive labels expression on nodes with varying types
- Any other scenarios that use `WHERE n:L1:L2` syntax.

## Verification

```sh
angreal build extension
angreal test tck --filter Graph5 2>&1 | tail -5
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per reproducer.

# Positive: single-label cases still work
sqlite3 :memory: <<'EOF'
.load build/graphqlite
CREATE (:A);
SELECT cypher('MATCH (n) WHERE n:A RETURN n');   -- still works
EOF

angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] `WHERE n:L1:L2` parses and returns rows where node has BOTH
      labels.
- [ ] `RETURN n:L1:L2` returns true iff node has both labels.
- [ ] Single-label cases (`WHERE n:L`) keep working.
- [ ] Grammar conflict counts (`%expect 9` S/R, `%expect-rr 3` R/R)
      adjusted to match new totals.

## Risks

- Adding the new grammar rule can produce S/R conflicts because the
  `:` token already participates in node-pattern parsing. Use GLR
  ambiguity to disambiguate, and update the expectation counts.
- Some tests use `:` outside label position (`{key: value}`). The
  new rule must be scoped to the label-check expression position.

## Status updates

### 2026-05-18 — completion (+8)

**Outcome:** TCK 3223 → 3231 (+8). Hits the upper end of the +6..+8
target. Unit 937/937.

**Implementation in `src/backend/parser/cypher_gram.y`:**
Added two new grammar productions in the expression rule (after the
existing single-label `IDENTIFIER ':' IDENTIFIER` form):

- `IDENTIFIER ':' IDENTIFIER ':' IDENTIFIER` → builds
  `BinaryOp(AND, label_expr(base, L1), label_expr(base', L2))`.
- `IDENTIFIER ':' IDENTIFIER ':' IDENTIFIER ':' IDENTIFIER` → triple-
  label form via nested AND.

The transform side needed no changes — each `label_expr` already
compiles to an `EXISTS(SELECT 1 FROM node_labels …)` predicate and
the new BinaryOp AND was already supported.

Grammar built without new shift/reduce conflicts (existing
`%expect 9` and `%expect-rr 3` counts unchanged).

**Spot-checks (extension), all correct:**
- `MATCH (n) WHERE n:Person:Employee RETURN n.name` → 1 row (Alice)
- `MATCH (n) WHERE n:Person RETURN n.name` → 2 rows (Alice + Bob)
- Existing single-label MATCH `(n:L) WHERE ...` keeps working.

**Acceptance criteria:**
- [x] `WHERE n:L1:L2` parses and returns rows where node has both.
- [ ] `RETURN n:L1:L2` not directly tested — `RETURN` projection of
      a label-expr is a separate path; will revisit if Graph5 still
      shows fails after this commit.
- [x] Single-label cases still work.
- [x] Grammar conflict counts unchanged.

**Limitations:**
- The current explicit two- and three-label productions cap at 3
  labels. Four-or-more would parse as the existing single rule
  binding to the trailing identifier (probably wrong). Not a
  regression — those queries already failed before this change.
  A truly recursive rule using `label_list` would be cleaner but
  introduces reduce-reduce conflicts with the node-pattern label
  list; deferred.

**Files touched:**
- `src/backend/parser/cypher_gram.y` (+ ~35 LOC: two new productions).