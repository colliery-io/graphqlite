---
id: standard-list-and-utility-functions
level: initiative
title: "Standard List and Utility Functions"
short_code: "GQLITE-I-0009"
created_at: 2025-12-20T02:01:03.720120+00:00
updated_at: 2025-12-22T02:43:05.247572+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: standard-list-and-utility-functions
---

# Standard List and Utility Functions Initiative

## Context

The OpenCypher specification defines comprehensive list processing and utility functions that are essential for data manipulation and analysis in graph queries. Currently, GraphQLite lacks most of these functions, severely limiting its ability to handle complex data transformations and list operations that are fundamental to modern graph processing.

Missing functions include list manipulation (`head()`, `tail()`, `last()`, `size()`), list construction (`range()`, `collect()`), filtering and transformation utilities (`filter()`, `reduce()`), and general utility functions (`coalesce()`, `timestamp()`, `randomUUID()`). These functions are critical for:
- Processing variable-length path results and node/relationship collections
- Data transformation and aggregation in graph queries
- Building dynamic queries with computed values
- Ensuring compatibility with standard OpenCypher applications and tools

## Goals & Non-Goals

**Goals:**
- Implement core list functions: `head()`, `tail()`, `last()`, `size()`, `reverse()`
- Add list construction functions: `range()`, `collect()`, `extract()`
- Create filtering and transformation functions: `filter()`, `reduce()`, `any()`, `all()`, `none()`, `single()`
- Implement utility functions: `coalesce()`, `timestamp()`, `randomUUID()`, `toInteger()`, `toFloat()`, `toString()`, `toBoolean()`
- Support list comprehension-style operations for data transformation
- Ensure full compatibility with OpenCypher list function semantics

**Non-Goals:**
- Advanced functional programming constructs beyond OpenCypher specification
- Custom list processing functions not defined in OpenCypher standard
- Performance optimization for massive lists (>10K elements) - focus on correctness
- Complex nested list operations beyond standard specification

## Detailed Design

### List Functions Implementation

**Core List Operations:**
- `size(list)` - Return list length using SQLite JSON array length
- `head(list)` - Get first element using JSON extraction
- `tail(list)` - Return list without first element using JSON manipulation
- `last(list)` - Get last element using JSON array indexing
- `reverse(list)` - Reverse list order using SQL array processing

**List Construction:**
- `range(start, end)` - Generate integer sequence using recursive CTE
- `collect(expr)` - Aggregate values into JSON array (extend existing aggregation)
- `extract(var IN list | expr)` - Transform list elements using subquery logic

**Utility Functions:**
- `coalesce(expr1, expr2, ...)` - Return first non-null value using SQL COALESCE
- `timestamp()` - Current Unix timestamp using SQLite datetime functions
- `randomUUID()` - Generate UUID using SQLite hex and random functions
- Type conversion functions using SQLite CAST operations

### SQL Implementation Patterns

**JSON-Based List Processing:**
```sql
-- List size using SQLite JSON functions
SELECT json_array_length(list_column)

-- Head/tail operations
SELECT json_extract(list_column, '$[0]') as head,
       json_extract(list_column, '$[1:]') as tail
```

**Function Integration:**
- Extend transform_function_call() with new function handlers
- Support JSON list representation throughout query pipeline
- Add list literal parsing and generation capabilities

## Alternatives Considered

**1. Array-Based Implementation:** Use SQLite array types instead of JSON - Rejected: SQLite doesn't have native array types, JSON is more portable

**2. External Processing Libraries:** Use external list processing - Rejected: Breaks self-contained architecture

**3. Custom List Storage:** Create dedicated list tables - Rejected: Adds complexity without clear benefits over JSON

## Implementation Plan

**Phase 1: Core List Functions** - Implement size(), head(), tail(), last(), reverse()

**Phase 2: Utility Functions** - Add coalesce(), timestamp(), randomUUID(), type conversion functions

**Phase 3: List Construction** - Implement range(), collect(), extract()

**Phase 4: Advanced List Operations** - Add filter(), reduce(), any(), all(), none(), single()

## Testing Strategy

**Function Correctness:** Validate against OpenCypher specification examples for all list functions

**Edge Cases:** Empty lists, single-element lists, large lists, null handling

**Integration:** Function composition, complex queries, performance with typical list sizes

**Compliance:** Cross-validation with Apache AGE and other OpenCypher implementations