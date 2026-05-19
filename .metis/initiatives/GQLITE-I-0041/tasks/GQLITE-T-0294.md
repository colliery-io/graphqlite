---
id: c9-migrate-every-merge-variant
level: task
title: "C9: Migrate every MERGE variant caller to canonical execute_merge_clause"
short_code: "GQLITE-T-0294"
created_at: 2026-05-19T14:49:12.739076+00:00
updated_at: 2026-05-19T14:49:12.739076+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C9: Migrate every MERGE variant caller to canonical execute_merge_clause

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

Update every caller of `execute_merge_clause`, `execute_merge_clause_with_vars`, `execute_merge_clause_with_varmap`, `execute_merge_clause_with_vars_ex`, `execute_merge_with_variables` to the canonical signature from C8.

## Acceptance Criteria

- [ ] Every caller compiles against the canonical signature.
- [ ] The five variant function bodies still exist (deletion happens in C10).
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
