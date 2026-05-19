---
id: s9-migrate-transform-return-c-to
level: task
title: "S9: Migrate transform_return.c to sql_builder (104 call sites)"
short_code: "GQLITE-T-0263"
created_at: 2026-05-19T14:45:05.777162+00:00
updated_at: 2026-05-19T14:45:05.777162+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S9: Migrate transform_return.c to sql_builder (104 call sites)

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Largest single non-function file. RETURN handles aggregates, ORDER BY, SKIP/LIMIT — all of which already partially use `sql_builder`. This task finishes the migration so RETURN no longer mixes APIs.

## Acceptance Criteria

- [ ] No deprecated-API warnings in transform_return.c.
- [ ] Aggregation, ORDER BY, SKIP/LIMIT, DISTINCT all still pass their TCK suites.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
