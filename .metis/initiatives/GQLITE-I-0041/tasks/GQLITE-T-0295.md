---
id: c10-delete-deprecated-merge
level: task
title: "C10: Delete deprecated MERGE variant function bodies + decls"
short_code: "GQLITE-T-0295"
created_at: 2026-05-19T14:49:19.073156+00:00
updated_at: 2026-05-19T14:49:19.073156+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C10: Delete deprecated MERGE variant function bodies + decls

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

After C9 has shifted every caller, delete the four redundant MERGE variants and their header declarations.

## Acceptance Criteria

- [ ] Exactly one `execute_merge_clause` symbol remains in `executor_internal.h`.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
