---
id: s18-delete-append-sql-trio
level: task
title: "S18: Delete append_sql trio definitions + sql_buffer fields from context"
short_code: "GQLITE-T-0272"
created_at: 2026-05-19T14:46:10.600782+00:00
updated_at: 2026-05-19T14:46:10.600782+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S18: Delete append_sql trio + sql_buffer fields from context

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Final cleanup. Delete `append_sql`, `append_identifier`, `append_string_literal` function bodies from `cypher_transform.c` and declarations from `cypher_transform.h`. Delete `sql_buffer`/`sql_size`/`sql_capacity` from `cypher_transform_context`. After this commit, new code physically cannot use the legacy API.

## Acceptance Criteria

- [ ] `grep -r 'append_sql\\b\\|append_identifier\\b\\|append_string_literal\\b' src/backend/` returns no matches.
- [ ] `grep -r 'sql_buffer\\b' src/backend/transform/` returns no matches.
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
