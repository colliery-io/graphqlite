---
id: s5-migrate-transform-set-c
level: task
title: "S5: Migrate transform_set.c, transform_delete.c, transform_remove.c to sql_builder"
short_code: "GQLITE-T-0259"
created_at: 2026-05-19T14:44:22.937344+00:00
updated_at: 2026-05-19T14:44:22.937344+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S5: Migrate transform_set.c, transform_delete.c, transform_remove.c to sql_builder

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

First migration pass — smallest call-site counts, simple shapes. Each file in one commit. Verification: unit + functional + TCK clean, zero scenario delta.

## Acceptance Criteria

- [ ] No deprecated-API warnings in these three files.
- [ ] `angreal test unit && angreal test functional && angreal test tck` clean.
- [ ] TCK scenario set byte-identical to pre-migration baseline.

## Status Updates

*To be added during implementation*
