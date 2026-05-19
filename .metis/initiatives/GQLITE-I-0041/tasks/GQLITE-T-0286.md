---
id: c1-delete-stale-bak-files-under
level: task
title: "C1: Delete stale .bak files under src/backend/executor/"
short_code: "GQLITE-T-0286"
created_at: 2026-05-19T14:48:10.242361+00:00
updated_at: 2026-05-19T14:48:10.242361+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C1: Delete stale .bak files under src/backend/executor/

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

Three `.bak` files predate the `.gitignore` exclusion: `executor_set.c.bak`, `executor_remove.c.bak`, `executor_merge.c.bak`. `git rm` them in a single commit.

## Acceptance Criteria

- [ ] No `.bak` files remain under `src/`.
- [ ] Single small commit; no other changes.

## Status Updates

*To be added during implementation*
