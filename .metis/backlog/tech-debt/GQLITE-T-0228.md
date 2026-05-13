---
id: tck-harness-close-remaining-90
level: task
title: "[TCK harness] Close remaining 90-scenario skip gap (step vocab + literal forms)"
short_code: "GQLITE-T-0228"
created_at: 2026-05-13T17:04:00.405884+00:00
updated_at: 2026-05-13T17:04:00.405884+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#tech-debt"


exit_criteria_met: false
initiative_id: NULL
---

# Close the remaining 90-scenario skip gap in the TCK harness

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: tech-debt
- Priority: P2
- Affected TCK scenarios: 90

## Description

90 scenarios in the baseline are `skipped` because the harness's Gherkin step dispatcher or openCypher literal parser doesn't recognise some form. Breakdown:

- unknown step: 58
- side-effects table: 17
- value-parse: 15

Not a correctness ticket — but every skipped scenario is a scenario we have no signal on. Closing the skip rate sharpens the conformance picture.

## Affected feature files (top 15)

- `vendor/tck/features/clauses/call/Call1.feature` — 14 scenario(s)
- `vendor/tck/features/expressions/temporal/Temporal4.feature` — 12 scenario(s)
- `vendor/tck/features/clauses/call/Call5.feature` — 8 scenario(s)
- `vendor/tck/features/clauses/delete/Delete5.feature` — 7 scenario(s)
- `vendor/tck/features/clauses/call/Call2.feature` — 6 scenario(s)
- `vendor/tck/features/clauses/call/Call3.feature` — 6 scenario(s)
- `vendor/tck/features/clauses/create/Create3.feature` — 4 scenario(s)
- `vendor/tck/features/clauses/call/Call6.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/return/Return2.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/graph/Graph3.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/map/Map3.feature` — 3 scenario(s)
- `vendor/tck/features/clauses/call/Call4.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/create/Create2.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/set/Set3.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/delete/Delete1.feature` — 1 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
