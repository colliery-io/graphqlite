---
id: tck-phase-c-list-map-function-arg
level: task
title: "[TCK] Phase C: List/Map function arg validation (~30 scenarios)"
short_code: "GQLITE-T-0232"
created_at: 2026-05-14T01:54:03.230856+00:00
updated_at: 2026-05-21T17:58:17.407977+00:00
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

- [x] Validation logic in `transform_validate.c` (`validate_list_map_call`).
- [x] Emits `TypeError: InvalidArgumentType: <fname>() does not accept argument of type <Type>`.
- [x] No regression — 944/944 unit, functional clean.
- [x] TCK pass at 3462.

## Status Updates

**2026-05-21** — Completed in commit c1ac2f3. `validate_list_map_call`
rejects:
- `head(1)`, `tail('s')`, `last(true)` — non-list scalar literal.
- `head({})`, `tail({})`, `last({})` — Map literal (List-only).
- `keys(1)`, `values('s')`, `properties(true)` — non-map scalar literal.
- `keys([1,2])`, `values([])` — List literal (Map-only).
- `range('a', 'b')`, `range(1.5, 5)` — non-integer literal in range().

Variable / parameter / property-access args bypass the static check
(runtime guards in transform_func_list / transform_func_aggregate
catch those). Remaining List1/Map1/Map2 fails are different bugs
(parameter-typed subscript, list comprehension result shape) — see
T-0301 and the I-0042 work queue.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).