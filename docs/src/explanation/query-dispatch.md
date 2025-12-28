# Query Pattern Dispatch System

GraphQLite uses a table-driven pattern dispatch system to execute Cypher queries. This document describes how the system works and how to extend it.

## Overview

Instead of a massive if-else chain checking clause combinations, queries are matched against a registry of patterns. Each pattern defines:

- **Required clauses**: Must all be present
- **Forbidden clauses**: Must all be absent
- **Priority**: Higher priority patterns are checked first
- **Handler**: Function to execute the query

## Supported Query Patterns

| Pattern | Required | Forbidden | Priority | Description |
|---------|----------|-----------|----------|-------------|
| `UNWIND+CREATE` | UNWIND, CREATE | RETURN, MATCH | 100 | Batch node/edge creation |
| `WITH+MATCH+RETURN` | WITH, MATCH, RETURN | - | 100 | Subquery pipeline |
| `MATCH+CREATE+RETURN` | MATCH, CREATE, RETURN | - | 100 | Match then create with results |
| `MATCH+SET` | MATCH, SET | - | 90 | Update matched nodes |
| `MATCH+DELETE` | MATCH, DELETE | - | 90 | Delete matched nodes |
| `MATCH+REMOVE` | MATCH, REMOVE | - | 90 | Remove properties/labels |
| `MATCH+MERGE` | MATCH, MERGE | - | 90 | Conditional create/match |
| `MATCH+CREATE` | MATCH, CREATE | RETURN | 90 | Match then create |
| `OPTIONAL_MATCH+RETURN` | MATCH, OPTIONAL, RETURN | CREATE, SET, DELETE, MERGE | 80 | Left join pattern |
| `MULTI_MATCH+RETURN` | MATCH, MULTI_MATCH, RETURN | CREATE, SET, DELETE, MERGE | 80 | Multiple match clauses |
| `MATCH+RETURN` | MATCH, RETURN | OPTIONAL, MULTI_MATCH, CREATE, SET, DELETE, MERGE | 70 | Simple query |
| `UNWIND+RETURN` | UNWIND, RETURN | CREATE | 60 | List processing |
| `CREATE` | CREATE | MATCH, UNWIND | 50 | Create nodes/edges |
| `MERGE` | MERGE | MATCH | 50 | Merge nodes/edges |
| `SET` | SET | MATCH | 50 | Standalone set |
| `FOREACH` | FOREACH | - | 50 | Iterate and update |
| `MATCH` | MATCH | RETURN, CREATE, SET, DELETE, MERGE, REMOVE | 40 | Match without return |
| `RETURN` | RETURN | MATCH, UNWIND, WITH | 10 | Expressions, graph algorithms |
| `GENERIC` | - | - | 0 | Fallback for any query |

## How Pattern Matching Works

1. **Analyze**: Extract clause flags from query AST
2. **Match**: Find highest-priority pattern where:
   - All required flags are present
   - No forbidden flags are present
3. **Execute**: Call the pattern's handler function

```c
clause_flags flags = analyze_query_clauses(query);
const query_pattern *pattern = find_matching_pattern(flags);
return pattern->handler(executor, query, result, flags);
```

## Debugging

### Debug Logging

With `GRAPHQLITE_DEBUG` defined, pattern matching logs:

```
[CYPHER_DEBUG] Query clauses: MATCH|RETURN
[CYPHER_DEBUG] Matched pattern: MATCH+RETURN (priority 70)
```

### EXPLAIN Command

Use `EXPLAIN` to see pattern info without executing:

```sql
SELECT cypher('EXPLAIN MATCH (n:Person) RETURN n.name');
```

Output:
```
Pattern: MATCH+RETURN
Clauses: MATCH|RETURN
SQL: SELECT ... FROM nodes ...
```

## Adding New Patterns

### Step 1: Define the Pattern

Add an entry to the `patterns[]` array in `query_dispatch.c`:

```c
{
    .name = "MY_PATTERN",
    .required = CLAUSE_MATCH | CLAUSE_CUSTOM,
    .forbidden = CLAUSE_DELETE,
    .handler = handle_my_pattern,
    .priority = 85
}
```

### Step 2: Implement the Handler

```c
static int handle_my_pattern(cypher_executor *executor,
                             cypher_query *query,
                             cypher_result *result,
                             clause_flags flags)
{
    (void)flags;
    CYPHER_DEBUG("Executing MY_PATTERN via pattern dispatch");

    // Implementation here

    result->success = true;
    return 0;
}
```

### Step 3: Add Tests

Add tests to `test_query_dispatch.c`:

```c
static void test_pattern_my_pattern(void)
{
    const query_pattern *p = find_matching_pattern(CLAUSE_MATCH | CLAUSE_CUSTOM);
    CU_ASSERT_PTR_NOT_NULL(p);
    if (p) {
        CU_ASSERT_STRING_EQUAL(p->name, "MY_PATTERN");
        CU_ASSERT_EQUAL(p->priority, 85);
    }
}
```

## Priority Guidelines

| Priority | Use Case |
|----------|----------|
| 100 | Most specific multi-clause combinations |
| 90 | MATCH + write operation patterns |
| 80 | Complex read patterns (OPTIONAL, multi-MATCH) |
| 70 | Simple read patterns |
| 50-60 | Standalone clauses with modifiers |
| 40-50 | Standalone write clauses |
| 10 | Expressions and algorithms |
| 0 | Generic fallback |

## Files

- `src/include/executor/query_patterns.h` - Types and API
- `src/backend/executor/query_dispatch.c` - Pattern registry and handlers
- `tests/test_query_dispatch.c` - Unit tests

## Graph Algorithm Handling

Graph algorithms (PageRank, Dijkstra, etc.) are detected within the `RETURN` pattern handler. When a RETURN-only query contains a graph algorithm function call, it's executed via the C-based algorithm implementations for performance.
