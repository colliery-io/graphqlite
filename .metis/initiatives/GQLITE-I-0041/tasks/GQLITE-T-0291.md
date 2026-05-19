---
id: c6-update-includes-replace
level: task
title: "C6: Update includes — replace transform_functions.h with per-file headers"
short_code: "GQLITE-T-0291"
created_at: 2026-05-19T14:48:52.407705+00:00
updated_at: 2026-05-19T14:48:52.407705+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C6: Update includes — replace transform_functions.h with per-file headers

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

Audit every `#include "transform/transform_functions.h"` and rewrite to include the per-file headers actually needed by that translation unit.

## Acceptance Criteria

- [ ] No `#include "transform/transform_functions.h"` remains in src/.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
