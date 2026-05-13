---
id: tck-failed-to-transform-return
level: task
title: "[TCK] "Failed to transform RETURN clause" — 17 scenarios"
short_code: "GQLITE-T-0226"
created_at: 2026-05-13T17:03:58.116846+00:00
updated_at: 2026-05-13T17:03:58.116846+00:00
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

# Generic `Failed to transform RETURN clause` — needs sub-investigation

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P2
- Affected TCK scenarios: 17

## Description

17 scenarios fail with the opaque error `Failed to transform RETURN clause` (code: EXECUTION_ERROR). Two problems:

1. The error is uninformative — a user can't tell which expression in their RETURN broke the transform.
2. Some subset of the inputs are valid openCypher the transform layer doesn't handle yet.

Fix the error message first (cheap quality improvement), then sub-cluster the actual transform gaps.

## Affected feature files (top 8)

- `vendor/tck/features/clauses/merge/Merge5.feature` — 9 scenario(s)
- `vendor/tck/features/expressions/aggregation/Aggregation6.feature` — 2 scenario(s)
- `vendor/tck/features/clauses/merge/Merge1.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge6.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge7.feature` — 1 scenario(s)
- `vendor/tck/features/clauses/merge/Merge8.feature` — 1 scenario(s)
- `vendor/tck/features/expressions/list/List12.feature` — 1 scenario(s)
- `vendor/tck/features/expressions/pattern/Pattern2.feature` — 1 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
