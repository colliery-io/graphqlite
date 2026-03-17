---
id: segfault-on-parameterized-bfs-dfs
level: task
title: "Segfault on parameterized bfs/dfs traversals"
short_code: "GQLITE-T-0112"
created_at: 2026-03-03T02:12:25.655318+00:00
updated_at: 2026-03-03T02:27:22.507223+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/active"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Segfault on parameterized bfs/dfs traversals

GitHub Issue: #27 (reported by @kynx)
Version: 0.3.5, SQLite 3.51.2

## Objective

Fix segmentation fault when using parameterized `bfs()` / `dfs()` traversals on a populated graph.

## Bug Details

- **Priority**: P0 - segfault crashes the sqlite3 process
- **Reproduction Steps**:
  1. `select cypher('RETURN bfs($a)', '{"a": "A"}');` — works on empty graph
  2. `select cypher('CREATE (a:Node {id: ''A''})');` — create a node
  3. `select cypher('RETURN bfs($a)', '{"a": "A"}');` — segfault
- **Expected**: Returns traversal result with the created node
- **Actual**: Segmentation fault (signal 11)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `RETURN bfs($param)` with parameters works on populated graphs
- [ ] `RETURN dfs($param)` with parameters works on populated graphs
- [ ] Functional test covering parameterized traversals added
- [ ] No segfault under any combination of empty/populated graph + params

## Investigation Findings

**Root cause**: `detect_graph_algorithm()` (`graph_algorithms.c:477-504`) only handles `AST_NODE_LITERAL` args, ignoring `AST_NODE_PARAMETER`. When `$a` is passed, `source_id` stays NULL → passed to `execute_bfs()` → `strcmp(user_id, NULL)` segfaults at `graph_algo_traversal.c:149`.

Empty graphs don't crash because the node loop (n=0) never executes the strcmp.

**Crash path**: `query_dispatch.c:914` → `detect_graph_algorithm()` → `graph_algorithms.c:477` (literal-only check) → `query_dispatch.c:977` (NULL source_id) → `graph_algo_traversal.c:149` (strcmp with NULL)

**All affected functions**: bfs, dfs, dijkstra, astar, nodeSimilarity, knn — all have the same literal-only parameter extraction.

**Fix approach**: Resolve `AST_NODE_PARAMETER` nodes in `detect_graph_algorithm()` using the executor's `params_json` (infrastructure already exists via `get_param_value()` in `executor_helpers.c`).

## Implementation Plan

1. **NULL guards** in `graph_algo_traversal.c`: `execute_bfs()` and `execute_dfs()` return empty result if `start_id` is NULL (safety net)
2. **Signature change**: Add `const char *params_json` to `detect_graph_algorithm()` in `graph_algorithms.h` and update call site in `query_dispatch.c:914`
3. **Parameter resolution**: Add `resolve_string_arg()` helper in `graph_algorithms.c` that handles both `AST_NODE_LITERAL` and `AST_NODE_PARAMETER` via existing `get_param_value()`. Apply to all affected algos: bfs, dfs, dijkstra, astar, nodeSimilarity, knn
4. **Tests**: `tests/functional/36_parameterized_algorithms.sql` (already written)

### Files to modify
- `src/backend/executor/graph_algo_traversal.c` — NULL guards
- `src/include/executor/graph_algorithms.h` — signature change
- `src/backend/executor/query_dispatch.c` — pass `executor->params_json`
- `src/backend/executor/graph_algorithms.c` — helper + all extraction sites

## Status Updates

### Implementation Complete
All 4 changes implemented:

1. **NULL guards** — `execute_bfs()` and `execute_dfs()` now return `[]` if `start_id` is NULL
2. **Signature change** — `detect_graph_algorithm()` now accepts `const char *params_json`; call site in `query_dispatch.c` updated to pass `executor->params_json`
3. **Parameter resolution** — Added `resolve_string_arg()` static helper that handles both `AST_NODE_LITERAL` and `AST_NODE_PARAMETER` via `get_param_value()`. Applied to all 6 affected algorithms: bfs, dfs, dijkstra, astar, nodeSimilarity, knn
4. **Include** — Added `#include "executor/executor_internal.h"` for `get_param_value()` and `property_type`

### Verification
- `angreal build extension` — builds clean, no warnings
- `tests/functional/36_parameterized_algorithms.sql` — all 20 tests pass, no segfault
- `angreal test functional` — all existing functional tests pass
- `angreal test unit` — all 770+ unit tests pass