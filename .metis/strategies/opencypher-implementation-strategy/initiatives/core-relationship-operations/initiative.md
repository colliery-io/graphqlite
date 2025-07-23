---
id: core-relationship-operations
level: initiative
title: "Core Relationship Operations (CREATE/MATCH/DELETE)"
created_at: 2025-07-23T15:03:31.862844+00:00
updated_at: 2025-07-23T15:03:31.862844+00:00
parent: opencypher-implementation-strategy
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
---

# Core Relationship Operations (CREATE/MATCH/DELETE) Initiative

## Context

Knowledge graphs for RAG systems fundamentally depend on rich relationship modeling to represent semantic connections between entities. While our current implementation can CREATE simple relationships and has partial MATCH support, we lack complete end-to-end relationship operations essential for RAG applications.

**Current State:**
- Relationship CREATE operations work but have limited pattern support
- Relationship MATCH operations are parsed but execution is incomplete
- No DELETE support for relationships (critical for knowledge graph maintenance)
- Missing relationship property updates via SET operations
- Limited relationship pattern matching for complex graph traversals

**RAG System Context:**
RAG applications need to model complex semantic relationships like:
- `(entity:Concept)-[:RELATED_TO {similarity: 0.8}]->(concept:Concept)`
- `(document:Document)-[:CONTAINS]->(entity:Entity)-[:INSTANCE_OF]->(type:EntityType)`
- `(query:Query)-[:SEMANTICALLY_SIMILAR {score: 0.9}]->(document:Document)`

Without robust relationship operations, RAG systems cannot effectively represent and query the semantic connections that drive contextual retrieval and reasoning.

## Goals & Non-Goals

**Goals:**
- Complete end-to-end relationship MATCH execution with full pattern support
- Implement relationship DELETE operations for graph maintenance
- Add relationship property updates via SET operations
- Support bidirectional and undirected relationship matching
- Enable complex relationship patterns: `(a)-[r:TYPE {prop: value}]->(b)`
- Optimize relationship queries for RAG retrieval performance (<50ms for typical patterns)
- Comprehensive test coverage for all relationship operations

**Non-Goals:**
- Variable-length path traversals (`[*1..5]`) - separate initiative
- Complex graph algorithms (shortest path, centrality) - future scope
- Relationship constraints and validation - schema initiative
- Multi-hop optimization beyond basic indexing - performance initiative
- Relationship aggregation functions - analytics initiative

## Detailed Design

**Technical Architecture:**

1. **Complete Relationship MATCH Execution**
   - Extend existing `execute_match_relationship_with_where()` to handle all relationship patterns
   - Support for relationship variable binding: `MATCH (a)-[r:KNOWS]->(b) RETURN r`
   - Bidirectional matching: `MATCH (a)-[r:CONNECTS]-(b)` (either direction)
   - Property-based relationship filtering integrated with WHERE clause engine

2. **Relationship DELETE Operations**
   - New AST node type: `AST_DELETE_STATEMENT`
   - Grammar extension: `DELETE r` where r is relationship variable
   - Cascade deletion handling (delete relationship but preserve nodes)
   - Transaction safety for relationship removal

3. **Relationship SET Operations**
   - Extend existing SET grammar for relationship properties
   - Support: `MATCH (a)-[r:KNOWS]->(b) SET r.strength = 0.9`
   - Property type validation and conversion
   - Atomic property updates within transactions

4. **Database Schema Enhancements**
   - Index optimization for relationship type and property queries
   - Efficient bidirectional relationship lookups
   - Property key interning for relationship properties

**Implementation Phases:**
1. Complete relationship MATCH execution engine
2. Add relationship DELETE operations
3. Implement relationship SET operations
4. Optimize relationship queries for performance
5. Comprehensive testing and validation

## Alternatives Considered

**Alternative 1: Deferred Relationship Operations**
- Approach: Focus only on node operations, defer relationship complexity
- Rejected: RAG systems fundamentally require rich relationship modeling for semantic connections
- Impact: Would severely limit RAG use cases and competitive positioning

**Alternative 2: Simplified DELETE (Cascade All)**
- Approach: DELETE relationship always deletes connected nodes  
- Rejected: Breaks graph integrity and typical RAG workflows where entities persist across relationships
- Impact: Would make knowledge graph maintenance impossible

**Alternative 3: Relationship-Only SET Operations**
- Approach: Implement SET only for relationships, not nodes
- Rejected: Inconsistent API and nodes also need property updates for RAG metadata
- Impact: Poor developer experience and incomplete OpenCypher compliance

**Alternative 4: SQL-Only Implementation**
- Approach: Implement relationship operations purely through SQL without C execution engine
- Rejected: Complex relationship patterns perform poorly in pure SQL and lack needed flexibility
- Impact: Would not meet performance targets for RAG retrieval (<50ms)

## Implementation Plan

**Phase 1: Complete Relationship MATCH Execution (Week 1-2)**
- Finish implementing `execute_match_relationship_with_where()`
- Add support for relationship variable binding and return
- Implement bidirectional relationship matching
- Integration testing with existing WHERE clause engine

**Phase 2: Relationship DELETE Operations (Week 3)**
- Extend BISON grammar for DELETE statements
- Create AST nodes for DELETE operations
- Implement relationship deletion in execution engine
- Add transaction safety and error handling

**Phase 3: Relationship SET Operations (Week 4)**
- Extend existing SET grammar for relationship properties
- Implement relationship property updates in execution engine
- Add type validation and conversion for relationship properties
- Integration with property key interning system

**Phase 4: Performance Optimization (Week 5)**
- Add relationship type and property indexing
- Optimize bidirectional relationship queries
- Profile and tune for RAG retrieval performance targets
- Memory usage optimization for relationship operations

**Phase 5: Testing and Validation (Week 6)**
- Comprehensive CUnit test suite for all relationship operations
- SQL functional tests covering RAG use cases
- Performance benchmarking against targets
- Integration testing with complete OpenCypher query workflows

## Testing Strategy

**Unit Testing (CUnit Framework)**
- Test relationship MATCH execution with various pattern combinations
- Validate relationship DELETE operations with edge cases (non-existent relationships, cascade behavior)
- Test relationship SET operations with different property types and validation
- Memory leak testing for all relationship operations
- Error handling validation for malformed queries

**Functional Testing (SQL Test Suite)**
- RAG-specific test scenarios: document-entity-concept relationship chains
- Bidirectional relationship matching validation
- Complex relationship patterns with WHERE clause filtering
- Performance regression tests for relationship queries
- Transaction isolation testing for concurrent relationship operations

**Integration Testing**
- End-to-end workflows combining CREATE, MATCH, SET, DELETE operations
- Integration with existing WHERE clause engine
- Relationship operations combined with node operations
- Cross-pattern testing (nodes and relationships in same queries)

**Performance Validation**
- Benchmark relationship MATCH queries against <50ms target
- Memory usage profiling for large relationship datasets
- Scalability testing with increasing relationship density
- Comparison testing against current partial implementation

**RAG System Validation**
- Test semantic similarity relationship storage and retrieval
- Document-entity relationship modeling scenarios
- Knowledge graph maintenance workflows (adding/removing relationships)
- Query pattern validation for typical RAG retrieval patterns
