---
id: m15-implement-percentilecont-and
level: task
title: "M15: Implement percentileCont() and percentileDisc() aggregates (unblocks 6 TCK; depends on M5)"
short_code: "GQLITE-T-0300"
created_at: 2026-05-19T14:50:22.803577+00:00
updated_at: 2026-05-19T14:50:22.803577+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M15: Implement percentileCont() and percentileDisc() aggregates (unblocks 6 TCK; depends on M5)

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Aggregation6 [1] (`percentileDisc`) and [2] (`percentileCont`) currently error with "is not yet fully supported in SQLite". Both are aggregates over a sorted numeric column with a 0..1 percentile parameter. Implement as SQLite aggregate functions in `runtime/udf_temporal.c` (or a new `udf_aggregate.c` if the temporal split feels wrong) once M5 is in place. Disc takes the value at floor(p*N), Cont linearly interpolates between adjacent sorted values.

## Acceptance Criteria

- [ ] `percentileDisc(numeric, percentile)` and `percentileCont(numeric, percentile)` registered as SQLite aggregates.
- [ ] Aggregation6 [1] and [2] all 6 example rows pass.
- [ ] Unit-test coverage for percentile values 0, 0.25, 0.5, 0.75, 1, plus single-element and empty-input edge cases.

## Status Updates

*To be added during implementation*
