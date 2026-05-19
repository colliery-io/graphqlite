---
id: s2-inventory-pending-prop-joins
level: task
title: "S2: Inventory pending_prop_joins writes and prepend_cte_to_sql invariant"
short_code: "GQLITE-T-0256"
created_at: 2026-05-19T14:43:56.062205+00:00
updated_at: 2026-05-19T14:43:56.062205+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S2: Inventory pending_prop_joins writes and prepend_cte_to_sql invariant

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Catalog every `pending_prop_joins` write (via `add_pending_prop_join`) and the single `prepend_cte_to_sql()` invocation. Document the prepend invariant — what gets spliced where, in what order, and what assumptions hold at finalize time. Extends the inventory doc from S1.

## Acceptance Criteria

- [ ] Inventory doc lists every `add_pending_prop_join` call site.
- [ ] Prepend invariant documented (order, position relative to CTEs from `sql_builder`, finalize ordering).

## Status Updates

*To be added during implementation*
