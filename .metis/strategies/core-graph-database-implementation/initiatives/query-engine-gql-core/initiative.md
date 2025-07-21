---
id: query-engine-gql-core
level: initiative
title: "Query Engine - GQL Core Implementation"
created_at: 2025-07-19T21:41:14.928422+00:00
updated_at: 2025-07-19T21:41:14.928422+00:00
parent: core-graph-database-implementation
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
---

# Query Engine - GQL Core Implementation Initiative

## Context

The Query Engine initiative transforms GraphQLite from a programmatic graph storage system into a complete graph database with GQL (Graph Query Language) support. While the Foundation Layer provides the storage and CRUD operations, the Query Engine adds the critical capability for developers to express complex graph queries using standard syntax.

GQL Core compliance is essential for GraphQLite's market positioning and developer adoption. The Query Engine must efficiently translate GQL patterns into optimized SQL queries that leverage our typed EAV storage and property-first optimization strategy validated in Phase Zero.

This initiative builds directly on the Foundation Layer and enables the final Integration & Polish phase by providing a complete, standards-compliant graph database that early adopters can evaluate and provide feedback on.

**Key Dependencies:**
- Foundation Layer typed EAV storage and indexing
- Property key interning system for efficient property filtering
- Dual-mode API for handling both interactive and bulk query scenarios

## Goals & Non-Goals

**Goals:**

**GQL Core Implementation:**
- Basic MATCH clause supporting node/edge pattern matching
- Variable-length path expressions (e.g., `-[*1..3]->`, `-[*]->`)
- WHERE clause with property filtering and boolean logic
- RETURN clause with column selection and aliasing
- OPTIONAL MATCH for optional pattern matching
- Proper GQL syntax error handling with informative messages

**Query Translation & Optimization:**
- Efficient GQL-to-SQL translation leveraging typed EAV storage
- Property-first optimization: filter by properties before graph traversal
- Variable-length path translation to recursive SQL CTEs
- Query plan optimization based on available indexes

**Performance Requirements:**
- All queries complete in <150ms on target hardware
- Property filtering queries under 5ms (maintaining Phase Zero performance)
- Variable-length path queries scale efficiently with path length
- Memory usage remains linear with result set size

**SQLite Extension Integration:**
- Register GQL query function for direct SQL usage
- Virtual table interface for GQL query results
- Seamless integration with existing SQLite tooling

**Non-Goals:**

**Advanced GQL Features (Deferred to Phase 2+):**
- Graph construction (CREATE, MERGE statements)
- Complex aggregation functions beyond basic COUNT/SUM
- Stored procedures or user-defined functions
- Full ISO GQL standard compliance beyond Core subset

**Complex Query Optimization:**
- Cost-based query optimization beyond property-first patterns
- Advanced join reordering or query plan analysis
- Adaptive query optimization based on runtime statistics

**Real-time Features:**
- Change notifications or subscriptions
- Streaming query results
- Incremental query updates

## Detailed Design

{Technical approach and implementation details}

## Alternatives Considered

{Alternative approaches and why they were rejected}

## Implementation Plan

{Phases and timeline for execution}

## Testing Strategy

{How the initiative will be validated}