---
id: add-unwind-for-list-processing
level: initiative
title: "Add UNWIND for List Processing"
short_code: "GQLITE-I-0003"
created_at: 2025-12-20T02:00:48.050159+00:00
updated_at: 2025-12-21T20:26:46.184005+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: add-unwind-for-list-processing
---

# Add UNWIND for List Processing Initiative

## Context

UNWIND functionality is missing from the current implementation, preventing list processing and iteration over collections. UNWIND is essential for transforming lists into rows, enabling batch operations, data transformation, and working with array-like data structures in graph queries.

## Goals & Non-Goals

**Goals:**
- Implement UNWIND clause syntax and parsing: `UNWIND list AS item`
- Support list literals, variables, and expressions as UNWIND sources
- Generate SQL that expands lists into multiple rows
- Enable list processing in query pipelines between MATCH and other clauses
- Support nested UNWIND operations for multi-dimensional data

**Non-Goals:**
- Complex list manipulation functions (slice, filter) - separate initiative
- Performance optimization for large lists
- Custom list serialization formats

## Detailed Design

Implement UNWIND as a row-generating clause that expands lists:

1. **Grammar Implementation** (cypher_gram.y):
   - Add UNWIND clause rule: `UNWIND expression AS identifier`
   - Support in query composition between other clauses
   - Handle list expressions, literals, and variables

2. **SQL Generation Strategy**:
   - For literal lists: Generate UNION ALL for each element
   - Example: `UNWIND [1, 2, 3] AS x` → `SELECT 1 AS x UNION ALL SELECT 2 AS x UNION ALL SELECT 3 AS x`
   - For JSON arrays: Use json_each() or similar table-valued functions

3. **Variable Scoping**:
   - UNWIND alias variable available in subsequent clauses
   - Preserve existing variables from before UNWIND
   - Handle variable name conflicts appropriately

## Alternatives Considered

1. **Application-level iteration**: Handle list expansion in application code - rejected as it breaks query composition
2. **Stored procedures with cursors**: Use database cursors - rejected due to complexity
3. **Temporary table approach**: Store list elements in temporary tables - rejected due to overhead

## Implementation Plan

1. **Grammar and AST Development**: Add UNWIND clause grammar rule
2. **List Literal Processing**: Implement SQL generation for literal lists using UNION ALL
3. **Transform Layer Integration**: Extend transform layer to handle UNWIND clauses
4. **Dynamic List Support**: Add support for variable lists and expressions

## Testing Strategy

1. **Basic UNWIND Operations**:
   - Simple list literals: `UNWIND [1, 2, 3] AS x RETURN x`
   - String lists: `UNWIND ['a', 'b', 'c'] AS item`

2. **Query Composition**:
   - UNWIND with MATCH: `UNWIND [1, 2] AS id MATCH (n) WHERE n.id = id`
   - Multiple UNWIND: `UNWIND [1, 2] AS x UNWIND [3, 4] AS y`

3. **Edge Cases**:
   - Empty lists: `UNWIND [] AS x` (should return no rows)
   - Null values in lists: `UNWIND [1, null, 3] AS x`