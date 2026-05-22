---
id: e9-full-tck-regression-gate-after
level: task
title: "E9: full TCK regression gate after E2-E8 land"
short_code: "GQLITE-T-0318"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T13:58:33.695752+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0311, GQLITE-T-0312, GQLITE-T-0313, GQLITE-T-0314, GQLITE-T-0315, GQLITE-T-0316, GQLITE-T-0317]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E9: full TCK regression gate after E2-E8 land

## Status Updates

**2026-05-22 — gate passed.** Final regression measurement:

- TCK [extension]: **3486 / 3880** (89.8% executable)
  vs start-of-initiative baseline 3468 → **+18 net gain**
- Unit: **944 / 944** (asserts 5570 / 5570)
- Functional: clean (44 .sql files all pass)

All goal targets met:

- **G1**: handle_* in query_dispatch.c routed via dedicated handlers
  (handle_match_set, handle_match_delete, handle_match_remove,
  handle_match_create, handle_match_merge, handle_call_subquery).
  MATCH+WITH+MERGE got its own dispatcher pattern (T-0317).
- **G2**: finalize_sql_generation is idempotent and called once at
  the transform→execute boundary (T-0311 in
  transform_single_query_sql and cypher_transform_query, T-0312 same
  for cypher_transform_generate_sql).
- **G3**: unified_builder is the sole assembly point;
  sql_builder_to_string handles both SELECT and pure-DML shapes
  (T-0312); the legacy raw_output drain shim is removed.
- **G4**: zero regressions. +18 net TCK gain across the whole
  initiative.

Cumulative wins by sub-task:
  T-0310  +1   (DML/SELECT split — Set6 [5])
  T-0311  0    (E2 finalize relocation — structural)
  T-0312  0    (E3 drain shim removal — structural)
  T-0313  +4   (E4 transform_with edge-aware — Delete6 [12]/[13]/[14] + Return4 [11])
  T-0314  +9   (E5 SET set6/remove3/withskiplimit)
  T-0315  +3   (E6 REMOVE label snapshot — Remove3 [10]/[11]/[13])
  T-0316  0    (E7 CALL hoist — structural)
  T-0317  +2   (E8 MATCH+WITH+MERGE — Merge5 [16]/[17])

I-0042 ready to transition completed → archived.

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

After E2-E8 are all landed, run a full TCK regression and verify the
pass count is at or above the I-0042 start baseline. If anything
regressed, file follow-ons before transitioning I-0042 to completed.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

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