---
id: s8-migrate-transform-with-c-to-sql
level: task
title: "S8: Migrate transform_with.c to sql_builder"
short_code: "GQLITE-T-0262"
created_at: 2026-05-19T14:44:56.522270+00:00
updated_at: 2026-05-19T14:44:56.522270+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S8: Migrate transform_with.c to sql_builder

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Migrate the WITH transform. Coordinates with `transform_match.c` (S7) since WITH-scoped variable handoff shares state with MATCH alias bookkeeping.

## Acceptance Criteria

- [ ] No deprecated-API warnings in transform_with.c.
- [ ] `angreal test` triad clean, TCK delta zero.

## Status Updates

*To be added during implementation*
