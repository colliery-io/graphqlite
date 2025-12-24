---
id: core-path-and-entity-functions
level: initiative
title: "Core Path and Entity Functions"
short_code: "GQLITE-I-0008"
created_at: 2025-12-20T02:01:03.658494+00:00
updated_at: 2025-12-22T02:38:41.348275+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: core-path-and-entity-functions
---

# Core Path and Entity Functions Initiative

## Context

The OpenCypher specification defines essential functions for working with paths, nodes, and relationships that are fundamental to graph querying and analysis. Currently, GraphQLite is missing approximately 90% of these core functions, severely limiting OpenCypher standard compliance and functionality.

## Goals & Non-Goals

**Goals:**
- Implement core path functions: `nodes()`, `relationships()`, `length()`, `startNode()`, `endNode()`
- Add entity introspection functions: `id()`, `labels()`, `properties()`, `keys()`
- Create relationship inspection functions: `type()`, `startNode()`, `endNode()`
- Support path construction and manipulation utilities
- Enable proper variable-length path result processing

**Non-Goals:**
- Advanced path pattern matching (covered in separate pattern initiatives)
- Complex path algorithms beyond basic introspection
- Path comparison and equality operators (separate initiative)

## Detailed Design

### Path Functions
- `length(path)` - Return number of relationships in path
- `nodes(path)` - Extract all nodes from path as list
- `relationships(path)` - Extract all relationships from path as list
- `startNode(path)` - Get first node in path
- `endNode(path)` - Get last node in path

### Entity Introspection Functions
- `id(node/relationship)` - Get internal ID of entity
- `labels(node)` - Get list of labels for node
- `properties(node/relationship)` - Get property map
- `keys(node/relationship)` - Get list of property keys

### Implementation
- Path functions work with JSON path representations stored in variables
- Entity functions query appropriate tables (nodes, edges, node_labels, property tables)
- Support both scalar returns (id, length) and list returns (nodes, labels)

## Alternatives Considered

1. **Eager path materialization**: Store full path data in memory - Rejected for memory concerns
2. **Lazy evaluation**: Compute path functions on demand - Selected for efficiency
3. **External path processing**: Handle in application layer - Rejected for SQL composition

## Implementation Plan

1. **Path Functions**: Implement nodes(), relationships(), length()
2. **Entity Functions**: Add id(), labels(), properties(), keys()
3. **Path Node Access**: startNode(), endNode()
4. **Integration**: Connect with variable-length path queries

## Testing Strategy

- Test path functions with simple and variable-length paths
- Validate entity introspection against stored data
- Test list returns are properly formatted
- Verify integration with RETURN clause processing