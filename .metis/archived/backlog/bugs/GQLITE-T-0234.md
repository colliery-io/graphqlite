---
id: tck-phase-e-remaining-negative
level: task
title: "[TCK] Phase E: Remaining negative-test validations (~30 scenarios)"
short_code: "GQLITE-T-0234"
created_at: 2026-05-14T01:54:05.762770+00:00
updated_at: 2026-05-21T17:58:31.525921+00:00
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

# Phase E: Remaining negative-test validations

## Source
Filed during [[GQLITE-T-0211]] negative-test triage of the [[GQLITE-I-0037]] baseline. See `docs/tck/baseline-2026-05-13.md` and the cluster table in [[GQLITE-T-0222]].

## Classification
- Type: bug
- Priority: P1
- Parent cluster: [[GQLITE-T-0222]] (extension accepts queries spec requires rejected)
- Expected scenarios unlocked: ~30 scenarios

## Why this matters

Smaller clusters that don't fit cleanly into A-D: 15 ArgumentError (wrong-arity calls), 2 SemanticError (scope violations like variable redefinition), plus the long tail.

## Approach

Per-cluster small fixes; expect each to be a 5-15 scenario unlock. Decompose into sub-tickets once A-D are landed and the remaining set is re-clustered.

## Affected scenarios (clusters)

- ReturnSkipLimit2 (9)
- misc Match (~10)
- misc semantic checks (~10)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] Validation logic in `transform_validate.c` — `validate_skip_limit`
  with `expr_has_var_reference` walker, plus the broader Phase A-D
  rules (AmbiguousAggregation, NonConstantExpression in agg, ORDER BY
  scoping, ORDER BY new-aggregate, WHERE Boolean check).
- [x] Emits the openCypher-expected error class (SyntaxError /
  TypeError) per TCK convention.
- [x] No regression — 944/944 unit, functional clean.
- [x] TCK pass at 3462 (was 3347 at I-0037 baseline; +115 net over
  the audit period).

## Status Updates

**2026-05-21** — Completed in commit a302058 (and the broader
validation work that landed across this session). Specifically:

- `validate_skip_limit` now allows constant-valued expressions like
  `LIMIT toInteger(ceil(1.7))` (ReturnSkipLimit2 [6]). Var-ref
  expressions still rejected as NonConstantExpression.

The remaining 4 expected-error fails (Aggregation6 [5], List1 [9],
Pattern1 [24], TypeConversion3 [6]) each need substantial structural
work (pattern comprehension grammar, parameter-typed subscript
validation, pattern-in-expression rejection, runtime boolean subtype
preservation) — out of scope for Phase E negative-test validation.

The "expected SyntaxError, none raised" bucket dropped from 25 at
session start to 1 (Pattern1 [24]) over the long session.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).