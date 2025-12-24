---
id: implement-merge-functionality
level: initiative
title: "Implement MERGE Functionality"
short_code: "GQLITE-I-0005"
created_at: 2025-12-20T02:00:48.170617+00:00
updated_at: 2025-12-21T22:57:32.759779+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: L
strategy_id: NULL
initiative_id: implement-merge-functionality
---

# Implement MERGE Functionality Initiative

## Context

MERGE functionality is missing from the current implementation, preventing upsert operations on nodes and relationships. MERGE is essential for data integration scenarios where you want to create entities if they don't exist or match existing ones, combining the semantics of MATCH and CREATE in a single atomic operation.

## Goals & Non-Goals

**Goals:**
- Implement MERGE clause syntax and parsing for nodes and relationships
- Support basic MERGE pattern: `MERGE (n:Label {property: value})`
- Implement ON CREATE and ON MATCH clauses for conditional actions
- Generate SQL upsert logic using INSERT OR IGNORE + UPDATE patterns
- Ensure atomic upsert behavior to prevent race conditions

**Non-Goals:**
- MERGE performance optimization (separate initiative)
- Complex MERGE patterns with multiple disconnected components
- MERGE with variable-length paths (advanced feature)

## Detailed Design

1. **Grammar Implementation** (cypher_gram.y):
   - Add MERGE clause rule: `MERGE pattern [ON CREATE set_clause] [ON MATCH set_clause]`
   - Support ON CREATE and ON MATCH sub-clauses

2. **AST Structure**:
   ```c
   typedef struct {
       cypher_astnode_t *pattern;       // Pattern to merge
       cypher_astnode_t *on_create;     // Actions if created
       cypher_astnode_t *on_match;      // Actions if matched
   } cypher_ast_merge_t;
   ```

3. **SQL Generation Approach**:
   - For nodes: `INSERT OR IGNORE` followed by conditional UPDATE
   - Handle unique constraints and primary keys properly

## Alternatives Considered

1. **Application-level upsert**: Handle in application code - rejected as it breaks transactional guarantees
2. **Two-phase approach**: Separate SELECT then INSERT/UPDATE - rejected due to race conditions
3. **REPLACE statement**: Use SQL REPLACE - rejected as it doesn't provide ON CREATE/ON MATCH semantics

## Implementation Plan

1. **Grammar and AST Development**: Add MERGE clause grammar rules
2. **Basic MERGE Transform**: Implement for simple node patterns
3. **ON CREATE/ON MATCH Implementation**: Add conditional SET clause execution
4. **Relationship MERGE Support**: Extend to handle relationship patterns

## Testing Strategy

1. **Basic MERGE Operations**:
   - Node creation: `MERGE (n:Person {name: 'John'})` when node doesn't exist
   - Node matching: Same MERGE when node already exists

2. **ON CREATE/ON MATCH Testing**:
   - `MERGE (n:Person {name: 'John'}) ON CREATE SET n.created = timestamp()`
   - `MERGE (n:Person {name: 'John'}) ON MATCH SET n.accessed = timestamp()`

3. **Atomicity and Consistency**:
   - Concurrent MERGE operations on same entity
   - Transaction rollback behavior