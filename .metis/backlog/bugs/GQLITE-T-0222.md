---
id: tck-extension-accepts-queries
level: task
title: "[TCK] Extension accepts queries openCypher requires to be rejected — 66 scenarios"
short_code: "GQLITE-T-0222"
created_at: 2026-05-13T17:03:53.541385+00:00
updated_at: 2026-05-13T17:03:53.541385+00:00
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

# Extension accepts queries that openCypher requires to be rejected

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P1
- Affected TCK scenarios: 66

## Description

66 TCK scenarios are NEGATIVE tests — they describe Cypher that should produce a categorised error (TypeError, SyntaxError, EntityNotFound, ConstraintVerificationFailed, etc.). GraphQLite runs them and returns a (possibly wrong) result instead. The extension is more permissive than the spec.

This is a correctness ticket: silent acceptance of invalid queries is worse than rejecting valid ones, because users can't tell when they've written something nonsensical.

## Affected feature files (top 15)

- `vendor/tck/features/clauses/return-skip-limit/ReturnSkipLimit2.feature` — 9 scenario(s)
- `vendor/tck/features/clauses/create/Create1.feature` — 8 scenario(s)
- `vendor/tck/features/clauses/merge/Merge5.feature` — 7 scenario(s)
- `vendor/tck/features/clauses/return-skip-limit/ReturnSkipLimit1.feature` — 7 scenario(s)
- `vendor/tck/features/clauses/create/Create2.feature` — 4 scenario(s)
- `vendor/tck/features/clauses/return/Return6.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/pattern/Pattern1.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/merge/Merge1.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/return-orderby/ReturnOrderBy6.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/set/Set1.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/union/Union3.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/with-orderBy/WithOrderBy4.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/literals/Literals2.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/literals/Literals3.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/path/Path3.feature` — 2 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
