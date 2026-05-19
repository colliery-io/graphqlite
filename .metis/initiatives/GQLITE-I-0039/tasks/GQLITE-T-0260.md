---
id: s6-migrate-transform-create-c
level: task
title: "S6: Migrate transform_create.c, transform_foreach.c, transform_unwind.c to sql_builder"
short_code: "GQLITE-T-0260"
created_at: 2026-05-19T14:44:33.503726+00:00
updated_at: 2026-05-19T14:44:33.503726+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S6: Migrate transform_create.c, transform_foreach.c, transform_unwind.c to sql_builder

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Second migration pass — medium call-site counts. One commit per file. Same verification protocol as S5.

## Acceptance Criteria

- [ ] No deprecated-API warnings in these three files.
- [ ] `angreal test unit && angreal test functional && angreal test tck` clean, zero scenario delta.

## Status Updates

*To be added during implementation*
