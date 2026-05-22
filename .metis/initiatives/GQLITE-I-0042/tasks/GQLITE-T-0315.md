---
id: e6-handle-match-remove-two-pass
level: task
title: "E6: handle_match_remove two-pass refactor"
short_code: "GQLITE-T-0315"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T12:30:17.839886+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0310]
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E6: handle_match_remove two-pass refactor

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

Same shape as E5 (T-0314) but for REMOVE. Pre-REMOVE MATCH captures
the var_map; REMOVE strips labels/properties via
`execute_remove_operations`; post-REMOVE RETURN projects from the
captured ids.

Synth-match-without-WHERE post-REMOVE RETURN already landed (commit
fe3db6d, iteration 30) and parallels the SET light fix.

## Blocked by

T-0310 — same DML/SELECT split architecture issue as T-0314.

## Acceptance Criteria

## Acceptance Criteria

- [ ] T-0310 landed.
- [ ] All Remove tests in TCK still pass (Remove1 through Remove3,
  Remove4 if applicable).
- [ ] Multi-row REMOVE+RETURN scenarios behave correctly (same
  category as Set6).

## Affected files

- `src/backend/executor/query_dispatch.c` — handle_match_remove
- `src/backend/transform/transform_remove.c` — emission goes through
  the new DML buffer
- `src/backend/executor/executor_remove.c` — already has
  `execute_remove_operations` with the canonical shape