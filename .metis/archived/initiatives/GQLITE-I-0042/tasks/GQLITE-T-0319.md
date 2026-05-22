---
id: e10-audit-bind-match-clause-into
level: task
title: "E10: audit bind_match_clause_into_varmap + similar helpers; collapse if possible"
short_code: "GQLITE-T-0319"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T13:57:01.623227+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E10: audit bind_match_clause_into_varmap + similar helpers; collapse if possible

## Status Updates

**2026-05-22 — audit complete; consolidation deferred.**

The canonical helper `bind_match_clause_into_varmap` (executor_match.c:805)
captures FIRST-row var_map bindings via transform + ID-only SELECT
+ prepare + step. As of T-0313 it handles both VAR_KIND_NODE and
VAR_KIND_EDGE.

**Latent duplicates** — functions that inline the same six-step
MATCH-transform-then-step pattern instead of reusing the helper:

- `execute_match_create_query` (executor_match.c:949)
- `execute_match_set_query` (executor_set.c:~390)
- `execute_match_remove_query` (executor_remove.c:~16)
- `execute_match_merge_query_with_varmap` (executor_merge.c:~975)

Each duplicates with subtle variations (single-row vs all-rows
iteration, what to do per row). A clean consolidation would replace
the inline blocks with a callback-based helper, but the per-row
callback signature would be wide (needs to cover SET / CREATE /
DELETE / MERGE / REMOVE semantics).

**Decision: defer.** TCK-neutral structural cleanup. Better tackled
when sql_builder fully replaces the `append_sql` scratchpad path
(post-I-0043) — at that point the helper can return structured
fragments instead of strings, making the variants easier to express
via composition rather than callbacks.

The audit findings are now documented in a code comment block above
`bind_match_clause_into_varmap` (executor_match.c) so future
sessions can find the duplicates listed in one place.

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

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

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