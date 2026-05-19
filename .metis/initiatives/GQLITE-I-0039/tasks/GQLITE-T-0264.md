---
id: s10-migrate-transform-expr-ops-c
level: task
title: "S10: Migrate transform_expr_ops.c, transform_expr_predicate.c to sql_builder"
short_code: "GQLITE-T-0264"
created_at: 2026-05-19T14:45:13.767085+00:00
updated_at: 2026-05-19T14:45:13.767085+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S10: Migrate transform_expr_ops.c, transform_expr_predicate.c to sql_builder

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Expression-level transforms. These power BINARY_OP, NOT, NULL_CHECK, and predicate evaluation across every clause. Care needed around the `_gql_in`, `_gql_bool`, `_gql_eq` helper-UDF call patterns.

## Acceptance Criteria

- [ ] No deprecated-API warnings in either file.
- [ ] Three-valued logic test cases unchanged; `angreal test` triad clean.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
