---
id: s12-migrate-transform-func-list-c
level: task
title: "S12: Migrate transform_func_list.c to sql_builder (432 call sites — depends on I-0040 M8-M11)"
short_code: "GQLITE-T-0266"
created_at: 2026-05-19T14:45:29.948736+00:00
updated_at: 2026-05-19T14:45:29.948736+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S12: Migrate transform_func_list.c to sql_builder (432 call sites — depends on I-0040 M8-M11)

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Largest single file in the migration. Strongly preferred to do AFTER I-0040 M8-M11 splits this file into temporal/geo/json/typeconv/list_core (each ~500 LOC) — migrating four small files is reviewable; migrating one 1,979-LOC file is not. Each post-split file migrated in its own commit.

## Acceptance Criteria

- [ ] I-0040 M8-M11 completed first.
- [ ] No deprecated-API warnings in any of the post-split files.
- [ ] TCK delta zero across all commits.

## Status Updates

*To be added during implementation*
