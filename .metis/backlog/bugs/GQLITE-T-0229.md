---
id: tck-with-order-by-conformance-area
level: task
title: "[TCK] WITH...ORDER BY conformance area — 70+ failures in WithOrderBy1/2"
short_code: "GQLITE-T-0229"
created_at: 2026-05-13T17:04:01.295168+00:00
updated_at: 2026-05-13T17:04:01.295168+00:00
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

# WITH … ORDER BY conformance — concentrated failure area

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P2
- Affected TCK scenarios: 94

## Description

`clauses/with-orderBy/` has 94 failures concentrated in WithOrderBy1.feature (45) and WithOrderBy2.feature (24). These exercise ORDER BY on projected expressions, ORDER BY with aggregations, ORDER BY on aliased columns, and tied-order determinism. Likely overlaps with [[GQLITE-T-0219]] (WITH preceding) and [[GQLITE-T-0227]] (result decoding) — pick this up AFTER those two land so the real semantic gaps are isolated.

## Affected feature files (top 4)

- `vendor/tck/features/clauses/with-orderBy/WithOrderBy1.feature` — 45 scenario(s)
- `vendor/tck/features/clauses/with-orderBy/WithOrderBy2.feature` — 24 scenario(s)
- `vendor/tck/features/clauses/with-orderBy/WithOrderBy4.feature` — 18 scenario(s)
- `vendor/tck/features/clauses/with-orderBy/WithOrderBy3.feature` — 7 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
