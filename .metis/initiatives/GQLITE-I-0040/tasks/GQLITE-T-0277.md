---
id: m4-move-helper-udfs-gql-bool-gql
level: task
title: "M4: Move helper UDFs (_gql_bool, _gql_in, etc.) from extension.c → runtime/udf_core.c"
short_code: "GQLITE-T-0277"
created_at: 2026-05-19T14:46:50.480198+00:00
updated_at: 2026-05-19T14:46:50.480198+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M4: Move helper UDFs from extension.c → runtime/udf_core.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

Create `src/backend/runtime/` directory and move: `_gql_bool`, `_gql_bool_str`, `_gql_to_bool_strict`, `_gql_to_int_strict`, `_gql_to_float_strict`, `_gql_to_string_strict`, `_gql_in`, `_gql_eq`, `_gql_subscript`, `_gql_order_key`, `_gql_extract_ns`, `_gql_dyn_add`, `_gql_dyn_sub`. Makefile updated to compile the new TU.

## Acceptance Criteria

- [ ] `src/backend/runtime/udf_core.c` exists with the named UDFs.
- [ ] `extension.c` no longer defines them.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
