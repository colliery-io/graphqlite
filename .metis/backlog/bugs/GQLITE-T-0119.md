---
id: bfs-dfs-return-empty-results-in
level: task
title: "BFS/DFS return empty results in Python bindings"
short_code: "GQLITE-T-0119"
created_at: 2026-03-17T02:45:26.902672+00:00
updated_at: 2026-03-17T02:51:50.036225+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# BFS/DFS return empty results in Python bindings

## Objective

Fix `bfs()` and `dfs()` in Python bindings to return actual traversal results instead of empty lists.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P1 - High (important for user experience)

### Impact Assessment
- **Affected Users**: All Python binding users calling BFS/DFS
- **Reproduction Steps**: 
  1. Create a graph with nodes and edges
  2. Call `g.bfs("start_node")` or `g.dfs("start_node")`
  3. Get empty list `[]` instead of traversal results
- **Expected vs Actual**: Should return list of dicts with `user_id`, `depth`, `order`. Returns `[]`.

## Root Cause

The C extension returns algorithm results wrapped as `[{"column_0": [...array...]}]`. Other algorithm mixins (centrality, community, components) call `extract_algo_array()` to unwrap this `column_0` wrapper. The `TraversalMixin` in `bindings/python/src/graphqlite/algorithms/traversal.py` iterates over `result` rows directly looking for `user_id`, but the raw rows have a single `column_0` key containing the actual array. The data is silently dropped.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `bfs()` returns non-empty list with correct traversal results
- [ ] `dfs()` returns non-empty list with correct traversal results
- [ ] Results contain `user_id`, `depth`, `order` fields
- [ ] `max_depth` parameter works correctly
- [ ] Existing Python tests pass

## Implementation Notes

### Technical Approach
Add `extract_algo_array()` call in `traversal.py` to unwrap `column_0`, matching the pattern used in centrality/community/components mixins.

## Status Updates

### Implementation Complete
- **Fix**: Added `extract_algo_array()` and `parse_traversal_result()` calls in `traversal.py`
- **Root cause**: `result` rows had `column_0` wrapping — needed unwrapping like other algo mixins
- **Added**: `bfs()`, `dfs()`, `apsp()` to `ALGO_COLUMN_NAMES` in `_parsing.py`
- **Verified**: BFS returns `[{user_id, depth, order}]` correctly, max_depth works
- **Tests**: 226 Python tests pass