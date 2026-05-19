---
id: s17-wire-transform-output
level: task
title: "S17: Wire transform_output lifecycle through context create/free"
short_code: "GQLITE-T-0271"
created_at: 2026-05-19T14:46:04.082057+00:00
updated_at: 2026-05-19T14:46:04.082057+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S17: Wire transform_output lifecycle through context create/free

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Allocate `transform_output` in `cypher_transform_create_context`; free it in `cypher_transform_free_context`. Memory ownership exclusively on the context — `transform_output` is never heap-shared.

## Acceptance Criteria

- [ ] No leaks under `angreal test unit` (verify with sanitizer build).
- [ ] No double-free / use-after-free in fast-path stress.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
