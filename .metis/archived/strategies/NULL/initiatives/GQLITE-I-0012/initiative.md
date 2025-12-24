---
id: pattern-comprehensions
level: initiative
title: "Pattern Comprehensions"
short_code: "GQLITE-I-0012"
created_at: 2025-12-20T02:01:03.909755+00:00
updated_at: 2025-12-22T18:10:07.786676+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: L
strategy_id: NULL
initiative_id: pattern-comprehensions
---

# Pattern Comprehensions Initiative

## Status

**PARTIALLY IMPLEMENTED** - AST and transform layer are complete. Grammar rules are disabled due to LALR(1) parser limitations that cause ambiguity with list literals.

**Workaround**: Use MATCH with collect() function:
```cypher
-- Instead of: [(n)-[r]->(m) | m.name]
-- Use: MATCH (n)-[r]->(m) RETURN collect(m.name)
```

**Future Fix**: Enable grammar when switching to GLR parser or adopting alternative syntax.

## Context

Pattern comprehensions are missing from the current implementation, preventing creation of lists from graph patterns. Pattern comprehensions like `[(n)-[r]->(m) WHERE r.type = 'KNOWS' | m.name]` are an advanced OpenCypher feature that combines pattern matching with list comprehensions for sophisticated graph data collection and analysis.

## Goals & Non-Goals

**Goals:**
- Implement pattern comprehension syntax: `[pattern WHERE condition | expression]`
- Support graph pattern matching within list comprehension context
- Enable collection of values from matched graph patterns
- Generate SQL subqueries combining pattern matching with result aggregation
- Support complex patterns including multi-hop relationships and multiple nodes
- Handle variable scoping between pattern and comprehension contexts

**Non-Goals:**
- Performance optimization for complex pattern comprehensions (separate initiative)
- Pattern comprehensions with variable-length paths (advanced combination)
- Non-standard pattern comprehension extensions
- Recursive pattern comprehensions

## Detailed Design

Implement pattern comprehensions by combining pattern matching with list aggregation:

1. **Grammar Integration** (cypher_gram.y):
   - Pattern comprehension: `'[' pattern ['WHERE' condition] ['|' expression] ']'`
   - Reuse existing pattern parsing infrastructure within comprehension context
   - Handle variable scoping for pattern-bound variables

2. **AST Structure**:
   ```c
   typedef struct {
       cypher_astnode_t *pattern;         // Graph pattern to match
       cypher_astnode_t *where_expr;      // Optional filter condition
       cypher_astnode_t *collect_expr;    // Expression to collect
       cypher_astnode_t **variables;      // Variables introduced by pattern
       size_t variable_count;            // Number of pattern variables
   } cypher_ast_pattern_comprehension_t;
   ```

3. **SQL Generation Strategy**:
   - Generate subquery that matches the pattern and collects results
   - Use json_group_array() to aggregate matching results into JSON array
   - Example: `[(n)-[r]->(m) WHERE r.type = 'KNOWS' | m.name]`
   - Becomes: `SELECT json_group_array(m.name) FROM (SELECT ... FROM nodes n JOIN relationships r JOIN nodes m WHERE r.type = 'KNOWS')`

4. **Variable Scoping**:
   - Pattern variables (n, r, m) scoped only within the comprehension
   - Outer query variables accessible in WHERE and collect expressions
   - Handle name conflicts between pattern and outer variables

## Alternatives Considered

1. **Separate MATCH with COLLECT**: Use separate MATCH clauses with collect() aggregation - rejected as it's more verbose
2. **Nested query expansion**: Expand to explicit nested SELECT statements - rejected as it creates overly complex SQL
3. **Application-level pattern processing**: Handle pattern matching in application code - rejected as it prevents SQL optimization
4. **Materialized pattern results**: Pre-compute pattern results in temporary tables - rejected due to overhead

## Implementation Plan

1. **Grammar Integration and AST**: Add pattern comprehension grammar rules, implement AST structure
2. **Pattern Matching Integration**: Reuse existing pattern transformation logic within comprehension context
3. **SQL Subquery Generation**: Implement SQL generation using json_group_array() for result aggregation
4. **Advanced Pattern Support**: Support complex patterns with multiple nodes and relationships
5. **Variable Scoping and Integration**: Ensure proper variable scoping between pattern and outer query contexts

## Testing Strategy

**Basic Pattern Comprehensions:**
- Simple node patterns: `[(n:Person) | n.name]`
- Relationship patterns: `[(n)-[r]->(m) | m.name]`
- Empty pattern results (should return empty list)

**Pattern Variations:**
- Directed relationships: `[(n)-[:KNOWS]->(m) | m.name]`
- Undirected relationships: `[(n)-[:FRIENDS]-(m) | m.name]`
- Complex patterns: `[(n)-[r1]->(m)-[r2]->(p) | {from: n.name, to: p.name}]`

**WHERE Clause Filtering:**
- Property filtering: `[(n)-[r]->(m) WHERE m.age > 25 | m.name]`
- Complex conditions: `[(n)-[r]->(m) WHERE n.active AND m.age > 18 | m.name]`

**Variable Scoping Validation:**
- Pattern variables isolated within comprehension
- Outer query variables accessible in WHERE and collection expressions