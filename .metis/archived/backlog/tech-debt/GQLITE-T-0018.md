---
id: modularize-cypher-executor-c-3-963
level: task
title: "Modularize cypher_executor.c (3,963 lines)"
short_code: "GQLITE-T-0018"
created_at: 2025-12-24T18:26:04.255124+00:00
updated_at: 2025-12-25T16:29:10.578308+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#tech-debt"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Modularize cypher_executor.c (3,963 lines)

## Objective

Split `src/backend/executor/cypher_executor.c` (3,963 lines) into focused, maintainable modules to improve code organization, testability, and developer experience.

## Backlog Item Details

### Type
- [x] Tech Debt - Code improvement or refactoring

### Priority
- [x] P2 - Medium (nice to have)

### Technical Debt Impact
- **Current Problems**: 
  - Single 4K-line file is difficult to navigate and understand
  - Related functionality scattered throughout the file
  - Hard to test individual components in isolation
  - Merge conflicts more likely when multiple developers work on executor
- **Benefits of Fixing**: 
  - Clear separation of concerns
  - Easier onboarding for new contributors
  - Better testability per module
  - Faster compilation when only one module changes
- **Risk Assessment**: Low risk if done incrementally with test coverage

## Current File Analysis

The file handles 9+ distinct concerns:

| Concern | Functions | Est. Lines |
|---------|-----------|------------|
| MATCH execution | execute_match_*, find_node_by_pattern, find_edge_by_pattern | ~800 |
| MERGE execution | execute_merge_clause, execute_match_merge_query | ~700 |
| CREATE execution | execute_create_clause, execute_match_create_* | ~400 |
| SET execution | execute_set_*, execute_set_operations, execute_set_items | ~400 |
| Result building | create_empty_result, build_query_results, result_free/print | ~400 |
| DELETE execution | delete_*_by_id, execute_match_delete_query | ~250 |
| FOREACH execution | foreach_context funcs, execute_foreach_clause | ~200 |
| Variable map | create/free/get/set_variable_* | ~150 |
| Path execution | execute_path_pattern_with_variables, build_path_from_ids | ~300 |
| Core/dispatch | init, free, main entry points | ~300 |

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Create `executor_match.c` with MATCH-related functions
- [ ] Create `executor_merge.c` with MERGE-related functions  
- [ ] Create `executor_create.c` with CREATE-related functions
- [ ] Create `executor_set.c` with SET-related functions
- [ ] Create `executor_delete.c` with DELETE-related functions
- [ ] Create `executor_foreach.c` with FOREACH-related functions
- [ ] Create `executor_result.c` with result handling functions
- [ ] Create `variable_map.c` with variable map operations
- [ ] Update `cypher_executor.c` to only contain core init/dispatch logic (~400 lines)
- [ ] Create shared header `executor_internal.h` for inter-module declarations
- [ ] Update Makefile to compile new modules
- [ ] All existing tests pass
- [ ] No new compiler warnings

## Implementation Plan

### Phase 1: Infrastructure
1. Create `src/backend/executor/executor_internal.h` with shared types/declarations
2. Update Makefile to handle new source files

### Phase 2: Extract Low-Risk Modules
3. Extract `variable_map.c` (self-contained, no external deps)
4. Extract `executor_result.c` (result creation/handling)
5. Run tests after each extraction

### Phase 3: Extract Clause Handlers
6. Extract `executor_foreach.c` (relatively isolated)
7. Extract `executor_delete.c` 
8. Extract `executor_set.c`
9. Extract `executor_create.c`

### Phase 4: Extract Complex Modules
10. Extract `executor_match.c` (core pattern matching)
11. Extract `executor_merge.c` (depends on match + create logic)

### Phase 5: Cleanup
12. Refactor `cypher_executor.c` to be thin dispatch layer
13. Remove any remaining duplication
14. Final test pass

## Status Updates

*To be added during implementation*