---
id: tck-false-with-clause-requires-a
level: task
title: "[TCK] False "WITH clause requires a preceding MATCH" error — 69 scenarios"
short_code: "GQLITE-T-0219"
created_at: 2026-05-13T17:03:50.207288+00:00
updated_at: 2026-05-13T17:03:50.207288+00:00
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

# Parser/transform incorrectly demands a MATCH before every WITH clause

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: bug
- Priority: P1
- Affected TCK scenarios: 69

## Description

69 TCK scenarios fail because GraphQLite raises `WITH clause requires a preceding MATCH` (code: EXECUTION_ERROR) on Cypher queries that openCypher allows. WITH after CREATE, UNWIND, CALL, or as the leading clause (e.g. `WITH 1 AS n RETURN n`) is legal openCypher.

This is the single biggest error cluster after parse errors. Likely diagnoses: (a) the transform layer is gated on a MATCH AST node existing in the current scope; (b) the parser is allowed forms but the dispatcher rejects them.

## Affected feature files (top 15)

- `vendor/tck/features/expressions/list/List2.feature` — 10 scenario(s)
- `vendor/tck/features/expressions/temporal/Temporal6.feature` — 6 scenario(s)
- `vendor/tck/features/expressions/temporal/Temporal8.feature` — 5 scenario(s)
- `vendor/tck/features/expressions/typeConversion/TypeConversion2.feature` — 5 scenario(s)
- `vendor/tck/features/clauses/unwind/Unwind1.feature` — 4 scenario(s)
- `vendor/tck/features/expressions/map/Map2.feature` — 4 scenario(s)
- `vendor/tck/features/expressions/typeConversion/TypeConversion3.feature` — 4 scenario(s)
- `vendor/tck/features/expressions/comparison/Comparison1.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/list/List1.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/map/Map1.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier10.feature` — 3 scenario(s)
- `vendor/tck/features/expressions/list/List5.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier11.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier12.feature` — 2 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier9.feature` — 2 scenario(s)

## Reproduction

```sql
.load ./build/graphqlite
SELECT cypher('WITH 1 AS x RETURN x');
-- expected: rows=[{x: 1}]
-- actual: error WITH clause requires a preceding MATCH
```

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
