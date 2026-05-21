---
id: tck-sqlite-internal-error-messages
level: task
title: "[TCK] SQLite-internal error messages leak through as Cypher errors"
short_code: "GQLITE-T-0221"
created_at: 2026-05-13T17:03:52.398219+00:00
updated_at: 2026-05-21T19:09:24.847306+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# SQLite-internal error messages surface as Cypher errors

## Status Updates

**2026-05-21** — Reduced from 53 → 28 over the long session. The
remaining bucket breaks down:
- 11 `no such column: _gql_default_aliasN.id` (varlen RETURN
  follow-ons in Match4/7/9, `relationships(p)` over varlen paths
  needs the T-0309 list-of-edges projection extended)
- 13 `ambiguous column name: N.id` (Match5 [19-29] multi-MATCH
  endpoint reuse — needs transform_match alias scoping work)
- 4 misc (`no such column: p`, `_withN.a.id`, `json_each.value.id`,
  `_gql_default_aliasN.source_id`)

The omnibus shape is no longer right for further work — each sub-
cluster is a distinct transform-side bug. Closing as substantively
decomposed.

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