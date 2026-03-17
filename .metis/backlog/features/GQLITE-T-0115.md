---
id: no-typed-edge-query-helpers-get
level: task
title: "No typed edge query helpers (get_edges_from, get_edges_to, get_edges_by_type)"
short_code: "GQLITE-T-0115"
created_at: 2026-03-17T01:30:31.649565+00:00
updated_at: 2026-03-17T02:21:52.085817+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# No typed edge query helpers (get_edges_from, get_edges_to, get_edges_by_type)

## Objective

Add convenience methods to query edges by source node, target node, or relationship type without dropping to raw Cypher.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Common graph operations require raw Cypher boilerplate. Edge-centric queries are fundamental graph operations.
- **Effort Estimate**: S

## Current State

**What exists (both bindings):**
- `has_edge(source, target, rel_type)` — existence check
- `get_edge(source, target, rel_type)` — single edge lookup
- `get_all_edges()` — returns everything (not useful at scale)
- `get_neighbors(node_id)` — returns neighbor nodes (not edges)

**Python-only:** `get_node_edges(node_id)` — returns all edges connected to a node as `list[tuple[str, str, dict]]`. This is a **Python-only method** not present in Rust.

**What's missing (both bindings):**
- `get_edges_from(node_id)` — all outgoing edges from a node
- `get_edges_to(node_id)` — all incoming edges to a node  
- `get_edges_by_type(node_id, rel_type)` — outgoing edges of a specific type

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `get_edges_from(node_id)` added to both Rust and Python
- [ ] `get_edges_to(node_id)` added to both Rust and Python
- [ ] `get_edges_by_type(node_id, rel_type)` added to both Rust and Python
- [ ] Port Python's `get_node_edges` to Rust for parity
- [ ] Tests for all new methods

## Implementation Notes

### Technical Approach
Simple Cypher wrappers: `MATCH (a {id: $id})-[r]->(b) RETURN ...` and reverse direction variant. Use parameterized queries internally.

## Status Updates

### Implementation Complete
- **Rust**: Added `get_edges_from`, `get_edges_to`, `get_edges_by_type`, `get_node_edges` to `queries.rs`
- **Python**: Added `get_edges_from`, `get_edges_to`, `get_edges_by_type` to `QueriesMixin`
- **Tests**: 25 Rust tests pass, 226 Python tests pass