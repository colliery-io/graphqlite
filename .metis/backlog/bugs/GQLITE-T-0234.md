---
id: tck-phase-e-remaining-negative
level: task
title: "[TCK] Phase E: Remaining negative-test validations (~30 scenarios)"
short_code: "GQLITE-T-0234"
created_at: 2026-05-14T01:54:05.762770+00:00
updated_at: 2026-05-14T01:54:05.762770+00:00
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

- [ ] Validation logic implemented in `src/backend/transform/` (likely a new file `transform_validate.c` plus wiring).
- [ ] Validation emits the openCypher-expected error class (`SyntaxError` or `TypeError`) with `InvalidArgumentType` / `InvalidArgumentValue` codes per TCK convention.
- [ ] No regression on existing passing scenarios.
- [ ] Baseline JSON regenerated and pass-count delta recorded in the closeout note.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).
