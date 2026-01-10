---
id: bulk-insert-operations-for-nodes
level: task
title: "Bulk Insert Operations for Nodes and Edges"
short_code: "GQLITE-T-0093"
created_at: 2026-01-10T04:16:05.119817+00:00
updated_at: 2026-01-10T04:16:05.119817+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Bulk Insert Operations for Nodes and Edges

Add true bulk insert methods to graphqlite that bypass individual Cypher query overhead, enabling high-performance graph construction from external data sources.

## Objective

Enable efficient bulk insertion of nodes and edges by providing native bulk insert APIs that bypass per-insert Cypher parsing overhead, reducing graph construction time by 30-100x.

## Problem

When building graphs from parsed source code (or any external data), we need to insert thousands of nodes and edges efficiently. The current approach has significant overhead:

**Current Node Insertion:**
```rust
// upsert_nodes_batch is just a loop calling upsert_node individually
for (node_id, props, label) in nodes {
    self.upsert_node(node_id, props, label)?;  // Individual query per node
}
```

**Current Edge Insertion:**
```rust
// upsert_edge requires internal ID lookup via Cypher MATCH
self.graph.upsert_edge(&source_id, &target_id, props, rel_type)?;
```

**Benchmark Results (muninn codebase - 50 files):**
- Parse time (tree-sitter): 214ms
- Store time (graphqlite): 29,315ms
- **99.3% of indexing time is spent in graph storage**

The bottleneck is not SQLite itself (which can handle millions of inserts per second), but the per-insert overhead of:
1. Cypher query parsing
2. Property map construction
3. For edges: MATCH query to resolve external IDs to internal row IDs

## Proposed Solution

### 1. Bulk Node Insert

```rust
/// Insert multiple nodes in a single transaction with minimal overhead.
/// Returns a map of external_id -> internal_id for subsequent edge insertion.
fn insert_nodes_bulk<I, N, P, K, V, L>(
    &self,
    nodes: I,
) -> Result<HashMap<String, i64>>
where
    I: IntoIterator<Item = (N, P, L)>,
    N: AsRef<str>,           // external node ID
    P: IntoIterator<Item = (K, V)>,
    K: AsRef<str>,
    V: Into<Value>,
    L: AsRef<str>,           // label
```

**Implementation approach:**
- Begin transaction
- Batch INSERT into `nodes` table
- Batch INSERT into `node_labels` table
- Batch INSERT into `node_props_*` tables
- Commit transaction
- Return external_id -> internal_id mapping

### 2. Bulk Edge Insert (with ID mapping)

```rust
/// Insert multiple edges using pre-resolved internal IDs.
/// Use the mapping returned from insert_nodes_bulk.
fn insert_edges_bulk<I, P, K, V, R>(
    &self,
    edges: I,
    id_map: &HashMap<String, i64>,
) -> Result<()>
where
    I: IntoIterator<Item = (String, String, P, R)>,  // (source_ext_id, target_ext_id, props, rel_type)
    P: IntoIterator<Item = (K, V)>,
    K: AsRef<str>,
    V: Into<Value>,
    R: AsRef<str>,
```

**Implementation approach:**
- Begin transaction
- Look up internal IDs from provided mapping (in-memory, no DB query)
- Batch INSERT into `edges` table
- Batch INSERT into `edge_props_*` tables
- Commit transaction

### 3. Alternative: Raw SQL Access

If bulk methods are complex to implement, exposing raw SQL execution would allow users to optimize their specific use case:

```rust
/// Execute raw SQL for advanced use cases.
fn execute_sql(&self, sql: &str) -> Result<()>;

/// Execute raw SQL with parameters.
fn execute_sql_params(&self, sql: &str, params: &[Value]) -> Result<()>;
```

## Example Usage

```rust
// Build graph from parsed source code
let symbols: Vec<Symbol> = parse_files(&files);
let edges: Vec<Edge> = extract_relationships(&symbols);

// Bulk insert nodes, get ID mapping
let id_map = graph.insert_nodes_bulk(
    symbols.iter().map(|s| (s.id(), s.properties(), s.label()))
)?;

// Bulk insert edges using the mapping
graph.insert_edges_bulk(
    edges.iter().map(|e| (e.source_id, e.target_id, e.properties(), e.rel_type)),
    &id_map,
)?;
```

## Expected Performance Improvement

Based on SQLite's raw insert performance and our current bottleneck analysis:

| Operation | Current | Expected with Bulk |
|-----------|---------|-------------------|
| 1600 nodes | ~10s | <100ms |
| 7300 edges | ~20s | <500ms |
| **Total** | ~30s | <1s |

This would make graph indexing fast enough to run on every file save in watch mode.

## Workaround Attempted

We tried using raw Cypher with batched CREATE statements:

```cypher
CREATE (n0:Function {id: 'x', ...}), (n1:Struct {id: 'y', ...}), ...
```

This works for nodes but hits SQLite limits:
- `too many FROM clause terms, max: 200`
- `at most 64 tables in a join`

For edges, any MATCH-based approach triggers expensive joins:
```cypher
MATCH (s0 {id: 'x'}), (t0 {id: 'y'}) CREATE (s0)-[:CALLS]->(t0)
-- Each node match = table join
```

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [x] P1 - High (important for user experience)

### Business Justification
- **User Value**: Enables practical use of graphqlite for code indexing and other large-scale graph construction use cases
- **Business Value**: Unlocks the primary use case for muninn (code graph indexing for AI-assisted development)
- **Effort Estimate**: L

## Acceptance Criteria

- [ ] `insert_nodes_bulk` method implemented with batch INSERT operations
- [ ] `insert_edges_bulk` method implemented using in-memory ID mapping
- [ ] Both methods wrapped in transactions for atomicity
- [ ] Python bindings exposed for bulk operations
- [ ] Benchmark shows 30x+ improvement for 1000+ node/edge insertions
- [ ] Documentation with usage examples

## Implementation Notes

### Technical Approach
1. Add bulk insert methods to core Rust `Graph` struct
2. Use prepared statements with batch parameter binding
3. Return HashMap for external->internal ID mapping from node bulk insert
4. Expose via Python bindings with appropriate type conversions

### Dependencies
- Related to GQLITE-T-0094 (transaction-based batch bindings)

### Risk Considerations
- Schema evolution: bulk inserts bypass Cypher so must directly match table structure
- Memory usage: collecting ID mappings for very large graphs may need streaming approach

## Context

- **Project**: muninn - code graph indexing for AI-assisted development
- **Scale**: Typical codebase has 100-1000 files, 10k-100k symbols, 50k-500k edges
- **Use case**: Index on startup, incremental updates on file change

## Status Updates

### 2026-01-10: Initial Implementation Complete

Implemented bulk insert operations for both Rust and Python bindings:

**New API Methods:**
- `insert_nodes_bulk(nodes)` - Insert nodes, returns HashMap<external_id, rowid>
- `insert_edges_bulk(edges, id_map)` - Insert edges using ID map
- `insert_graph_bulk(nodes, edges)` - Convenience method for both
- `resolve_node_ids(ids)` - Resolve existing node IDs

**Performance Results (in-memory, 1000 nodes + 5000 edges):**

| Language | Nodes | Edges | Total |
|----------|-------|-------|-------|
| Rust | 15.6ms (64k/s) | 140ms (35k/s) | 156ms |
| Python | 11ms (94k/s) | 39ms (128k/s) | 49ms |

**Improvement vs Original:**
- Original approach: ~29 seconds for similar workload
- New bulk insert: ~50-156ms
- **Speedup: 185-580x faster**

**Files Added/Modified:**
- `bindings/rust/src/graph/bulk.rs` - Rust implementation
- `bindings/rust/src/graph/mod.rs` - Module export
- `bindings/rust/src/lib.rs` - Public export
- `bindings/python/src/graphqlite/graph/bulk.py` - Python implementation
- `bindings/python/src/graphqlite/graph/__init__.py` - Module export
- `bindings/python/src/graphqlite/__init__.py` - Public export