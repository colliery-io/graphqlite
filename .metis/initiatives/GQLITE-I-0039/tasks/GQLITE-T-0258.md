---
id: s4-deprecate-append-sql-trio-with
level: task
title: "S4: Deprecate append_sql trio with __attribute__((deprecated)); add gap-filling sql_builder APIs"
short_code: "GQLITE-T-0258"
created_at: 2026-05-19T14:44:10.804375+00:00
updated_at: 2026-05-19T14:44:10.804375+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S4: Deprecate append_sql trio + add gap-filling sql_builder APIs

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Add `__attribute__((deprecated("use sql_builder")))` to `append_sql`, `append_identifier`, `append_string_literal` so all new uses raise compile-time warnings. Implement the gap-filling APIs identified in S3. This is the bridge phase — both APIs coexist but new code is steered to sql_builder.

## Acceptance Criteria

- [ ] Three functions carry the `deprecated` attribute.
- [ ] Build still succeeds (existing call sites are still legal, just warn).
- [ ] Gap-filling APIs from S3 implemented + unit-tested.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
