---
id: case-expression-support
level: initiative
title: "CASE Expression Support"
short_code: "GQLITE-I-0004"
created_at: 2025-12-20T02:00:48.109739+00:00
updated_at: 2025-12-21T20:57:23.181613+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: case-expression-support
---

# CASE Expression Support Initiative

## Context

CASE expressions are fundamental conditional logic constructs in OpenCypher, enabling dynamic value selection based on conditions. The syntax `CASE WHEN condition THEN value ELSE default END` is essential for data transformation, conditional logic, and complex query patterns.

Currently, GraphQLite completely lacks CASE expression support, which significantly limits its ability to handle conditional logic in queries.

## Goals & Non-Goals

**Goals:**
- Implement basic CASE expression syntax: `CASE WHEN condition THEN value ELSE default END`
- Support multiple WHEN clauses
- Enable CASE expressions in RETURN, SET, and WHERE clauses
- Support nested CASE expressions for complex conditional logic
- Implement proper type checking and coercion for CASE result values

**Non-Goals:**
- Advanced pattern matching beyond basic conditional logic
- Custom CASE expression extensions not in OpenCypher specification
- Performance optimization for extremely complex nested CASE expressions

## Detailed Design

### Grammar Extensions

```yacc
case_expression:
    CASE when_list else_clause? END
    
when_clause:
    WHEN expression THEN expression
```

### AST Structure
```c
typedef struct cypher_case_expr {
    ast_node base;
    ast_list *when_clauses;     /* List of when/then pairs */
    ast_node *else_expr;        /* Optional ELSE expression */
} cypher_case_expr;
```

### SQL Generation Strategy
Direct mapping to SQL CASE:
```sql
CASE WHEN condition THEN value ELSE default END
```

## Alternatives Considered

1. **Function-Based Approach**: Implement CASE as special function - rejected as not OpenCypher compliant
2. **External Conditional Processing**: Handle in application layer - rejected for poor performance
3. **Multiple Query Approach**: Split into multiple queries - rejected for complexity

## Implementation Plan

1. **Grammar and AST**: Add CASE expression syntax to cypher_gram.y
2. **SQL Generation**: Implement CASE expression transformation to SQL
3. **Integration**: Integrate with existing expression evaluation
4. **Type System**: Implement proper type coercion for CASE result values

## Testing Strategy

1. **Syntax Validation**: Basic CASE expression parsing
2. **Functional Correctness**: CASE evaluation with various data types
3. **Integration Testing**: CASE in RETURN, WHERE, and SET clauses
4. **OpenCypher Compliance**: Cross-validation with specification