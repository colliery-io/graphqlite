---
id: query-pattern-dispatcher-for
level: initiative
title: "Query Pattern Dispatcher for Executor"
short_code: "GQLITE-I-0026"
created_at: 2025-12-26T04:50:28.831086+00:00
updated_at: 2025-12-26T04:50:28.831086+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
strategy_id: NULL
initiative_id: query-pattern-dispatcher-for
---

# Query Pattern Dispatcher for Executor Initiative

## Context

The executor layer in `cypher_executor.c` uses a **massive if-else chain** to determine how to execute queries based on which clauses are present. The function `cypher_executor_execute_ast()` spans 500+ lines with hardcoded pattern matching:

### Current Pattern Matching (cypher_executor.c:160-750)

```c
// Pattern 1: UNWIND + CREATE (no RETURN)
if (unwind_clause && create_clause && !return_clause) {
    // 70 lines of direct execution logic
}

// Pattern 2: MATCH + RETURN (simple case)
if (match_clause && return_clause && !create_clause && !set_clause && !delete_clause) {
    // Check for OPTIONAL MATCH...
    if (has_optional_match || match_clause_count > 1) {
        // Use transform pipeline
    } else {
        // Use direct execution
    }
}

// Pattern 3: MATCH + CREATE
if (match_clause && create_clause) {
    return execute_match_create_query(executor, query, result);
}

// Pattern 4: MATCH + SET
if (match_clause && set_clause) {
    return execute_match_set_query(executor, query, result);
}

// ... 10+ more patterns
```

### Problems

1. **500+ lines of pattern matching**: Hard to understand query flow
2. **Duplicate setup code**: Each pattern repeats transform context creation
3. **Inconsistent execution paths**: Some patterns use helpers, others inline
4. **Adding patterns is error-prone**: Must find correct position in if-else chain
5. **No visibility into supported patterns**: Implicit in code structure
6. **Testing gaps**: Easy to miss edge cases between patterns

### Specialized Executors

Additionally, there are 15+ specialized executor files:
- `execute_match_create_query()`
- `execute_match_set_query()`
- `execute_match_delete_query()`
- `execute_match_merge_query()`
- Graph algorithm executors (20+)

These are better structured but still dispatched via the main if-else chain.

## Goals & Non-Goals

**Goals:**
- Replace if-else chain with table-driven pattern dispatch
- Make supported query patterns explicit and documented
- Reduce boilerplate in pattern handlers
- Enable easier addition of new query patterns
- Improve testability of individual patterns

**Non-Goals:**
- Query optimization
- Changing transform layer interface
- Modifying graph algorithm dispatch (already well-structured)
- Supporting dynamic pattern registration (future consideration)

## Architecture

### Query Pattern Definition

```c
// Clause presence flags
typedef enum {
    CLAUSE_MATCH       = 1 << 0,
    CLAUSE_OPTIONAL    = 1 << 1,  // Has OPTIONAL MATCH
    CLAUSE_RETURN      = 1 << 2,
    CLAUSE_CREATE      = 1 << 3,
    CLAUSE_MERGE       = 1 << 4,
    CLAUSE_SET         = 1 << 5,
    CLAUSE_DELETE      = 1 << 6,
    CLAUSE_REMOVE      = 1 << 7,
    CLAUSE_WITH        = 1 << 8,
    CLAUSE_UNWIND      = 1 << 9,
    CLAUSE_FOREACH     = 1 << 10,
    CLAUSE_UNION       = 1 << 11,
    CLAUSE_CALL        = 1 << 12,
} clause_flags;

// Pattern handler signature
typedef int (*pattern_handler)(
    cypher_executor *executor,
    cypher_query *query,
    cypher_result *result,
    clause_flags present
);

// Pattern definition
typedef struct {
    const char *name;           // For debugging/logging
    clause_flags required;      // Must have these clauses
    clause_flags forbidden;     // Must NOT have these clauses
    clause_flags optional;      // May have these (don't affect matching)
    pattern_handler handler;
    int priority;               // Higher = checked first
} query_pattern;
```

### Pattern Registry

```c
static const query_pattern patterns[] = {
    // Specific patterns first (higher priority)
    {
        .name = "UNWIND+CREATE",
        .required = CLAUSE_UNWIND | CLAUSE_CREATE,
        .forbidden = CLAUSE_RETURN | CLAUSE_MATCH,
        .handler = execute_unwind_create,
        .priority = 100
    },
    {
        .name = "MATCH+OPTIONAL+RETURN",
        .required = CLAUSE_MATCH | CLAUSE_OPTIONAL | CLAUSE_RETURN,
        .forbidden = CLAUSE_CREATE | CLAUSE_SET | CLAUSE_DELETE,
        .handler = execute_match_optional_return,
        .priority = 90
    },
    {
        .name = "MATCH+CREATE",
        .required = CLAUSE_MATCH | CLAUSE_CREATE,
        .forbidden = 0,
        .handler = execute_match_create_query,
        .priority = 80
    },
    // ... more patterns
    
    // Generic fallback (lowest priority)
    {
        .name = "GENERIC",
        .required = 0,
        .forbidden = 0,
        .handler = execute_generic_query,
        .priority = 0
    },
    
    {NULL, 0, 0, 0, NULL, 0}  // Sentinel
};
```

### Dispatch Logic

```c
int cypher_executor_execute_ast(cypher_executor *executor, 
                                 cypher_query *query,
                                 cypher_result *result) {
    // 1. Analyze query to get clause flags
    clause_flags present = analyze_query_clauses(query);
    
    // 2. Find matching pattern (sorted by priority)
    const query_pattern *pattern = find_matching_pattern(present);
    
    if (!pattern) {
        set_result_error(result, "Unsupported query pattern");
        return -1;
    }
    
    // 3. Log pattern match (debug)
    LOG_DEBUG("Executing query via pattern: %s", pattern->name);
    
    // 4. Dispatch to handler
    return pattern->handler(executor, query, result, present);
}

static clause_flags analyze_query_clauses(cypher_query *query) {
    clause_flags flags = 0;
    
    if (query->match_clause) flags |= CLAUSE_MATCH;
    if (query->return_clause) flags |= CLAUSE_RETURN;
    if (query->create_clause) flags |= CLAUSE_CREATE;
    // ... check all clauses
    
    // Check for OPTIONAL MATCH
    if (has_optional_match(query)) flags |= CLAUSE_OPTIONAL;
    
    return flags;
}

static const query_pattern *find_matching_pattern(clause_flags present) {
    const query_pattern *best = NULL;
    
    for (int i = 0; patterns[i].handler; i++) {
        const query_pattern *p = &patterns[i];
        
        // Check required clauses present
        if ((present & p->required) != p->required) continue;
        
        // Check forbidden clauses absent
        if (present & p->forbidden) continue;
        
        // Found match - check priority
        if (!best || p->priority > best->priority) {
            best = p;
        }
    }
    
    return best;
}
```

## Detailed Design

### Phase 1: Define Infrastructure

**New files:**
- `src/include/executor/query_patterns.h` - Pattern types and API
- `src/backend/executor/query_dispatch.c` - Dispatch implementation

```c
// query_patterns.h
#ifndef QUERY_PATTERNS_H
#define QUERY_PATTERNS_H

typedef enum { ... } clause_flags;
typedef int (*pattern_handler)(...);
typedef struct { ... } query_pattern;

clause_flags analyze_query_clauses(cypher_query *query);
const query_pattern *find_matching_pattern(clause_flags present);
const char *clause_flags_to_string(clause_flags flags);  // Debug helper

#endif
```

### Phase 2: Create Pattern Registry

Document all existing patterns from the if-else chain:

| Pattern Name | Required | Forbidden | Current Handler |
|--------------|----------|-----------|-----------------|
| UNWIND+CREATE | UNWIND, CREATE | RETURN, MATCH | inline (70 lines) |
| MATCH+RETURN | MATCH, RETURN | CREATE, SET, DELETE, MERGE | transform pipeline |
| MATCH+OPTIONAL+RETURN | MATCH, OPTIONAL, RETURN | CREATE, SET, DELETE | transform pipeline |
| MATCH+CREATE | MATCH, CREATE | - | execute_match_create_query |
| MATCH+SET | MATCH, SET | - | execute_match_set_query |
| MATCH+DELETE | MATCH, DELETE | - | execute_match_delete_query |
| MATCH+MERGE | MATCH, MERGE | - | execute_match_merge_query |
| CREATE only | CREATE | MATCH | direct insert |
| RETURN only | RETURN | MATCH | transform pipeline |
| WITH+RETURN | WITH, RETURN | - | transform pipeline |

### Phase 3: Extract Inline Handlers

Move inline code to named functions:

```c
// Before: 70 lines inline in main switch
if (unwind_clause && create_clause && !return_clause) {
    // ... inline logic
}

// After: named handler
static int execute_unwind_create(cypher_executor *executor,
                                  cypher_query *query,
                                  cypher_result *result,
                                  clause_flags present) {
    // Same logic, now isolated and testable
}
```

### Phase 4: Replace Main Dispatch

Convert `cypher_executor_execute_ast()`:

```c
// Before: 500+ lines of if-else
int cypher_executor_execute_ast(...) {
    if (unwind && create && !return) { ... }
    else if (match && return && !create) { ... }
    // ... 20 more patterns
}

// After: ~30 lines
int cypher_executor_execute_ast(...) {
    // Check for graph algorithms first (already well-structured)
    if (is_graph_algorithm_query(query)) {
        return execute_graph_algorithm(executor, query, result);
    }
    
    // Pattern-based dispatch for Cypher queries
    clause_flags present = analyze_query_clauses(query);
    const query_pattern *pattern = find_matching_pattern(present);
    
    if (!pattern) {
        set_result_error(result, "Unsupported query pattern");
        return -1;
    }
    
    return pattern->handler(executor, query, result, present);
}
```

## Testing Strategy

### Pattern Matching Tests

```c
void test_pattern_matching_unwind_create(void) {
    clause_flags flags = CLAUSE_UNWIND | CLAUSE_CREATE;
    const query_pattern *p = find_matching_pattern(flags);
    CU_ASSERT_PTR_NOT_NULL(p);
    CU_ASSERT_STRING_EQUAL(p->name, "UNWIND+CREATE");
}

void test_pattern_priority(void) {
    // MATCH+OPTIONAL+RETURN should match before MATCH+RETURN
    clause_flags flags = CLAUSE_MATCH | CLAUSE_OPTIONAL | CLAUSE_RETURN;
    const query_pattern *p = find_matching_pattern(flags);
    CU_ASSERT_STRING_EQUAL(p->name, "MATCH+OPTIONAL+RETURN");
}

void test_forbidden_clause_prevents_match(void) {
    // UNWIND+CREATE pattern forbids RETURN
    clause_flags flags = CLAUSE_UNWIND | CLAUSE_CREATE | CLAUSE_RETURN;
    const query_pattern *p = find_matching_pattern(flags);
    CU_ASSERT_STRING_NOT_EQUAL(p->name, "UNWIND+CREATE");
}
```

### Integration Tests

- All existing executor tests must pass
- Each pattern has dedicated test coverage
- Edge cases between patterns tested

## Alternatives Considered

1. **Keep if-else chain**: Status quo - rejected due to maintenance burden
2. **Visitor pattern on AST**: More complex, less explicit patterns
3. **State machine**: Overkill for current pattern count
4. **Hash map dispatch**: Patterns not simple key-value, need masks

## Implementation Plan

### Task 1: Infrastructure
- Create `query_patterns.h` with types
- Create `query_dispatch.c` with analyze and find functions
- Unit tests for infrastructure

### Task 2: Document Existing Patterns
- Enumerate all patterns from current if-else chain
- Create pattern registry array
- Verify no patterns missed

### Task 3: Extract Handlers
- Move inline code to named functions
- Each handler in appropriate executor file
- Test each handler individually

### Task 4: Replace Main Dispatch
- Convert `cypher_executor_execute_ast()`
- Remove old if-else chain
- Run full test suite

### Task 5: Documentation
- Document supported query patterns
- Add pattern debugging/logging
- Update contributor guide