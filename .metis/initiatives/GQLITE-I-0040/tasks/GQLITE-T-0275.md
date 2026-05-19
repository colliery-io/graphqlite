---
id: m2-move-handle-merge-with-pipeline
level: task
title: "M2: Move handle_merge_with_pipeline + helpers → executor_merge_pipeline.c"
short_code: "GQLITE-T-0275"
created_at: 2026-05-19T14:46:37.114366+00:00
updated_at: 2026-05-19T14:46:37.114366+00:00
parent: GQLITE-I-0040
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0040
---

# M2: Move handle_merge_with_pipeline + helpers → executor_merge_pipeline.c

## Parent Initiative

[[GQLITE-I-0040]]

## Objective

`query_dispatch.c` lines ~1250–1633 (`handle_merge_with_pipeline` and its helpers) carve out into a new `src/backend/executor/executor_merge_pipeline.c`. Pure code move.

## Acceptance Criteria

- [ ] New file `executor_merge_pipeline.c` exists.
- [ ] `query_dispatch.c` is ~400 LOC smaller.
- [ ] TCK delta zero.

## Status Updates

*To be added during implementation*
