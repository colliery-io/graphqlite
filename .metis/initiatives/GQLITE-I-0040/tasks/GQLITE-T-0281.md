---
id: m8-move-temporal-transform-fns
level: task
title: "M8: Move temporal transform fns from transform_func_list.c → transform_func_temporal.c"
short_code: "GQLITE-T-0281"
created_at: 2026-05-19T14:47:30.255082+00:00
updated_at: 2026-05-19T14:47:30.255082+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M8: Move temporal transform fns → transform_func_temporal.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

From the 1,979-LOC `transform_func_list.c`, carve out date, time, datetime, localdatetime, duration, all `*_truncate_function`, `duration_between`, `duration_in`, `date_add`, `datetime_from_epoch` into a new `transform_func_temporal.c`.

## Acceptance Criteria

- [ ] `transform_func_temporal.c` exists.
- [ ] `transform_func_list.c` is ~500 LOC smaller.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
