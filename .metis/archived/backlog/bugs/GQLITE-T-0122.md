---
id: graph-cache-functions-not
level: task
title: "Graph cache functions not registered in debug extension build"
short_code: "GQLITE-T-0122"
created_at: 2026-03-17T02:45:29.424871+00:00
updated_at: 2026-03-17T13:02:01.161106+00:00
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

# Graph cache functions not registered in debug extension build

## Objective

Register `gql_load_graph`, `gql_unload_graph`, `gql_reload_graph`, `gql_graph_loaded` SQL functions in debug extension builds so graph caching works outside release builds.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P2 - Medium (nice to have)

### Impact Assessment
- **Affected Users**: All debug-build users (developers, CI)
- **Reproduction Steps**: 
  1. Build extension in debug mode: `angreal build extension`
  2. Load extension and call `SELECT gql_load_graph()`
  3. Error: `no such function: gql_load_graph`
- **Expected vs Actual**: Cache functions should be available in all builds. Only available in release builds.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `gql_load_graph()` works in debug builds
- [ ] `gql_unload_graph()` works in debug builds
- [ ] `gql_reload_graph()` works in debug builds
- [ ] `gql_graph_loaded()` works in debug builds
- [ ] Python cache tests pass (currently skipped/failing)

## Implementation Notes

### Technical Approach
Check the extension entry point (`extension.c` or equivalent) for `#ifdef` guards that gate the cache function registration. Remove or adjust the conditional compilation so these functions are always registered.

## Status Updates

### No Fix Needed
- **Investigation result**: Cache functions (`gql_load_graph` etc.) ARE registered unconditionally in `extension.c` line 566-574 — no `#ifdef` guards
- **Verified via sqlite3 CLI**: All 4 functions work in debug builds
- **Verified via angreal test python**: All 8 cache tests pass (test_load_graph, test_load_graph_already_loaded, test_unload_graph, test_unload_graph_not_loaded, test_reload_graph, test_reload_graph_not_loaded, test_cache_with_pagerank, test_cache_empty_graph)
- **Root cause of original failure**: The investigation's ad-hoc Python test script likely loaded the extension incorrectly (wrong path or missing extension loading)