---
id: s11-migrate-transform-func-string
level: task
title: "S11: Migrate transform_func_string/math/path/entity/aggregate/graph/dispatch.c to sql_builder"
short_code: "GQLITE-T-0265"
created_at: 2026-05-19T14:45:21.456043+00:00
updated_at: 2026-05-19T14:45:21.456043+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S11: Migrate transform_func_string/math/path/entity/aggregate/graph/dispatch.c to sql_builder

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Per-category Cypher function transforms. Each file in its own commit. Care with `transform_func_aggregate.c` since aggregation interacts with implicit GROUP BY emission already done by `sql_builder`.

## Acceptance Criteria

- [ ] No deprecated-API warnings in any of these seven files.
- [ ] TCK delta zero across all commits.

## Status Updates

*To be added during implementation*
