---
id: m11-move-type-conversion-transform
level: task
title: "M11: Move type-conversion transform fns (toString, toInteger, etc.) → transform_func_typeconv.c"
short_code: "GQLITE-T-0284"
created_at: 2026-05-19T14:47:55.024731+00:00
updated_at: 2026-05-19T14:47:55.024731+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M11: Move type-conversion transform fns → transform_func_typeconv.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Carve out `tostring_function`, `type_conversion_function`, `type_conversion_ornull_function`, `valuetype_function`, `nullif_function`.

## Acceptance Criteria

- [ ] `transform_func_typeconv.c` exists.
- [ ] `transform_func_list.c` ends near ~600 LOC (only true list operators remain).
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
