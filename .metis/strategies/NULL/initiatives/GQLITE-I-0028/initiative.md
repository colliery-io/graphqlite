---
id: cpu-graph-caching-for-algorithm
level: initiative
title: "CPU + Graph Caching for Algorithm Acceleration"
short_code: "GQLITE-I-0028"
created_at: 2026-01-09T19:25:48.400813+00:00
updated_at: 2026-01-09T19:25:48.400813+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: cpu-graph-caching-for-algorithm
---

# CPU + Graph Caching for Algorithm Acceleration Initiative

## Context

Graph algorithms (PageRank, centrality, community detection) suffer from significant performance overhead due to SQLite I/O. Each algorithm call rebuilds the CSR (Compressed Sparse Row) graph representation from SQLite, which dominates execution time (~75-95% of total time).

Benchmarking revealed that GPU acceleration provides no meaningful benefit on Apple Silicon due to:
- Memory-bound nature of graph algorithms (not compute-bound)
- Unified memory architecture (no VRAM advantage)
- Well-optimized CPU baseline

However, **caching the CSR graph in memory** provides consistent **4-5x speedup** by eliminating repeated SQLite I/O for read-heavy workloads.

## Goals & Non-Goals

**Goals:**
- Provide per-connection CSR graph caching for algorithm acceleration
- Support explicit cache lifecycle: load, reload (invalidate), unload
- Automatic cleanup on connection close
- 4-5x speedup for repeated algorithm calls on same graph

**Non-Goals:**
- GPU acceleration (see ADR-0003 for rationale)
- Automatic cache invalidation on mutations
- Cross-connection cache sharing

## Detailed Design

### SQL Functions

| Function | Purpose |
|----------|---------|
| `gql_load_graph()` | Build CSR from SQLite and cache in connection memory |
| `gql_unload_graph()` | Free cached graph memory |
| `gql_reload_graph()` | Invalidate cache and rebuild from current database state |
| `gql_graph_loaded()` | Return cache status (loaded, node_count, edge_count) |

### Usage Pattern

```sql
-- Load cache once per session
SELECT gql_load_graph();
-- {"status":"loaded","nodes":50000,"edges":500000}

-- Run multiple algorithms (all use cached graph - fast)
SELECT cypher('RETURN pageRank()');
SELECT cypher('RETURN betweenness()');
SELECT cypher('RETURN louvain()');

-- After mutations, invalidate and reload
SELECT cypher('CREATE (:NewNode {id: 999})');
SELECT gql_reload_graph();
-- {"status":"reloaded","nodes":50001,"edges":500000,"previous_nodes":50000,"previous_edges":500000}

-- Optional explicit cleanup (automatic on disconnect)
SELECT gql_unload_graph();
```

### Cache Scope

- **Per-connection**: Each SQLite connection has independent cache
- **Automatic cleanup**: Cache freed when connection closes
- **Explicit lifecycle**: User controls when to load/reload/unload

### Algorithm Integration

Graph algorithms check `executor->cached_graph`:
- If cached graph exists → use it (skip SQLite I/O)
- If no cache → load from SQLite (original behavior)

## Alternatives Considered

| Alternative | Why Rejected |
|-------------|--------------|
| GPU acceleration | No speedup on Apple Silicon (see ADR-0003) |
| Automatic cache invalidation | Complex, error-prone; explicit user control preferred |
| Global/shared cache | Complicates concurrency; per-connection is simpler |
| Two separate libraries | Unnecessary complexity; single library with optional caching |

## Implementation Plan

1. ✅ Add `cached_graph` field to connection cache struct
2. ✅ Implement `gql_load_graph()` function
3. ✅ Implement `gql_unload_graph()` function
4. ✅ Implement `gql_graph_loaded()` function
5. ✅ Add `gql_reload_graph()` for cache invalidation
6. ✅ Modify PageRank to use cached graph when available
7. ✅ Register all cache functions in extension init
8. ✅ Refactor all 17 algorithms to accept `cached` parameter
   - Changed signature: `execute_X(db, ...)` → `execute_X(db, cached, ...)`
   - When `cached` is non-NULL, uses directly (skip SQLite I/O)
   - When `cached` is NULL, loads from SQLite (original behavior)
9. Add CUnit tests for cache lifecycle
10. Add performance/benchmark tests
11. Add Python bindings for cache functions
12. Add Rust bindings for cache functions
13. Update documentation

## Benchmark Results

| Graph Size | Uncached | Cached | Speedup |
|------------|----------|--------|---------|
| 1K nodes / 5K edges | 1.1ms | 0.04ms | **28x** |
| 10K nodes / 50K edges | 8.0ms | 0.28ms | **28x** |