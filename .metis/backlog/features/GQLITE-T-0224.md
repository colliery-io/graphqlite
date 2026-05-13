---
id: tck-unsupported-opencypher-syntax
level: task
title: "[TCK] Unsupported openCypher syntax — 303 parse errors (omnibus)"
short_code: "GQLITE-T-0224"
created_at: 2026-05-13T17:03:55.828485+00:00
updated_at: 2026-05-13T17:03:55.828485+00:00
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

# Parser rejects 366 legal openCypher syntaxes — omnibus feature ticket

## Source
Filed during [[GQLITE-T-0211]] triage of the [[GQLITE-I-0037]] baseline run. See `docs/tck/baseline-2026-05-13.md`.

## Classification
- Type: feature
- Priority: P2
- Affected TCK scenarios: 366

## Description

366 TCK scenarios fail with `PARSE_ERROR`. The parser does not accept openCypher forms the spec requires. This is filed as a single umbrella for tracking; sub-tickets will be cut as each cluster is investigated and worked.

**Top 'unexpected token' triggers** (parser bailout point):
- `<'","code":"PARSE_ERROR"}` — 168 scenarios
- `IDENTIFIER` — 20 scenarios
- `(` — 18 scenarios
- `.'","code":"PARSE_ERROR"}` — 14 scenarios
- `SINGLE` — 9 scenarios
- `{` — 9 scenarios
- `|` — 8 scenarios
- `LIMIT","code":"PARSE_ERROR"}` — 6 scenarios
- `END_P` — 5 scenarios
- `>` — 4 scenarios
- `EXISTS` — 4 scenarios
- `)'","code":"PARSE_ERROR"}` — 3 scenarios
- `^` — 3 scenarios
- `<` — 3 scenarios
- `]` — 2 scenarios

Many of these are likely: list comprehensions and predicates (`SINGLE`, `ALL`, `ANY`, `NONE`, `EXTRACT`), pattern comprehensions (`[(...)|...]`), parameter syntax, label disjunction (`:A|B`) — items already partially tracked by other initiatives.

## Affected feature files (top 15)

- `vendor/tck/features/clauses/with-orderBy/WithOrderBy2.feature` — 24 scenario(s)
- `vendor/tck/features/clauses/with-orderBy/WithOrderBy1.feature` — 23 scenario(s)
- `vendor/tck/features/expressions/literals/Literals5.feature` — 20 scenario(s)
- `vendor/tck/features/expressions/temporal/Temporal1.feature` — 13 scenario(s)
- `vendor/tck/features/expressions/temporal/Temporal10.feature` — 13 scenario(s)
- `vendor/tck/features/clauses/match/Match7.feature` — 11 scenario(s)
- `vendor/tck/features/expressions/temporal/Temporal3.feature` — 11 scenario(s)
- `vendor/tck/features/clauses/return-skip-limit/ReturnSkipLimit2.feature` — 9 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier1.feature` — 9 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier2.feature` — 9 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier3.feature` — 9 scenario(s)
- `vendor/tck/features/expressions/quantifier/Quantifier4.feature` — 9 scenario(s)
- `vendor/tck/features/clauses/create/Create1.feature` — 8 scenario(s)
- `vendor/tck/features/expressions/literals/Literals4.feature` — 8 scenario(s)
- `vendor/tck/features/expressions/pattern/Pattern2.feature` — 8 scenario(s)

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
