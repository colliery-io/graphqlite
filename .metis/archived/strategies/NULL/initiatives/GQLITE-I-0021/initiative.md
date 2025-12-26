---
id: core-opencypher-compliance
level: initiative
title: "Core OpenCypher Compliance"
short_code: "GQLITE-I-0021"
created_at: 2025-12-22T18:33:16.792606+00:00
updated_at: 2025-12-22T19:03:26.147822+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: L
strategy_id: NULL
initiative_id: core-opencypher-compliance
---

# Core OpenCypher Compliance Initiative

## Context

GraphQLite implements most of OpenCypher but is missing several standard features that users expect. This initiative addresses the remaining gaps to achieve full OpenCypher compliance for core query functionality.

**Currently Working:**
- MATCH, CREATE, MERGE, DELETE, SET, REMOVE, DETACH DELETE
- WHERE, RETURN, ORDER BY, SKIP, LIMIT, WITH, UNWIND
- OPTIONAL MATCH, UNION
- Variable-length paths, shortestPath(), allShortestPaths()
- Pattern/List comprehensions, CASE, IS NULL/IS NOT NULL
- Regex `=~`, coalesce(), timestamp(), string/math functions

**Missing (this initiative):**
- List predicates: `all()`, `any()`, `none()`, `single()`
- List reduction: `reduce()`
- Boolean operator: `XOR`
- Temporal functions: `date()`, `time()`, `datetime()`, `duration()`
- Query analysis: `EXPLAIN`, `PROFILE`

## Goals & Non-Goals

**Goals:**
- Implement list predicate functions (all, any, none, single)
- Implement reduce() for list aggregation
- Add XOR boolean operator
- Add temporal functions (date, time, datetime, duration)
- Add EXPLAIN for query analysis

**Non-Goals:**
- CALL procedures (separate initiative - requires procedure framework)
- FOREACH clause (complex iteration semantics)
- CREATE INDEX/CONSTRAINT (schema DDL - separate initiative)
- Spatial functions point()/distance() (separate initiative)

## Detailed Design

### Phase 1: XOR Operator (Small)
Grammar addition only:
```yacc
expr: expr XOR expr { $$ = make_binary_op(OP_XOR, $1, $3); }
```
Transform: `($1) != ($3)` in SQL (XOR = not equal for booleans)

### Phase 2: List Predicates (Medium)
```cypher
all(x IN list WHERE predicate)   -- all elements satisfy
any(x IN list WHERE predicate)   -- at least one satisfies
none(x IN list WHERE predicate)  -- no elements satisfy
single(x IN list WHERE predicate) -- exactly one satisfies
```

SQL generation using json_each and aggregation:
```sql
-- all(x IN [1,2,3] WHERE x > 0)
(SELECT COUNT(*) = json_array_length('[1,2,3]') 
 FROM json_each('[1,2,3]') WHERE value > 0)
```

### Phase 3: reduce() Function (Medium)
```cypher
reduce(accumulator = initial, x IN list | expression)
```

SQL using recursive CTE or json_each with aggregation.

### Phase 4: Temporal Functions (Medium)
```cypher
date()                    -- current date
date('2024-01-15')       -- parse date
datetime()               -- current datetime
duration({days: 5})      -- duration literal
```

Map to SQLite date/time functions.

### Phase 5: EXPLAIN (Medium)
```cypher
EXPLAIN MATCH (n) RETURN n
```

Return generated SQL and SQLite query plan instead of executing.

## Alternatives Considered

1. **Skip list predicates** - Rejected: too commonly used in real queries
2. **Implement FOREACH first** - Rejected: higher complexity, lower usage
3. **Full GQL compliance** - Rejected: GQL is still evolving, focus on stable OpenCypher

## Implementation Plan

| Phase | Feature | Complexity | Files |
|-------|---------|------------|-------|
| 1 | XOR operator | S | cypher_gram.y, transform_return.c | ✅ DONE |
| 2 | all/any/none/single | M | cypher_gram.y, cypher_ast.c, transform_return.c | ✅ DONE |
| 3 | reduce() | M | cypher_gram.y, cypher_ast.c, transform_return.c | ✅ DONE |
| 4 | Temporal functions | M | transform_return.c | ✅ DONE |
| 5 | EXPLAIN | M | cypher_gram.y, cypher_executor.c, cypher_transform.c | ✅ DONE |

## Completion Notes

All phases completed on 2025-12-22:
- **XOR**: Implemented as `<>` comparison (works for boolean 0/1 values)
- **List predicates**: Uses `json_each()` with COUNT aggregation
- **reduce()**: Implemented using recursive CTE
- **Temporal**: Maps to SQLite date(), time(), datetime() functions
- **EXPLAIN**: Added `cypher_transform_generate_sql()` function to return SQL without executing

## Testing Strategy

Each phase includes unit tests validating:
- Parse correctness
- SQL generation
- Execution results matching Neo4j behavior