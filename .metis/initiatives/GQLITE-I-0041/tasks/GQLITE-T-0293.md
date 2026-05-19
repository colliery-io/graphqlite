---
id: c8-design-canonical-execute-merge
level: task
title: "C8: Design canonical execute_merge_clause signature; document in commit message"
short_code: "GQLITE-T-0293"
created_at: 2026-05-19T14:49:07.195492+00:00
updated_at: 2026-05-19T14:49:07.195492+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C8: Design canonical execute_merge_clause signature; document in commit message

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

Pick the canonical signature. Per initiative: `int execute_merge_clause(cypher_executor *executor, cypher_merge *merge, cypher_result *result, variable_map *in_vars, variable_map **out_vars);` — caller passes NULL for unused slots. Document the design and the migration plan for the five existing variants in the commit message that introduces the canonical signature.

## Acceptance Criteria

- [ ] Canonical signature lives in `executor_internal.h` (or successor header).
- [ ] Commit message captures the decision rationale and call-site count from each existing variant.
- [ ] No code migrated yet (that's C9); this task is design-only.

## Status Updates

*To be added during implementation*
