---
id: dst-duration-across-transition
level: task
title: "DST: duration across a DST transition ignores the gained/lost hour"
short_code: "GQLITE-T-0342"
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

# DST: duration across a DST transition ignores the gained/lost hour

## Type
- [x] Bug

## Priority
- [ ] P2 - Medium (TCK conformance; blocks the "100% TCK" goal)

## Objective
`duration.inSeconds` / `duration.between` over an interval that crosses a DST
transition must account for the gained/lost hour (wall-clock vs elapsed time).
Closes **Temporal10 [8]** (8 examples).

## Impact Assessment
- **Affected:** openCypher TCK Temporal10 [8] "Should handle durations at daylight
  saving time day" — 8 example rows.
- **Reproduction:**
  ```cypher
  RETURN duration.inSeconds(
    datetime({year:2017, month:10, day:29, hour:0, timezone:'Europe/Stockholm'}),
    localdatetime({year:2017, month:10, day:29, hour:4})
  ) AS d
  ```
- **Expected vs Actual:** expected `PT5H` (2017-10-29 is the Stockholm fall-back
  day; 00:00->04:00 wall clock = 5 real hours because 03:00 falls back to 02:00);
  actual `PT4H` (flat UTC diff using a single offset, missing the extra hour).
  Also `... date({year:2017,month:10,day:30})` expects `PT25H`, we give `PT24H`.

## Root cause (shared — see also [[GQLITE-T-0343]] / [[GQLITE-T-0344]])
`apply_duration_to_temporal` / the temporal-diff helpers compute elapsed time with
ONE offset for the whole interval. Across a DST transition the offset changes
mid-interval, so the elapsed seconds differ from `end_utc - start_utc` computed
with a single offset. Correct handling needs the exact transition INSTANT for the
named zone, which requires real IANA tzdata (see T-0344). Implementation analysis
in [[GQLITE-T-0341]] (initiative [[GQLITE-I-0049]]).

## Acceptance Criteria
- [ ] Temporal10 [8] all 8 examples pass
- [ ] Zero TCK regressions (rigorous full pass-set diff); unit 944/944; functional clean

## Notes
Deferred from the +30 Temporal push (PR #88). Likely depends on the IANA-tzdata
work in [[GQLITE-T-0344]] (need transition instants, not just an offset-by-date).
