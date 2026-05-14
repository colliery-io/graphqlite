---
id: tck-phase-a-compile-time-literal
level: task
title: "[TCK] Phase A: Compile-time literal-type validation pass (~150 scenarios)"
short_code: "GQLITE-T-0230"
created_at: 2026-05-14T01:54:01.015759+00:00
updated_at: 2026-05-14T02:06:21.890442+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Phase A: Compile-time literal-type validation pass

## Source
Filed during [[GQLITE-T-0211]] negative-test triage of the [[GQLITE-I-0037]] baseline. See `docs/tck/baseline-2026-05-13.md` and the cluster table in [[GQLITE-T-0222]].

## Classification
- Type: bug
- Priority: P1
- Parent cluster: [[GQLITE-T-0222]] (extension accepts queries spec requires rejected)
- Expected scenarios unlocked: ~150 scenarios

## Why this matters

The biggest negative-test cluster (331 of 404 "expected error, none raised" scenarios) — TCK calls them `SyntaxError: InvalidArgumentType`, but they're queries like `RETURN NOT 1`, `RETURN 'a' AND true`, `RETURN 1 OR null`, `RETURN [1] = true`, etc. Our parser accepts these because the grammar allows any expression in NOT/AND/OR operand position. openCypher requires them rejected at compile time.

## Approach

New transform-layer pass `transform_validate_types` that runs after the AST is built but before SQL generation. Walks the AST and emits `SyntaxError: InvalidArgumentType` when:

- `NOT <e>` where `<e>` is a literal of non-boolean type (and not NULL).
- `<e1> AND/OR/XOR <e2>` where either operand is a non-boolean literal.
- `<e1> = <e2>` (and `<>`, `<`, `>`, `<=`, `>=`) on heterogeneous literals where openCypher requires the same type family.

Variables and expressions whose type cannot be statically determined are skipped (no false-positive validation).

## Affected scenarios (clusters)

- Boolean1-4 (~116)
- WithOrderBy 1-3 (~50)
- Pattern1 (~17)
- Match1 negative-tests (~24)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Validation logic implemented in `src/backend/transform/` (likely a new file `transform_validate.c` plus wiring).
- [ ] Validation emits the openCypher-expected error class (`SyntaxError` or `TypeError`) with `InvalidArgumentType` / `InvalidArgumentValue` codes per TCK convention.
- [ ] No regression on existing passing scenarios.
- [ ] Baseline JSON regenerated and pass-count delta recorded in the closeout note.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).