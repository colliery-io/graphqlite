---
id: m6-move-regex-cypher-validate-udfs
level: task
title: "M6: Move regex + cypher_validate UDFs from extension.c → runtime/udf_misc.c"
short_code: "GQLITE-T-0279"
created_at: 2026-05-19T14:47:08.426631+00:00
updated_at: 2026-05-19T14:47:08.426631+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M6: Move regex + cypher_validate UDFs → runtime/udf_misc.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Move `regexp_func` and `cypher_validate_func` from `extension.c` into `src/backend/runtime/udf_misc.c`.

## Acceptance Criteria

- [ ] `src/backend/runtime/udf_misc.c` exists.
- [ ] `extension.c` no longer defines these.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
