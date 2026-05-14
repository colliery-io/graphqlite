---
id: tck-phase-b-type-conversion
level: task
title: "[TCK] Phase B: Type-conversion function arg validation (~22 scenarios)"
short_code: "GQLITE-T-0231"
created_at: 2026-05-14T01:54:01.879521+00:00
updated_at: 2026-05-14T02:09:00.368063+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/active"


exit_criteria_met: false
initiative_id: NULL
---

# Phase B: Type-conversion function arg validation

## Source
Filed during [[GQLITE-T-0211]] negative-test triage of the [[GQLITE-I-0037]] baseline. See `docs/tck/baseline-2026-05-13.md` and the cluster table in [[GQLITE-T-0222]].

## Classification
- Type: bug
- Priority: P1
- Parent cluster: [[GQLITE-T-0222]] (extension accepts queries spec requires rejected)
- Expected scenarios unlocked: ~22 scenarios (depends on Phase A landing first to clear literal-type lifts)

## Why this matters

56 scenarios expect `TypeError` from `toInteger(...)`, `toFloat(...)`, `toBoolean(...)`, `toString(...)` when given malformed input. Extension currently runs these and returns whatever SQLite's CAST produces (often silently NULL or coerced).

## Approach

In the function-dispatch layer (`transform_func_*`), validate operand types at compile time when literal. Emit `TypeError: InvalidArgumentValue` for cases like `toInteger([1,2])`, `toFloat({...})`, `toBoolean(123)`. Runtime checks for variable inputs where static type isn't known.

## Affected scenarios (clusters)

- typeConversion 1-4 (~22)
- spillover from List1 (~18 of 56)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Validation logic implemented in `src/backend/transform/` (likely a new file `transform_validate.c` plus wiring).
- [ ] Validation emits the openCypher-expected error class (`SyntaxError` or `TypeError`) with `InvalidArgumentType` / `InvalidArgumentValue` codes per TCK convention.
- [ ] No regression on existing passing scenarios.
- [ ] Baseline JSON regenerated and pass-count delta recorded in the closeout note.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).