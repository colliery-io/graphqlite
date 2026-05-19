---
id: s19-full-tck-regression-baseline
level: task
title: "S19: Full TCK regression baseline diff — verify byte-identical pass set"
short_code: "GQLITE-T-0273"
created_at: 2026-05-19T14:46:18.108953+00:00
updated_at: 2026-05-19T14:46:18.108953+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S19: Full TCK regression baseline diff — verify byte-identical pass set

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Capture `build/tck-results.json` immediately before S1 lands; compare against the post-S18 result. Pass scenario set must be byte-identical (or strictly larger if S7 unblocked the var-length / OPTIONAL MATCH classes). Closing this task closes the initiative.

## Acceptance Criteria

- [ ] Pre-initiative baseline saved.
- [ ] Post-initiative diff produces zero scenario regressions (and the gains from S7 are accounted for in the comparison).

## Status Updates

*To be added during implementation*
