---
id: e3-relax-order-by-undefinedvariable
level: task
title: "E3: Relax ORDER BY UndefinedVariable check — allow pre-WITH scope (cluster O)"
short_code: "GQLITE-T-0237"
created_at: 2026-05-18T12:24:33.0+00:00
updated_at: 2026-05-18T12:24:33.0+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E3: Relax ORDER BY UndefinedVariable check — allow pre-WITH scope (cluster O)

Parent initiative: [[GQLITE-I-0038]] · Cluster **O** · Current count: **10 scenarios** (false-positives created earlier in Phase D)

## Objective

The strict ORDER-BY UndefinedVariable validator that landed in commit
`41b9706` correctly catches 30 negative-test scenarios in WithOrderBy3
[8] but produces 10 false positives where ORDER BY legitimately
references a pre-WITH-scope variable (e.g.
`MATCH (a) WITH a.name AS name ORDER BY a.name + 'C'`). Find a check
that wins on both sides.

## Reproducer

```sh
# Should error (currently does, must keep failing):
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('WITH 1 AS a, 2 AS b, 3 AS c WITH a, b WITH a ORDER BY a, c RETURN a');
EOF
# Expected: SyntaxError UndefinedVariable on c.

# Should succeed (currently errors with the over-strict check):
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('CREATE ({name: ''A''}), ({name: ''B''}), ({name: ''C''})');
SELECT cypher('MATCH (a) WITH a.name AS name ORDER BY a.name + ''C'' LIMIT 2 RETURN name');
EOF
# Expected: 2 rows.
```

The difference: scenario 1 references `c` which is multiple WITH-hops
behind and was dropped intentionally. Scenario 2 references `a` which
is exactly one WITH-hop behind (the input scope to this WITH).

## Target files

- `src/backend/transform/transform_with.c` — `validate_identifiers_in_scope()`
  and the `pre_var_names` snapshot infrastructure that was added and
  then dialed off in the Phase D session. Re-enable the pre-WITH-scope
  union for the validator. An earlier experiment regressed 21
  scenarios; investigate whether that was a different bug (scope
  tracking error rather than a fundamental semantic conflict).

## Expected delta

`+10` for the WithOrderBy2 [21–24] + WithOrderBy4 [11] + WithSkipLimit3
[3] scenarios that currently fail with "Variable `a` not defined".

The 30 WithOrderBy3 [8] scenarios must continue to pass — the
validator must still reject `c` when it's genuinely out of scope.

## Verification

```sh
angreal build extension
angreal test tck 2>&1 | grep "TCK \[ext"
# Expected: pass count rises by ~10 from the current 3109.

# Negative-test guard (these must still fail with UndefinedVariable):
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('WITH 1 AS a, 2 AS b, 3 AS c WITH a, b WITH a ORDER BY a, c RETURN a');
EOF
# Expected: SyntaxError, not 1 row.

# Positive-test guard:
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('CREATE ({name: ''A''}), ({name: ''C''})');
SELECT cypher('MATCH (a) WITH a.name AS name ORDER BY a.name + ''C'' LIMIT 1 RETURN name');
EOF
# Expected: 1 row.

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] All 10 currently-failing "Variable `a` not defined" scenarios
      flip to pass.
- [ ] All 30 WithOrderBy3 [8] scenarios (out-of-scope variable
      negative tests) stay passing.
- [ ] No other regressions.

## Status updates

*To be added during implementation*
