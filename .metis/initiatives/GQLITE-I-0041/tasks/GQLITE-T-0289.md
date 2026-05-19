---
id: c4-delete-shadow-transform
level: task
title: "C4: Delete shadow transform_internal.h after caller audit"
short_code: "GQLITE-T-0289"
created_at: 2026-05-19T14:48:36.932330+00:00
updated_at: 2026-05-19T14:48:36.932330+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C4: Delete shadow transform_internal.h after caller audit

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

`src/include/transform/transform_internal.h` (32 LOC) re-declares functions already declared in `cypher_transform.h`. Audit current includers, migrate any to `cypher_transform.h`, then delete.

## Acceptance Criteria

- [ ] `transform_internal.h` deleted.
- [ ] No `#include` of that header anywhere.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
