---
id: e9-full-tck-regression-gate-after
level: task
title: "E9: full TCK regression gate after E2-E8 land"
short_code: "GQLITE-T-0318"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T00:12:47.008348+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0311, GQLITE-T-0312, GQLITE-T-0313, GQLITE-T-0314, GQLITE-T-0315, GQLITE-T-0316, GQLITE-T-0317]
archived: false

tags:
  - "#task"
  - "#phase/todo"

exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E9: full TCK regression gate after E2-E8 land

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

After E2-E8 are all landed, run a full TCK regression and verify the
pass count is at or above the I-0042 start baseline. If anything
regressed, file follow-ons before transitioning I-0042 to completed.

## Acceptance Criteria

- [ ] All blocking E-tasks (T-0311 through T-0317) completed and
  archived.
- [ ] TCK pass count ≥ 3468 (start-of-initiative baseline,
  2026-05-21).
- [ ] 944/944 unit clean.
- [ ] Functional clean.
- [ ] Performance: per-clause work no worse than baseline (anecdotal
  via timing on a couple of benchmark queries; no formal benchmark
  required for this task).

## Affected files

None directly — this task is a gate.
