---
id: tck-extension-returns-rows-where
level: task
title: "[TCK] Extension returns rows where TCK expects empty result — 56 scenarios"
short_code: "GQLITE-T-0223"
created_at: 2026-05-13T17:03:54.696827+00:00
updated_at: 2026-05-13T17:03:54.696827+00:00
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

# Extension returns rows where TCK expects empty result

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P1
- Affected TCK scenarios: 56

## Description

56 scenarios expect a 0-row result; GraphQLite returns 1+ rows. Possible causes: (a) MATCH patterns are over-matching; (b) WHERE filters are not applied; (c) the extension's `cypher()` returns a status row even when the result set is empty, and the harness counts that as a data row (see [[GQLITE-T-0227]] — fix that first to confirm).

Re-evaluate this count after T-0227 lands.

## Affected feature files (top 15)

- `vendor/tck/features/clauses/create/Create2.feature` — 12 scenario(s)
- `vendor/tck/features/clauses/create/Create1.feature` — 8 scenario(s)
- `vendor/tck/features/clauses/create/Create3.feature` — 5 scenario(s)
- `vendor/tck/features/clauses/create/Create5.feature` — 5 scenario(s)
- `vendor/tck/features/clauses/delete/Delete1.feature` — 5 scenario(s)
- `vendor/tck/features/clauses/merge/Merge1.feature` — 4 scenario(s)
- `vendor/tck/features/clauses/merge/Merge6.feature` — 4 scenario(s)
- `vendor/tck/features/clauses/merge/Merge7.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/create/Create4.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/delete/Delete2.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/merge/Merge5.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/delete/Delete3.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge2.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge3.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge9.feature` — 1 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
