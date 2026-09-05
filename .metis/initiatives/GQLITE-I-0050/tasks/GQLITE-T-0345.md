---
id: 104-105-106-unwrap-column-0-once
level: task
title: "#104/#105/#106: unwrap column_0 once at the result boundary (astar, nodeSimilarity, knn, Rust bfs/dfs/apsp)"
short_code: "GQLITE-T-0345"
created_at: 2026-09-05T13:15:32.542755+00:00
updated_at: 2026-09-05T13:15:32.542755+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #104/#105/#106: unwrap column_0 once at the result boundary (astar, nodeSimilarity, knn, Rust bfs/dfs/apsp)

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #104 (astar), #105 (node_similarity/knn), and #106 (Rust bfs/dfs/apsp): the core wraps algorithm output in `column_0`, and these eight entry points read fields off the raw row and therefore always return empty. Move the unwrap to one place per binding and delete the per-method copies.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed.
- Python `astar('a','c')` → `found=False`, while raw `RETURN astar('a','c')` → `{'column_0': {'path': ['a','c'], 'found': True, 'nodes_explored': 3}}`.
- Python `node_similarity()` / `knn('a',2)` → `[]`, raw shows 6 pairs.
- Rust `bfs`/`dfs`/`apsp` iterate `result.iter()` directly (confirmed by reading `traversal.rs`, `paths.rs`).
- Probe of every algorithm function (pageRank … triangleCount) shows the core emits **only** `column_0`; the `wcc()`/`pagerank()`-style names in `ALGO_COLUMN_NAMES` never match.

## Acceptance Criteria **[REQUIRED]**

- [ ] Python: `extract_algo_array` + new `extract_algo_object` in `_parsing.py` are the only unwrap sites; `ALGO_COLUMN_NAMES` reduced to `column_0`; astar, shortest_path, node_similarity, knn, triangle_count, apsp use them.
- [ ] Rust: `algo_rows()` / `algo_object()` in `algorithms/parsing.rs`; astar, shortest_path, node_similarity, knn, triangle_count, bfs, dfs, apsp, and the centrality/community/components callers route through them.
- [ ] Python tests: `test_astar`, `test_astar_no_path`, `test_node_similarity_all_pairs`, `test_node_similarity_pair`, `test_knn`.
- [ ] Rust tests: same seven cases plus `test_bfs`, `test_dfs`, `test_apsp` in `tests/integration.rs`.

## Implementation Notes

Files: `bindings/python/src/graphqlite/algorithms/{_parsing,paths,similarity,traversal}.py`, `bindings/rust/src/algorithms/{parsing,paths,similarity,traversal,centrality,community,components}.rs`.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: implemented: Python `_parsing.extract_algo_array`/`extract_algo_object` (only `column_0`), Rust `algo_rows`/`algo_object`; all 8 entry points + centrality/community/components routed through them. Tests added in both bindings (astar x3, similarity x2, knn, bfs, dfs, apsp). Python suite + Rust suite green.
