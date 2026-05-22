---
id: tck-extension-accepts-queries
level: task
title: "[TCK] Extension accepts queries openCypher requires to be rejected — 66 scenarios"
short_code: "GQLITE-T-0222"
created_at: 2026-05-13T17:03:53.541385+00:00
updated_at: 2026-05-21T19:07:03.387364+00:00
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

## Status Updates

**2026-05-21** — Substantively complete. Cluster reduced from 66 →
4 over the long open-work session. The 4 remaining scenarios each
need structural work outside compile-time validation:

- Pattern1 [24] — pattern in SET RHS (AST recognition of pattern-in-
  expression context).
- Aggregation6 [5] — `percentileDisc()` in pattern comprehension
  context (`[(n)-->() | 1]` not yet parsed).
- List1 [9] — parameter-typed subscript validation (runtime).
- TypeConversion3 [6] — runtime boolean subtype preservation through
  json_each of a list comprehension.

These are tracked in their respective parsing/runtime tickets.
Closing this omnibus.

Phase A-E validation work (T-0230 / T-0231 / T-0232 / T-0233 / T-0234)
delivered the bulk: AmbiguousAggregationExpression, NonConstantExpression
in aggregate, ORDER BY scoping + new-aggregate, Quantifier type
mismatch, WHERE bare-node Boolean check, conversion-fn arg validation,
list/map-fn arg validation, LIMIT/SKIP constant-expression check.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).