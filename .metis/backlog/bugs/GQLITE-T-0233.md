---
id: tck-phase-d-pattern-quantifier
level: task
title: "[TCK] Phase D: Pattern quantifier semantic validation (~20 scenarios)"
short_code: "GQLITE-T-0233"
created_at: 2026-05-14T01:54:04.578348+00:00
updated_at: 2026-05-14T09:18:33.596604+00:00
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

# Phase D: Pattern quantifier semantic validation

## Source
Filed during [[GQLITE-T-0211]] negative-test triage of the [[GQLITE-I-0037]] baseline. See `docs/tck/baseline-2026-05-13.md` and the cluster table in [[GQLITE-T-0222]].

## Classification
- Type: bug
- Priority: P1
- Parent cluster: [[GQLITE-T-0222]] (extension accepts queries spec requires rejected)
- Expected scenarios unlocked: ~20 scenarios

## Why this matters

~20 scenarios test that quantifier predicates `ANY/ALL/NONE/SINGLE` reject non-list operands, and that `EXISTS` rejects non-pattern operands. Today the parser accepts them and the executor produces (probably wrong) results.

## Approach

Extend the validation pass from Phase A to also check quantifier predicates' list-expression operand type. Reject `ANY(x IN 1 WHERE x > 0)` etc. at compile time.

## Affected scenarios (clusters)

- expressions/quantifier/* (~20)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Validation logic implemented in `src/backend/transform/` (likely a new file `transform_validate.c` plus wiring).
- [ ] Validation emits the openCypher-expected error class (`SyntaxError` or `TypeError`) with `InvalidArgumentType` / `InvalidArgumentValue` codes per TCK convention.
- [ ] No regression on existing passing scenarios.
- [ ] Baseline JSON regenerated and pass-count delta recorded in the closeout note.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).