---
id: k-hop-neighborhood-and-bfs
level: initiative
title: "K-Hop Neighborhood and BFS Traversal"
short_code: "GQLITE-I-0015"
created_at: 2025-12-20T02:01:22.765416+00:00
updated_at: 2025-12-20T02:01:22.765416+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: k-hop-neighborhood-and-bfs
---

# K-Hop Neighborhood and BFS Traversal Initiative

## Context

For graph RAG applications, K-hop neighborhood traversal and breadth-first search (BFS) are fundamental operations that enable efficient local graph exploration. These algorithms are essential for finding related entities within a specific distance from a starting point, making them critical for retrieval-augmented generation where we need to gather contextually relevant nodes and relationships within a bounded neighborhood.

Currently, GraphQLite lacks native support for k-hop traversals and BFS algorithms, limiting its effectiveness for RAG use cases where understanding local graph topology is crucial for relevance scoring and context gathering.

## Goals & Non-Goals

**Goals:**
- Implement k-hop neighborhood traversal functions for finding all nodes within k steps
- Add breadth-first search (BFS) algorithm implementation for systematic graph exploration
- Create efficient SQL-based implementations that leverage GraphQLite's EAV storage model
- Support parameterized k-hop queries with configurable depth limits
- Enable bi-directional traversal (both incoming and outgoing relationships)
- Provide relationship type filtering during traversal
- Optimize performance for typical RAG use cases (k ≤ 3)

**Non-Goals:**
- Depth-first search (DFS) algorithms (separate initiative)
- Variable-length path queries with complex patterns
- Graph analytics beyond basic neighborhood exploration
- Real-time streaming traversal APIs

## Detailed Design

### New Cypher Functions

**K-Hop Neighborhood Functions:**
- `khop(start_node, k)` - Returns all nodes within k hops of start_node
- `khop(start_node, k, relationship_types)` - K-hop with relationship type filtering
- `khop_directed(start_node, k, direction)` - Directional k-hop (INCOMING/OUTGOING/BOTH)

**BFS Functions:**
- `bfs(start_node, end_node)` - BFS path between two nodes
- `bfs_levels(start_node, max_depth)` - BFS with level information
- `bfs_neighbors(start_node, level)` - Get neighbors at specific BFS level

### Implementation Architecture

**SQL-Based Recursive CTEs:**
```sql
-- K-hop neighborhood using recursive CTE
WITH RECURSIVE khop_traversal(node_id, hop_count) AS (
    SELECT node_id, 0 FROM nodes WHERE id = ?
    UNION ALL
    SELECT e.target_id, kt.hop_count + 1
    FROM khop_traversal kt
    JOIN edges e ON (e.source_id = kt.node_id OR e.target_id = kt.node_id)
    WHERE kt.hop_count < ? AND e.target_id != kt.node_id
)
SELECT DISTINCT node_id FROM khop_traversal;
```

**Performance Optimizations:**
- Index-based edge traversal using source_id/target_id indexes
- Early termination for bounded searches
- Result set deduplication during traversal

## Alternatives Considered

**1. In-Memory Graph Processing:** Load graph subset into memory - Rejected: High memory overhead, doesn't leverage SQLite's query optimization

**2. External Graph Libraries:** Integrate with NetworkX or igraph - Rejected: Adds external dependencies, requires data marshaling

**3. Stored Procedures:** Implement as SQLite stored procedures - Rejected: Limited SQLite stored procedure support

**4. Materialized Views:** Pre-compute k-hop neighborhoods - Rejected: Storage overhead and staleness with dynamic graphs

## Implementation Plan

**Phase 1: Core K-Hop Function** - Implement basic `khop(start_node, k)` function with recursive CTE generation

**Phase 2: Directional and Filtered Traversal** - Add `khop_directed()` and relationship type filtering

**Phase 3: BFS Implementation** - Implement `bfs()`, `bfs_levels()`, and `bfs_neighbors()` functions

**Phase 4: Performance Optimization** - Add proper indexing and query plan optimization

**Phase 5: Integration and Documentation** - Comprehensive test suite with graph RAG scenarios

## Testing Strategy

**Unit Tests:**
- Function parameter validation (invalid node IDs, negative k values)
- Basic k-hop traversal correctness (1-hop, 2-hop, 3-hop neighborhoods)
- Directional traversal validation
- Edge cases: disconnected nodes, self-loops, cycles

**Integration Tests:**
- End-to-end queries with k-hop functions in WHERE and RETURN clauses
- Performance testing with graphs of varying sizes (100, 1K, 10K nodes)

**RAG-Specific Validation:**
- Neighborhood retrieval for entity relationships in knowledge graphs
- Multi-hop concept traversal for semantic similarity