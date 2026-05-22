---
id: e10-audit-bind-match-clause-into
level: task
title: "E10: audit bind_match_clause_into_varmap + similar helpers; collapse if possible"
short_code: "GQLITE-T-0319"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T00:12:47.008348+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"

exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E10: audit bind_match_clause_into_varmap + similar helpers; collapse if possible

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

Once E4-E8 have converged the handlers on the two-pass shape, audit
`bind_match_clause_into_varmap` and its near-cousins (the inline
MATCH-then-iterate logic in execute_match_set_query, execute_match_
delete_query, execute_call_subquery's inner MATCH handling). Collapse
into a single canonical helper if the shapes have converged.

This is a cleanup task — TCK-neutral, code-clarity positive.

## Acceptance Criteria

- [ ] Cross-reference every `bind_match_clause_into_varmap` caller
  and every inline MATCH-execute-iterate site.
- [ ] If shapes have converged: collapse to a single helper. If not:
  document remaining differences in this task's status.
- [ ] No TCK regression.
- [ ] 944/944 unit clean.

## Affected files (audit scope)

- `src/backend/executor/executor_match.c`
- `src/backend/executor/executor_set.c`
- `src/backend/executor/executor_delete.c`
- `src/backend/executor/executor_remove.c`
- `src/backend/executor/executor_call_subquery.c`
- `src/backend/executor/executor_merge_pipeline.c`
- `src/backend/executor/query_dispatch.c`

## Notes

Tracks I-0042 goal G1 (handler-shape consistency). Safe to take in
isolation — does not need T-0310 / E2-E8.
