---
id: f4-nodesimilarity-and-knn-re-sort
level: task
title: "F4: nodeSimilarity and knn re-sort adjacency lists per pair"
short_code: "GQLITE-T-0360"
created_at: 2026-09-07T01:25:06.872792+00:00
updated_at: 2026-09-07T01:46:59.521031+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F4: nodeSimilarity and knn re-sort adjacency lists per pair

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 4: `jaccard_similarity` calls `get_neighbors_sorted` (malloc + insertion sort) for both nodes on every pair; `knn` does the same per candidate.

## Validation

Validated 2026-09-06 on macOS with a release build via `tests/performance/python` (harness patched for macOS in H1). `nodeSimilarity(0.5)` 2.4 s at 5K nodes / 25K edges; `knn` 10 ms at 10K.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `csr_sorted_col_idx()` sorts each adjacency segment once; similarity and knn use slices with no per-pair allocation.
- [ ] Existing similarity/knn CUnit tests pass; Python `test_node_similarity_matches_pairwise_and_knn` checks all-pairs, single-pair, and knn agree.

## Status Updates **[REQUIRED]**

- 2026-09-06: created, validated, implemented (`graph_algorithms.h`, `graph_algo_similarity.c`, `graph_algo_knn.c`).
- 2026-09-06: nodeSimilarity/knn output verified byte-identical to origin/main on the baseline graph; existing CUnit suites green.
- 2026-09-06: Sorted-adjacency alone gave only 2x at 5K (1045 -> 590 ms) because the all-pairs loop is inherently O(n^2) and the n*(n-1)/2 preallocation was 200 MB. Second change: for threshold > 0 enumerate candidate pairs through shared out-neighbours (u -> w <- v, per-u stamp dedupe), growable pair list, canonical tie-break in the sort, and the node cap raised to 50,000 for that mode (5,000 stays for threshold 0, whose output is O(n^2) by definition).