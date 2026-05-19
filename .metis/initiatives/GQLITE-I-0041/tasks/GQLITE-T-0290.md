---
id: c5-create-per-file-headers-for
level: task
title: "C5: Create per-file headers for each transform_func_*.c"
short_code: "GQLITE-T-0290"
created_at: 2026-05-19T14:48:42.992138+00:00
updated_at: 2026-05-19T14:48:42.992138+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C5: Create per-file headers for each transform_func_*.c

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

One header per `.c`: `transform_func_string.h`, `transform_func_math.h`, `transform_func_path.h`, `transform_func_entity.h`, `transform_func_aggregate.h`, `transform_func_graph.h`, `transform_func_dispatch.h`, `transform_func_list.h` — plus any new files introduced by I-0040 Phase 3 (`temporal`, `geo`, `json`, `typeconv`). Each header contains only decls for its paired `.c`. Best run AFTER I-0040 M8-M11 lands.

## Acceptance Criteria

- [ ] Every `transform_func_*.c` has a same-named `.h` with only its own decls.
- [ ] Per-file headers compile in isolation (no transitive include-order surprises).

## Status Updates

*To be added during implementation*
