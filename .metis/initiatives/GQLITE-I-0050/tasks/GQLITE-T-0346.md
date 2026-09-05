---
id: 107-core-nodesimilarity-threshold
level: task
title: "#107: core nodeSimilarity(threshold, topK) ignores threshold"
short_code: "GQLITE-T-0346"
created_at: 2026-09-05T13:18:36.245066+00:00
updated_at: 2026-09-05T13:21:00.995924+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #107: core nodeSimilarity(threshold, topK) ignores threshold

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #107: `nodeSimilarity(threshold, topK)` ignores `threshold` because the two-argument numeric branch in `detect_graph_algorithm()` (`src/backend/executor/graph_algorithms.c`) only reads `top_k`, and the threshold read lives in an `else` branch that is skipped when two args are present.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed.
- `RETURN nodeSimilarity(0.9)` → `[]` (correct).
- `RETURN nodeSimilarity(0.9, 10)` → all 6 pairs including `similarity: 0.0` (threshold ignored).
- `RETURN nodeSimilarity(0, 1)` → exactly 1 pair (top_k works).

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] In the `count >= 2 && !params.source_id` branch, resolve `args[0]` as threshold before `args[1]` as top_k.
- [ ] CUnit test in `tests/test_executor_similarity.c`: `nodeSimilarity(0.5, 10)` on a fixed graph returns only pairs with similarity ≥ 0.5; `nodeSimilarity(0, 1)` returns exactly one pair.
- [ ] Functional SQL coverage in `tests/functional/36_parameterized_algorithms.sql` or a new file.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: fixed in `graph_algorithms.c` (threshold read from args[0] in the two-numeric-arg branch). CUnit `test_similarity_threshold_with_topk` added, 945/945 green; functional `tests/functional/38_similarity_args.sql` added with assertive checks.
