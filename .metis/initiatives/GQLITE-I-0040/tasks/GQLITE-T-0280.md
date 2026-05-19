---
id: m7-move-graphqlite-register-helper
level: task
title: "M7: Move graphqlite_register_helper_udfs → runtime/udf_register.c; drop forward-decls"
short_code: "GQLITE-T-0280"
created_at: 2026-05-19T14:47:23.608446+00:00
updated_at: 2026-05-19T14:47:23.608446+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M7: Move graphqlite_register_helper_udfs → runtime/udf_register.c; drop forward-decls

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Move the registration function. Update `cypher_transform.c` and `cypher_executor.c` to depend on a new `runtime/udf_register.h` instead of `extern`-declaring the function from extension.c (Risk 5 fix).

## Acceptance Criteria

- [ ] `src/backend/runtime/udf_register.{c,h}` exist.
- [ ] No `extern int graphqlite_register_helper_udfs` declarations remain outside `runtime/`.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
