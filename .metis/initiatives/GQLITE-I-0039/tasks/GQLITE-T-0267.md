---
id: s13-add-typed-cte-prepend-section
level: task
title: "S13: Add typed CTE-prepend section to sql_builder API"
short_code: "GQLITE-T-0267"
created_at: 2026-05-19T14:45:37.924593+00:00
updated_at: 2026-05-19T14:45:37.924593+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S13: Add typed CTE-prepend section to sql_builder API

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Add a typed "pending CTE / prepend" clause section to `sql_builder` so `pending_prop_joins` has a target API to migrate into. Section needs the same prepend semantics as today's manual splicing — placed BEFORE any user-written CTEs, then the existing CTE section, then SELECT.

## Acceptance Criteria

- [ ] `sql_builder` has a new typed section for pre-CTE prepends (signature documented in sql_builder.h).
- [ ] Unit tests cover the prepend semantics + ordering relative to user CTEs.
- [ ] Existing callers unaffected (additive change).

## Status Updates

*To be added during implementation*
