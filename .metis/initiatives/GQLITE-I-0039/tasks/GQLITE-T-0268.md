---
id: s14-migrate-add-pending-prop-join
level: task
title: "S14: Migrate add_pending_prop_join callers to sql_builder CTE-prepend"
short_code: "GQLITE-T-0268"
created_at: 2026-05-19T14:45:44.941646+00:00
updated_at: 2026-05-19T14:45:44.941646+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S14: Migrate add_pending_prop_join callers to sql_builder CTE-prepend

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Update every `add_pending_prop_join` call site to use the new `sql_builder` API from S13. After this task, `pending_prop_joins` is read-only on the context (writes go through the builder).

## Acceptance Criteria

- [ ] All `add_pending_prop_join` call sites migrated.
- [ ] `get_pending_prop_joins` returns NULL / empty for every test.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
