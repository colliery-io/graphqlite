---
id: e4-reject-unbound-pattern
level: task
title: "E4: Reject unbound pattern variables in WHERE pattern predicates (cluster A subset)"
short_code: "GQLITE-T-0238"
created_at: 2026-05-18T12:24:40.0+00:00
updated_at: 2026-05-18T12:24:40.0+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E4: Reject unbound pattern variables in WHERE pattern predicates (cluster A subset)

Parent initiative: [[GQLITE-I-0038]] · Cluster **A** subset · Current count: **~17 scenarios** (`expressions/pattern/Pattern1.feature` [10])

## Objective

`MATCH (n) WHERE <pattern> RETURN n` is only legal if every variable in
`<pattern>` is already bound (or refers to the matched `n`). Introducing
a fresh variable in a WHERE pattern is a Cypher SyntaxError
(UndefinedVariable). We currently accept the pattern and produce a
nonsense result.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('MATCH (n) WHERE (a) RETURN n');
SELECT cypher('MATCH (n) WHERE (n)-[r]->(a) RETURN n');
EOF
```

Expected: each query should raise `SyntaxError: UndefinedVariable` —
`a` (in scenario 1) and `r`, `a` (in scenario 2) are not bound before
the WHERE pattern is evaluated. Currently we evaluate the pattern as a
fresh match and return rows.

## Target files

- `src/backend/transform/transform_validate.c` — add a new check
  (or extend `validate_expr_typed`) for AST_NODE_PATH appearing inside
  a WHERE expression. Walk the path's elements; for each NODE_PATTERN
  / REL_PATTERN with a variable, verify it's already bound in the
  current scope.
- The scope is the variables collected by the `collect_pattern_names`
  pass for the surrounding MATCH (and prior MATCH/WITH/UNWIND clauses).

## Expected delta

`+17` for `expressions/pattern/Pattern1.feature` [10] examples.

Some examples may also depend on a working pattern-existence
evaluation; those would still fail on result correctness but at least
they shouldn't silently produce wrong rows.

## Verification

```sh
angreal build extension
angreal test tck --filter Pattern1 2>&1 | tail -5
# Expected: Pattern1 fail count drops by ~17.

angreal test tck 2>&1 | grep "TCK \[ext"
# Expected: pass count rises by ~17 from current 3109.

# Targeted spot-checks — both must error with SyntaxError:
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('MATCH (n) WHERE (a) RETURN n');
SELECT cypher('MATCH (n) WHERE (n)-[r]->(a) RETURN n');
EOF

# Pattern existence checks that ARE legal must still work:
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('CREATE (:A)-[:T]->(:B)');
SELECT cypher('MATCH (n:A), (m:B) WHERE (n)-[]->(m) RETURN n, m');
EOF
# Expected: 1 row.

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] All 17 Pattern1 [10] examples fail with a SyntaxError-classified
      diagnostic (the harness `_classify()` recognises "syntax").
- [ ] Pattern existence checks where all variables ARE bound still
      work (Pattern1 [12] and similar passing scenarios stay green).
- [ ] No other regressions.

## Status updates

*To be added during implementation*
