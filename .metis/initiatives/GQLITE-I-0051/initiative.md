---
id: performance-review-implementation
level: initiative
title: "Performance review implementation (PR #118) — read path, varlen, algorithms, write path, result memory"
short_code: "GQLITE-I-0051"
created_at: 2026-09-07T01:22:08.248029+00:00
updated_at: 2026-09-07T01:47:19.489253+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/active"


exit_criteria_met: false
estimated_complexity: L
initiative_id: performance-review-implementation
---

# Performance review implementation (PR #118) — read path, varlen, algorithms, write path, result memory Initiative

## Context **[REQUIRED]**

PR #118 (`docs/internal/performance-review.md` + `tests/performance/python/`) is a measured review of v0.7.0's speed and memory. It has no reviewer comments; the PR *is* the review. It lists ten findings ranked by impact plus one correctness bug, and a suggested order of work in four phases. All numbers were re-validated on this machine (macOS, release build, 2026-09-06) and reproduced, several worse than reported: Louvain 11.8 s at 10K nodes, nodeSimilarity 2.4 s at 5K, varlen `*1..3` 1.5 s at 10K/50K, `$id` lookup 100x slower than a literal, and EXPLAIN / control characters producing invalid JSON.

Work happens on branch `perf/review-phase1` (cut from the PR branch so the harness is available); PR #118 merges first, then this branch is rebased onto main.

## Goals & Non-Goals **[REQUIRED]**

**Goals:**
- Phase 1 (this branch): findings 1–4 (local, algorithmic, 10x–1000x) plus the C1 escaping bug and harness portability. Each with deterministic regression tests on SQL/plan shape, not timings.
- Phases 2–3 (F5–F9): constant-factor transform/result/write-path changes, each its own task and PR.
- Phase 4 (F10): design document for a table-valued result interface before any implementation.

**Non-Goals:**
- Changing the `cypher()` public surface in phases 1–3.
- Timing-based tests in the CI suites (flaky); numbers live in the harness output and PR descriptions.

## Detailed Design **[REQUIRED]**

- **F1** `transform_match.c`: inline `{k: $p}` on nodes and edges emits `<id> IN (SELECT id FROM typed_table JOIN property_keys ... WHERE key = 'k' AND value = :p UNION ALL ...)` over the four typed tables, driven by the `(key_id, value, id)` covering indexes. SQL text stays independent of parameter values (prerequisite for F8).
- **F2** `build_anchor_ids_sql()` (transform_match.c) turns the start node's inline map (literals and params, INTERSECT for multiple pairs) into a node-id set; `generate_varlen_cte()` gained an `anchor_ids_sql` argument and applies it to every base case (directed, reversed, both undirected orientations, zero-hop). Anchoring only uses the pattern's own inline map; a start node bound solely by an earlier clause is not yet anchored (follow-up).
- **F3** Louvain: `neighbor_comms` allocated once; `k_i_in` cleared only for touched communities after each node. O(E) per pass.
- **F4** `csr_sorted_col_idx()` (static inline in `graph_algorithms.h`) sorts every adjacency segment once; similarity and knn take slices with no per-pair allocation.
- **C1** `extension.c` text path escapes `\n \r \t \b \f` and other control chars as `\u00XX`; buffer sizing uses the 6x worst case.
- **H1** harness reads `/proc` when present and falls back to `ru_maxrss`; default library name follows the platform.

## Alternatives Considered **[REQUIRED]**

- F1: resolving the parameter's runtime type in the transform (SQL would then depend on parameter values, conflicting with the F8 statement cache). Rejected per the review's own recommendation.
- F2: replacing the `visited NOT LIKE` cycle check with an integer-set representation. Second-order after anchoring; deferred.

## Implementation Plan **[REQUIRED]**

1. Phase 1 on `perf/review-phase1`: F1, F2, F3, F4, C1, H1 → one PR after #118 merges.
2. F5 + F6 (consolidate the eight entity-JSON template copies first).
3. F7, F8, F9.
4. F10 design doc (ADR) before implementation.