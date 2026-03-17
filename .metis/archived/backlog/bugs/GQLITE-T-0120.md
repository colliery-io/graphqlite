---
id: apsp-returns-empty-results-in
level: task
title: "APSP returns empty results in Python bindings"
short_code: "GQLITE-T-0120"
created_at: 2026-03-17T02:45:27.787638+00:00
updated_at: 2026-03-17T02:54:10.922249+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# APSP returns empty results in Python bindings

## Objective

Fix `all_pairs_shortest_path()` / `apsp()` in Python bindings to return actual results instead of empty list.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P1 - High (important for user experience)

### Impact Assessment
- **Affected Users**: All Python binding users calling APSP
- **Reproduction Steps**: 
  1. Create a graph with nodes and edges
  2. Call `g.apsp()`
  3. Get empty list `[]` instead of path results
- **Expected vs Actual**: Should return list of dicts with `source`, `target`, `distance`. Returns `[]`.

## Root Cause

Same as GQLITE-T-0119. The `all_pairs_shortest_path()` in `bindings/python/src/graphqlite/algorithms/paths.py` iterates `result` rows directly without calling `extract_algo_array()` to unwrap the `column_0` wrapper from the C extension.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `apsp()` returns non-empty list with correct path results
- [ ] Results contain `source`, `target`, `distance` fields
- [ ] Existing Python tests pass

## Implementation Notes

### Technical Approach
Add `extract_algo_array()` call in `paths.py` for `all_pairs_shortest_path`, matching the pattern used in other algorithm mixins.

## Status Updates

### Implementation Complete
- **Fix**: Added `extract_algo_array()` call in `paths.py` for `all_pairs_shortest_path`
- **Verified**: APSP returns `[{source, target, distance}]` correctly
- **Tests**: 226 Python tests pass