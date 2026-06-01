---
id: dst-named-zone-offset-approximation
level: task
title: "DST: named-zone offset uses coarse month approximation, not historical rules"
short_code: "GQLITE-T-0343"
created_at: 2026-06-01T00:00:00.000000+00:00
updated_at: 2026-06-01T00:00:00.000000+00:00
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

# DST: named-zone offset uses coarse month approximation, not historical rules

## Type
- [x] Bug

## Priority
- [ ] P2 - Medium (TCK conformance; blocks "100% TCK")

## Objective
Resolve a named zone's UTC offset for a given date using the zone's actual DST
transition dates, including historical changes — not the current coarse
"April–September = summer" month heuristic. Closes **Temporal3 [10]** (2 remaining
tz-conversion examples) and is a prerequisite for clean DST elsewhere.

## Impact Assessment
- **Affected:** `named_tz_offset()` in `src/backend/runtime/udf_helpers.c`, used by
  datetime construction, parsing, `duration.between`, the `.offset`/`.epochSeconds`
  accessors, and time rendering.
- **Reproduction:**
  ```cypher
  RETURN datetime({year:1984, month:3, day:28, hour:12, timezone:'Europe/Stockholm'})
  ```
  Expected `1984-03-28T12:00+02:00[Europe/Stockholm]` (DST started 1984-03-25);
  the coarse month rule yields `+01:00` for March. (Direct construction was made
  to work; the SELECT/tz-conversion forms in Temporal3 [10] ex31/32 remain.)

## Root cause + the trap (CRITICAL)
The coarse heuristic is **load-bearing**. A naive "modern" last-Sunday-of-March..
October EU rule REGRESSED **-52 examples**, because the TCK uses HISTORICALLY
accurate IANA data: pre-1996 EU DST ended the last Sunday of SEPTEMBER (so
`1984-10-11 Stockholm = +01:00`, not `+02:00`). A regression-free improvement was
prototyped — historical end-month `(y>=1996)?Oct:Sep` + threading the real base
date into `_gql_tz_offset_for(tz, _gql_date_compose(... $.date, $.datetime))` — and
it fixes individual examples but flips no full scenario (so it was reverted to keep
PR #88 clean). Full correctness needs real IANA transition tables (see
[[GQLITE-T-0344]]). Detailed analysis: [[GQLITE-T-0341]] / [[GQLITE-I-0049]].

## Acceptance Criteria
- [ ] Temporal3 [10] all examples pass
- [ ] No regression in Temporal1 [10], Temporal3 [3]/[9]/[11] (the -52 trap)
- [ ] Rigorous full pass-set diff zero regressions; unit 944/944; functional clean

## Notes
Re-derive the regression-free foundation from the 2026-06-01 note in
[[GQLITE-T-0341]] when picking this up. Shares root cause with [[GQLITE-T-0342]],
[[GQLITE-T-0344]].
