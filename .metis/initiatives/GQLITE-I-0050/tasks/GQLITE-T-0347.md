---
id: 108-bindings-node-similarity-top-k
level: task
title: "#108: bindings node_similarity(top_k=N) ignored unless threshold set"
short_code: "GQLITE-T-0347"
created_at: 2026-09-05T13:18:37.693293+00:00
updated_at: 2026-09-05T13:34:39.651909+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #108: bindings node_similarity(top_k=N) ignored unless threshold set

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #108: both bindings only emit the two-argument `nodeSimilarity(threshold, top_k)` form when `threshold > 0`, so `node_similarity(top_k=5)` with the default threshold falls through to `nodeSimilarity()` and returns every pair.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `g.node_similarity(top_k=1)` returned `[]` (masked by #105); reading `similarity.py:35` / `similarity.rs:35` confirms the branch order.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Python and Rust: branch order becomes pair → `top_k > 0` (two-arg form with whatever threshold) → `threshold > 0` → bare.
- [ ] Python `test_node_similarity_topk_only`: `len(result) == k` when more pairs exist.
- [ ] Rust: same.

Depends on [[GQLITE-T-0345]] and [[GQLITE-T-0346]] for the result to be observable.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: branch order fixed in `similarity.py` and `similarity.rs` (top_k>0 → two-arg form). `test_node_similarity_topk_only` + `test_node_similarity_threshold_and_topk` in both bindings, green.