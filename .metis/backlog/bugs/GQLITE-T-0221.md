---
id: tck-sqlite-internal-error-messages
level: task
title: "[TCK] SQLite-internal error messages leak through as Cypher errors"
short_code: "GQLITE-T-0221"
created_at: 2026-05-13T17:03:52.398219+00:00
updated_at: 2026-05-13T17:03:52.398219+00:00
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

# SQLite-internal error messages surface as Cypher errors

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P2
- Affected TCK scenarios: 53

## Description

53 scenarios fail with errors like `ambiguous column name: n_2.id` or `no such column: _gql_default_alias_0.id` — these are SQLite complaints about the SQL the transform layer generated. They should never reach the user; either the transform is producing invalid SQL (bug) or the error path isn't catching and re-classifying SQLite errors as a categorised Cypher error.

Each of these is also a quality-of-error issue: a Cypher user has no way to act on `no such column: _gql_default_alias_0.id`.

## Affected feature files (top 15)

- `vendor/tck/features/expressions/graph/Graph8.feature` — 7 scenario(s)
- `vendor/tck/features/clauses/match/Match5.feature` — 5 scenario(s)
- `vendor/tck/features/clauses/match/Match9.feature` — 4 scenario(s)
- `vendor/tck/features/clauses/match/Match4.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/unwind/Unwind1.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean5.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/match/Match6.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/match/Match7.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean1.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean2.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/boolean/Boolean3.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/graph/Graph6.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/return/Return3.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/return/Return4.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/return/Return6.feature` — 1 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
