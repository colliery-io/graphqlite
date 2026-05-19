---
id: m1-move-handle-call-subquery
level: task
title: "M1: Move handle_call_subquery + helpers → executor_call_subquery.c"
short_code: "GQLITE-T-0274"
created_at: 2026-05-19T14:46:29.720353+00:00
updated_at: 2026-05-19T14:46:29.720353+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M1: Move handle_call_subquery + helpers → executor_call_subquery.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

`query_dispatch.c` lines ~3029–3829 (`handle_call_subquery` and its private helpers) carve out into a new `src/backend/executor/executor_call_subquery.c`. Headers updated; `#include` added at every former caller. Pure code move.

## Acceptance Criteria

- [ ] New file `executor_call_subquery.c` exists.
- [ ] `query_dispatch.c` is ~800 LOC smaller.
- [ ] `git mv` used where possible for blame preservation.
- [ ] `angreal test unit && angreal test functional && angreal test tck` clean, zero scenario delta.

## Status Updates

*To be added during implementation*
