---
id: s15-delete-pending-prop-joins
level: task
title: "S15: Delete pending_prop_joins fields + prepend_cte_to_sql helpers"
short_code: "GQLITE-T-0269"
created_at: 2026-05-19T14:45:51.068993+00:00
updated_at: 2026-05-19T14:45:51.068993+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S15: Delete pending_prop_joins fields + prepend_cte_to_sql helpers

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

After S14 has migrated all writes, delete `pending_prop_joins` / `pending_prop_joins_len` / `pending_prop_joins_cap` fields from `cypher_transform_context`, and the helpers `add_pending_prop_join`, `get_pending_prop_joins`, `reset_pending_prop_joins`, `prepend_cte_to_sql` from the transform module.

## Acceptance Criteria

- [ ] No `pending_prop_joins` references remain in src/.
- [ ] No `prepend_cte_to_sql` references remain.
- [ ] `cypher_transform_context` struct is one buffer smaller.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
