---
id: add-with-clause-support
level: initiative
title: "Add WITH Clause Support"
short_code: "GQLITE-I-0002"
created_at: 2025-12-20T02:00:47.995119+00:00
updated_at: 2025-12-21T18:54:28.290322+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: add-with-clause-support
---

# Add WITH Clause Support Initiative

## Context

The WITH clause is missing from the current implementation, preventing query composition and intermediate result processing. WITH is essential for complex queries that need to aggregate, filter, or transform data between MATCH clauses, enabling more sophisticated graph analysis patterns.

## Goals & Non-Goals

**Goals:**
- Implement WITH clause syntax and parsing in cypher_gram.y
- Support query composition between MATCH clauses with intermediate results
- Enable aggregation and filtering in WITH clauses (COUNT, WHERE, etc.)
- Implement proper variable scoping - WITH defines new scope boundary
- Generate SQL subqueries or CTEs for WITH clause functionality
- Support DISTINCT and ORDER BY within WITH clauses

**Non-Goals:**
- Complex query optimization across WITH boundaries (future performance initiative)
- WITH clause performance tuning and optimization
- Non-standard WITH extensions or custom syntax

## Detailed Design

Implement WITH clause as a query composition mechanism:

1. **Grammar Integration** (cypher_gram.y):
   - Add WITH clause rule: `WITH projection_body [WHERE where_expression]`
   - Support query chaining: `query_part WITH query_part*`
   - Handle variable projection and aliasing in WITH

2. **AST Structure**:
   ```c
   typedef struct {
       cypher_astnode_t *projection;    // Variables/expressions to project
       cypher_astnode_t *where;         // Optional WHERE filter
       cypher_astnode_t *order_by;      // Optional ORDER BY
       bool distinct;                   // DISTINCT flag
   } cypher_ast_with_t;
   ```

3. **Variable Scoping**:
   - WITH creates new variable scope - only projected variables visible after
   - Symbol table management for scope boundaries
   - Variable name resolution across WITH boundaries

4. **SQL Generation Strategy**:
   - Generate Common Table Expressions (CTEs) for WITH clauses
   - Example: `MATCH (n) WITH n.name AS name WHERE length(name) > 5`
   - Becomes: `WITH intermediate AS (SELECT n.name AS name FROM nodes n) SELECT * FROM intermediate WHERE length(name) > 5`

## Alternatives Considered

1. **Nested subqueries**: Use regular subqueries instead of CTEs - rejected due to variable scoping complexity
2. **Query splitting**: Split WITH queries into separate SQL statements - rejected as it breaks transactional consistency
3. **View-based approach**: Create temporary views for each WITH clause - rejected due to naming conflicts

## Implementation Plan

1. **Grammar and AST Development**: Add WITH clause grammar rules, implement AST structure
2. **Variable Scoping Implementation**: Extend symbol table for scope boundaries
3. **Transform Layer Integration**: Add WITH clause transformation to generate CTEs
4. **SQL Generation and Testing**: Implement CTE generation, test query composition

## Implementation Progress

**Completed:**
- ✅ Grammar integration: Added `with_clause` rule to cypher_gram.y
- ✅ AST structure: Added `cypher_with` struct with distinct, items, order_by, skip, limit, where fields
- ✅ AST constructor: Implemented `make_cypher_with()` in cypher_ast.c  
- ✅ Memory management: Added `AST_NODE_WITH` case to ast_node_free()
- ✅ Transform integration: Added WITH case to clause dispatcher in cypher_transform.c
- ✅ SQL generation: Implemented `transform_with_clause()` generating CTEs
- ✅ Parser tests: 5 tests added and passing (basic, alias, distinct, where, order+limit)

**Key Implementation Details:**
- WITH generates CTE (Common Table Expression) for query composition
- Supports DISTINCT, ORDER BY, SKIP, LIMIT, and WHERE clauses
- Variable projections become CTE columns
- Subsequent clauses query from the generated CTE

## Testing Strategy

1. **Basic WITH Functionality**:
   - Simple projection: `MATCH (n) WITH n.name AS name RETURN name`
   - Variable aliasing: `MATCH (n) WITH n AS node, n.age AS age`
   - Filtering with WHERE: `MATCH (n) WITH n WHERE n.age > 30`

2. **Aggregation in WITH**:
   - Count aggregation: `MATCH (n) WITH count(n) AS total`
   - Grouping: `MATCH (n) WITH n.type, count(n) AS count`

3. **Query Composition**:
   - Multiple WITH clauses: `MATCH (a) WITH a MATCH (a)-[]->(b) WITH a, b`
   - Integration with OPTIONAL MATCH

4. **Variable Scoping**:
   - Test that non-projected variables are not accessible after WITH
   - Validate variable aliasing and renaming