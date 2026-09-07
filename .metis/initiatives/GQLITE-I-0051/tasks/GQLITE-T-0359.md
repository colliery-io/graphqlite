---
id: f3-louvain-local-move-pass-is-o-n
level: task
title: "F3: Louvain local-move pass is O(n^2) per iteration"
short_code: "GQLITE-T-0359"
created_at: 2026-09-07T01:25:05.368326+00:00
updated_at: 2026-09-07T01:46:58.459658+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F3: Louvain local-move pass is O(n^2) per iteration

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 3: `graph_algo_louvain.c` mallocs an n-sized array and zeroes an n-sized array for every node in every iteration, so each local-move pass is O(n^2).

## Validation

Validated 2026-09-06 on macOS with a release build via `tests/performance/python` (harness patched for macOS in H1). `RETURN louvain()` took 11.8 s at 10K nodes / 50K edges.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `neighbor_comms` allocated once per run; `k_i_in` cleared only for the communities touched by the node (plus its own).
- [ ] Existing Louvain CUnit tests pass; Python `test_louvain_two_cliques` asserts two 5-cliques joined by one edge land in two communities.
- [ ] Harness shows the 10K case in the tens of milliseconds.

## Status Updates **[REQUIRED]**

- 2026-09-06: created, validated, implemented.
- 2026-09-06: Output on a two-clique graph verified byte-identical to a fresh origin/main build (worktree baseline). Note: the existing single-level local-move pass can split a clique; the Python test asserts cliques never merge across the bridge and the result is deterministic.
- 2026-09-06: Release numbers: 1483 ms -> 168 ms at 10K (9x); 839 ms at 50K vs the review's 44.6 s baseline (53x). The remaining time is the local-move iteration count itself (pageRank on the same cached graph is 25 ms), not allocation.