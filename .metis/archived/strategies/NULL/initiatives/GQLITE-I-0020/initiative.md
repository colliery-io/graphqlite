---
id: support-multiple-relationship-types
level: initiative
title: "Support Multiple Relationship Types"
short_code: "GQLITE-I-0020"
created_at: 2025-12-20T02:01:23.132400+00:00
updated_at: 2025-12-22T18:10:07.857514+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: support-multiple-relationship-types
---

# Support Multiple Relationship Types Initiative

## Context

The `[:TYPE1|TYPE2]` syntax for matching multiple relationship types is not working in the current implementation. This is a common OpenCypher pattern that allows queries to match relationships of different types in a single pattern, essential for flexible graph traversals.

## Goals & Non-Goals

**Goals:**
- Enable `[:TYPE1|TYPE2]` syntax for matching multiple relationship types
- Support unlimited number of relationship types in single pattern
- Generate efficient SQL with OR conditions for relationship type matching
- Maintain compatibility with single relationship type patterns
- Support in all relationship pattern contexts (MATCH, OPTIONAL MATCH, etc.)

**Non-Goals:**
- Regular expression patterns for relationship types
- Dynamic relationship type specification (runtime-determined types)
- Complex type hierarchies or inheritance

## Detailed Design

Fix and enhance relationship type parsing to support multiple types:

1. **Grammar Updates** (cypher_gram.y):
   - Fix relationship type list parsing in rel_type rule
   - Support pipe-separated type lists: `':' rel_type_name ('|' rel_type_name)*`
   - Ensure proper AST node creation for multiple types

2. **AST Structure Enhancement**:
   ```c
   typedef struct {
       char **type_names;        // Array of relationship type names
       size_t type_count;        // Number of types in array
   } cypher_rel_types_t;
   ```

3. **SQL Generation**:
   - Single type: `relationships.type = 'KNOWS'`
   - Multiple types: `relationships.type IN ('KNOWS', 'FRIENDS', 'FOLLOWS')`
   - Optimize with indexes on relationship type column

4. **Parser Integration**:
   - Ensure lexer properly tokenizes pipe (`|`) character
   - Handle whitespace around type separators

## Alternatives Considered

1. **Multiple MATCH clauses**: Use separate MATCH for each type and UNION - rejected as inefficient
2. **Regex-based matching**: Support pattern matching for types - rejected as non-standard
3. **Post-processing filter**: Filter types after SQL query - rejected due to performance
4. **Separate relationship tables**: Store types in separate tables - rejected as it breaks model

## Implementation Plan

1. **Grammar Fixes**: Fix rel_type rule in cypher_gram.y for pipe-separated lists
2. **AST Structure Updates**: Extend relationship type AST nodes for multiple types
3. **Transform Layer Enhancement**: Generate SQL IN clauses for type filtering
4. **Integration Testing**: Test queries with multiple relationship types

## Testing Strategy

**Basic Multiple Type Patterns:**
- Two types: `MATCH (a)-[:KNOWS|:FRIENDS]->(b)`
- Three types: `MATCH (a)-[:KNOWS|:FRIENDS|:FOLLOWS]->(b)`
- Many types: Test with 5+ relationship types

**Parser Validation:**
- Test whitespace handling around pipe separators
- Validate type name parsing with various identifiers
- Test error handling for invalid syntax

**SQL Generation Testing:**
- Verify IN clause generation for multiple types
- Test SQL correctness and performance

**Integration Scenarios:**
- Multiple type patterns with WHERE clauses
- Combination with other relationship features (properties, variables)
- Use in OPTIONAL MATCH contexts