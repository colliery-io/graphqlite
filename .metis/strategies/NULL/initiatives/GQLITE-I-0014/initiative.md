---
id: standard-shortest-path-algorithms
level: initiative
title: "Standard Shortest Path Algorithms"
short_code: "GQLITE-I-0014"
created_at: 2025-12-20T02:01:22.693922+00:00
updated_at: 2025-12-22T14:31:32.400043+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: L
strategy_id: NULL
initiative_id: standard-shortest-path-algorithms
---

# Standard Shortest Path Algorithms Initiative

## Context

The OpenCypher specification defines standard shortest path functions that are essential for graph analysis and pathfinding operations. These functions are distinct from the general shortest path algorithms and focus on OpenCypher-compliant path operations. Currently, GraphQLite lacks these standard functions, limiting compatibility with OpenCypher queries that rely on built-in shortest path capabilities.

Standard functions include `shortestPath()`, `allShortestPaths()`, and related path computation utilities defined in the OpenCypher specification. These functions are critical for graph applications requiring standardized pathfinding operations and compatibility with other OpenCypher implementations.

## Goals & Non-Goals

**Goals:**
- Implement OpenCypher standard `shortestPath()` function with proper syntax and semantics
- Add `allShortestPaths()` function for finding multiple minimum-length paths
- Support path pattern syntax as defined in OpenCypher specification
- Ensure full compatibility with OpenCypher shortest path query patterns
- Integrate with existing path variable assignment and processing
- Support both simple and complex path patterns in shortest path queries
- Provide proper error handling for invalid path patterns or unreachable nodes

**Non-Goals:**
- Custom shortest path algorithms beyond OpenCypher specification
- Advanced pathfinding optimizations (covered in separate algorithms initiative)
- Real-time dynamic shortest path updates
- Shortest path algorithms for weighted graphs beyond basic OpenCypher support

## Detailed Design

### OpenCypher Standard Functions
**shortestPath(pattern):** Find single shortest path matching pattern using BFS
**allShortestPaths(pattern):** Find all paths of minimum length matching pattern

### Implementation Approach
**Pattern Integration:** Extend existing pattern matching to support shortest path syntax
**BFS Implementation:** Use recursive CTEs for breadth-first pathfinding
**Path Assembly:** Build complete path objects with nodes and relationships
**Integration:** Leverage existing path variable and transformation infrastructure

## Alternatives Considered

**1. Custom Path Syntax:** Create non-standard shortest path syntax - Rejected: Must follow OpenCypher specification

**2. External Graph Libraries:** Use external pathfinding libraries - Rejected: Breaks self-contained architecture

**3. Pre-computed Paths:** Materialize shortest paths - Rejected: Dynamic computation needed for flexible patterns

## Implementation Plan

**Phase 1: Parser Extensions** - Extend grammar to support shortestPath() and allShortestPaths() syntax

**Phase 2: Core Implementation** - Implement BFS-based shortest path finding with pattern matching

**Phase 3: Path Assembly** - Build complete path objects and integrate with existing path infrastructure

**Phase 4: Testing and Compliance** - Comprehensive testing and OpenCypher specification validation

## Testing Strategy

**OpenCypher Compliance:** Validate against OpenCypher specification test cases for shortest path functions

**Pattern Correctness:** Test various path patterns, relationship types, and node constraints

**Performance:** Benchmark with different graph sizes and path complexity

**Integration:** Test with existing GraphQLite features and complex query patterns