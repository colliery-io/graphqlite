---
id: implement-shortest-path-dijkstra
level: task
title: "Implement Shortest Path (Dijkstra) Algorithm"
short_code: "GQLITE-T-0020"
created_at: 2025-12-24T22:50:01.156723+00:00
updated_at: 2025-12-24T22:50:01.156723+00:00
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

# Implement Shortest Path (Dijkstra) Algorithm

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Implement Dijkstra's shortest path algorithm in the C extension to find the minimum-weight path between two nodes in the graph.

## Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P1 - High (important for user experience)

### Cypher Syntax
```cypher
RETURN shortestPath(source_id, target_id)
RETURN shortestPath(source_id, target_id, 'weight')  -- with edge weight property
```

### Return Format
```json
{"path": ["node1", "node2", "node3"], "distance": 2.5}
```

### Reference
- Neo4j GDS: Dijkstra Source-Target
- Apache AGE: Supports via custom implementation

## Acceptance Criteria

- [ ] Implement `shortestPath(source, target)` function in C
- [ ] Support optional edge weight property parameter
- [ ] Return path as array of node IDs plus total distance
- [ ] Handle disconnected nodes (return null/empty)
- [ ] Add Python binding wrapper
- [ ] Add unit tests

## Implementation Notes

### Technical Approach
- Use priority queue (min-heap) for O((V+E) log V) complexity
- Leverage existing CSR graph structure from `graph_algorithms.c`
- Add to `transform_func_graph.c` for Cypher integration

## Status Updates **[REQUIRED]**

*To be added during implementation*