---
id: m14-temporal-iso-week-math-off-by
level: task
title: "M14: Temporal ISO-week math off-by-one fix (unblocks ~10 TCK; depends on M8)"
short_code: "GQLITE-T-0299"
created_at: 2026-05-19T14:50:14.153268+00:00
updated_at: 2026-05-19T14:50:14.153268+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M14: Temporal ISO-week math off-by-one fix (unblocks ~10 TCK; depends on M8)

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Off-by-one in ISO week-date logic. Temporal1 [1]/[2]/[3] ("Should construct week date/localdatetime/datetime") all report e.g. `1817-01-07` expected vs `1817-01-08` actual at specific year boundaries. Once M8 has carved temporal transform code into its own file, the calendar math is easier to audit and fix in isolation. Likely a leap-year / year-boundary edge case in week-1 anchor selection.

## Acceptance Criteria

- [ ] Audit ISO-week computation in `transform_func_temporal.c` against the ISO 8601 spec.
- [ ] Temporal1 [1]/[2]/[3], Temporal2/Temporal3/Temporal6 week-construction examples pass.
- [ ] No regressions in non-week temporal scenarios.

## Status Updates

*To be added during implementation*
