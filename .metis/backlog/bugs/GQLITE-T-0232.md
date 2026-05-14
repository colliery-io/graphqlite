---
id: tck-phase-c-list-map-function-arg
level: task
title: "[TCK] Phase C: List/Map function arg validation (~30 scenarios)"
short_code: "GQLITE-T-0232"
created_at: 2026-05-14T01:54:03.230856+00:00
updated_at: 2026-05-14T08:20:34.083533+00:00
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

# Phase C: List/Map function arg validation

## Source
Filed during [[GQLITE-T-0211]] negative-test triage of the [[GQLITE-I-0037]] baseline. See `docs/tck/baseline-2026-05-13.md` and the cluster table in [[GQLITE-T-0222]].

## Classification
- Type: bug
- Priority: P1
- Parent cluster: [[GQLITE-T-0222]] (extension accepts queries spec requires rejected)
- Expected scenarios unlocked: ~30 scenarios

## Why this matters

List functions (`range`, `head`, `tail`, `size`, `last`, `reverse`) and Map functions (`keys`, `values`, `properties`) expect specific input shapes. TCK has ~30 scenarios where these are called with wrong-shape inputs and expects `TypeError`. Extension currently accepts/coerces silently.

## Approach

In each function transformer, validate input shape at compile time. Reject `range('a', 'b')`, `head(1)`, `keys('not-a-map')`, etc. Same `TypeError` class as Phase B.

## Affected scenarios (clusters)

- List1 (~18)
- Map1-2 (~12)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Validation logic implemented in `src/backend/transform/` (likely a new file `transform_validate.c` plus wiring).
- [ ] Validation emits the openCypher-expected error class (`SyntaxError` or `TypeError`) with `InvalidArgumentType` / `InvalidArgumentValue` codes per TCK convention.
- [ ] No regression on existing passing scenarios.
- [ ] Baseline JSON regenerated and pass-count delta recorded in the closeout note.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).