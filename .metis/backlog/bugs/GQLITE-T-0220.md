---
id: tck-false-unknown-variable-in
level: task
title: "[TCK] False "Unknown variable" in legal Cypher scopes — 43 scenarios"
short_code: "GQLITE-T-0220"
created_at: 2026-05-13T17:03:51.247821+00:00
updated_at: 2026-05-13T17:03:51.247821+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#bug"


exit_criteria_met: false
initiative_id: NULL
---

# Variable scoping resolver rejects legal Cypher bindings

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P1
- Affected TCK scenarios: 49

## Description

49 scenarios surface `Unknown variable: a` (or `r`, etc.) on Cypher that defines and uses the variable in the same query. Most likely a scoping issue in the transform layer: variables bound by an earlier clause (`CREATE`, `WITH AS`, list comprehension) are not visible to subsequent clauses in some configurations.

Suspected interplay with [[GQLITE-T-0219]] — fixing the WITH gating may uncover or unmask scoping bugs.

## Affected feature files (top 10)

- `vendor/tck/features/useCases/triadicSelection/TriadicSelection1.feature` — 18 scenario(s)
- `vendor/tck/features/expressions/precedence/Precedence1.feature` — 12 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean5.feature` — 5 scenario(s)
- `vendor/tck/features/clauses/with-where/WithWhere1.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/with-where/WithWhere7.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean1.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean2.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean3.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/precedence/Precedence2.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/precedence/Precedence3.feature` — 1 scenario(s)

## Reproduction

Check scenarios in `vendor/tck/features/clauses/with/` — many define a variable in `WITH ... AS a` and then reference `a` in the following RETURN/WHERE.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
