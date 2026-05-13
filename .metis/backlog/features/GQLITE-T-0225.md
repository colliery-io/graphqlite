---
id: tck-explicit-not-yet-implemented
level: task
title: "[TCK] Explicit "not yet implemented" surfaces — 21 scenarios"
short_code: "GQLITE-T-0225"
created_at: 2026-05-13T17:03:57.017550+00:00
updated_at: 2026-05-13T17:03:57.017550+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
initiative_id: NULL
---

# Explicit 'not yet implemented' / 'not yet supported' surfaces

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: feature
- Priority: P2
- Affected TCK scenarios: 38

## Description

38 scenarios fail because the extension explicitly returns `NOT_IMPLEMENTED` or `not yet supported` for features it knows it lacks. The biggest sub-clusters today are 'RETURN after CREATE' (10 scenarios) and 'Complex expressions in WITH' (11 scenarios). These are honest gaps and good candidate work for prioritisation against the worth they unlock in the TCK pass-rate.

## Affected feature files (top 15)

- `vendor/tck/features/clauses/create/Create6.feature` — 10 scenario(s)
- `vendor/tck/features/clauses/delete/Delete5.feature` — 7 scenario(s)
- `vendor/tck/features/clauses/create/Create3.feature` — 4 scenario(s)
- `vendor/tck/features/expressions/list/List12.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/create/Create2.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/match/Match9.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/graph/Graph6.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/delete/Delete2.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/match/Match4.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge4.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge6.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge7.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/with/With5.feature` — 1 scenario(s)
- `vendor/tck/features/expressions/comparison/Comparison2.feature` — 1 scenario(s)
- `vendor/tck/features/expressions/graph/Graph4.feature` — 1 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
