---
id: unified-sql-builder-architecture
level: initiative
title: "Unified SQL Builder Architecture"
short_code: "GQLITE-I-0025"
created_at: 2025-12-26T04:50:28.707879+00:00
updated_at: 2025-12-26T20:34:11.095249+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/decompose"


exit_criteria_met: false
estimated_complexity: L
strategy_id: NULL
initiative_id: unified-sql-builder-architecture
---

# Unified SQL Builder Architecture Initiative

## Current Status: BROKEN - Needs Rollback

The partial migration created **duplicate CTEs** because we mixed paths:
- CTEs added to `unified_builder->cte` via `sql_cte()`
- SELECT/FROM/WHERE still written to `sql_buffer` via `append_sql()`
- `prepend_cte_to_sql()` tries to merge both, causing duplication

**Test failure**: `WITH _unwind_0 AS (...) WITH _unwind_0 AS (...) SELECT ...`

**Root cause**: We migrated CTE generation to `sql_cte()` but left everything else using `append_sql()`. This creates two incompatible paths.

**Key insight**: For each clause type, migrate COMPLETELY or not at all. If using `unified_builder`, use it for EVERYTHING: CTE, SELECT, FROM, WHERE.

## Revised Approach

1. **Rollback** partial CTE migrations to get tests passing
2. **Migrate READ queries completely** (MATCH + RETURN through unified_builder)
3. **Migrate CTEs** once the base works
4. **Migrate WRITE queries** last
5. **Remove legacy** cte_prefix and append_cte_prefix()

## Context

The transform layer currently uses **three incompatible SQL generation mechanisms** that don't compose cleanly:

### Mechanism 1: Direct Buffer (`sql_buffer`)

```c
void append_sql(cypher_transform_context *ctx, const char *format, ...);
```

- Primary mechanism, 278+ calls across codebase
- Writes directly to `ctx->sql_buffer`
- Simple but doesn't support deferred assembly

**Used by**: Most transform files, RETURN clause, simple MATCH

### Mechanism 2: SQL Builder Struct

```c
struct {
    char *from_clause;
    char *join_clauses;
    char *where_clauses;
    bool using_builder;
    // ... sizes
} sql_builder;
```

- Added for OPTIONAL MATCH (requires deferred WHERE placement)
- Separate buffers for each SQL clause
- Mode flag `using_builder` controls which system is active
- Finalized via `finalize_sql_generation()` which reassembles

**Used by**: OPTIONAL MATCH, some complex JOIN patterns

### Mechanism 3: CTE Prefix Buffer

```c
char *cte_prefix;
size_t cte_prefix_size;
```

- Separate buffer for Common Table Expressions
- Prepended to final SQL after everything else
- Not integrated with sql_builder

**Used by**: Variable-length relationships, recursive patterns

### The Problem

These systems don't integrate:

1. **Mode flags everywhere**: `if (ctx->sql_builder.using_builder)` scattered throughout
2. **Buffer manipulation**: OPTIONAL MATCH with WHERE required saving/restoring sql_buffer state
3. **Assembly order matters**: CTE must come first, but generated last
4. **Fragile combinations**: OPTIONAL MATCH + variable-length paths requires all 3 systems

Recent bugs (GQLITE-T-0043 OPTIONAL MATCH with WHERE) stemmed directly from this complexity.

## Goals & Non-Goals

**Goals:**
- Single unified SQL builder that handles all query patterns
- Eliminate mode flags and conditional buffer switching
- Clean separation of SQL clause construction from assembly
- Support all existing patterns: OPTIONAL MATCH, variable-length, CTEs
- Simpler mental model for contributors

**Non-Goals:**
- SQL optimization (separate concern)
- Query plan generation
- Supporting non-SQLite backends
- Breaking existing tests

## Architecture

### Unified Builder Design

```c
typedef struct {
    // Dynamic string buffers for each clause
    dynamic_buffer cte;           // WITH RECURSIVE ...
    dynamic_buffer select;        // SELECT ...
    dynamic_buffer from;          // FROM ...
    dynamic_buffer joins;         // JOIN ... LEFT JOIN ...
    dynamic_buffer where;         // WHERE ...
    dynamic_buffer group_by;      // GROUP BY ...
    dynamic_buffer having;        // HAVING ...
    dynamic_buffer order_by;      // ORDER BY ...
    dynamic_buffer limit_offset;  // LIMIT ... OFFSET ...
    
    // Join tracking for correct assembly
    int join_count;
    bool has_left_join;
    
    // State
    bool finalized;
} sql_builder_v2;
```

### Builder API

```c
// Lifecycle
sql_builder_v2 *sql_builder_create(void);
void sql_builder_free(sql_builder_v2 *b);

// Clause construction (order-independent)
void sql_add_cte(sql_builder_v2 *b, const char *name, const char *query);
void sql_add_select(sql_builder_v2 *b, const char *expr, const char *alias);
void sql_add_from(sql_builder_v2 *b, const char *table, const char *alias);
void sql_add_join(sql_builder_v2 *b, join_type type, const char *table, 
                  const char *alias, const char *condition);
void sql_add_where(sql_builder_v2 *b, const char *condition);
void sql_add_where_and(sql_builder_v2 *b, const char *condition);
void sql_add_group_by(sql_builder_v2 *b, const char *expr);
void sql_add_having(sql_builder_v2 *b, const char *condition);
void sql_add_order_by(sql_builder_v2 *b, const char *expr, bool desc);
void sql_set_limit(sql_builder_v2 *b, int limit, int offset);

// Assembly (called once at end)
char *sql_builder_finalize(sql_builder_v2 *b);
```

### Usage Example

```c
// Before (3 systems, mode flags)
if (is_optional_match) {
    ctx->sql_builder.using_builder = true;
    append_from_clause(ctx, "nodes n");
    append_join_clause(ctx, "LEFT JOIN edges e ON ...");
    // Later, manually save/restore sql_buffer for WHERE...
}

// After (single system)
sql_add_from(builder, "nodes", "n");
sql_add_join(builder, JOIN_LEFT, "edges", "e", "n.id = e.source_id");
sql_add_where(builder, "e.label = 'KNOWS'");
// Assembly handles correct placement
```

## Detailed Design

### Phase 1: Create New Builder Module

**New files:**
- `src/backend/transform/sql_builder.h`
- `src/backend/transform/sql_builder.c`

Implement:
1. `dynamic_buffer` helper (growing string buffer)
2. All `sql_add_*` functions
3. `sql_builder_finalize()` that assembles in correct order
4. Unit tests for builder in isolation

### Phase 2: Integrate with Transform Context

```c
// In cypher_transform_context
sql_builder_v2 *builder;  // New field

// Create in cypher_transform_create_context()
// Free in cypher_transform_free_context()
```

### Phase 3: Migrate MATCH Clause

Start with `transform_match.c`:
1. Replace `append_sql()` calls with `sql_add_*()` calls
2. Replace `sql_builder.from_clause` usage with `sql_add_from()`
3. Replace join generation with `sql_add_join()`
4. Test OPTIONAL MATCH thoroughly

### Phase 4: Migrate RETURN Clause

Update `transform_return.c`:
1. Use `sql_add_select()` for column expressions
2. Use `sql_add_order_by()` for ORDER BY
3. Use `sql_set_limit()` for LIMIT/OFFSET

### Phase 5: Migrate Remaining Clauses

- `transform_with.c` - subquery construction
- `transform_create.c` - INSERT statements (may need different builder)
- Variable-length relationship CTE generation

### Phase 6: Remove Legacy Systems

1. Remove `sql_buffer` direct usage (or repurpose for expressions only)
2. Remove `sql_builder` struct
3. Remove `cte_prefix` handling
4. Remove `finalize_sql_generation()` in favor of `sql_builder_finalize()`
5. Remove all `using_builder` checks

## Testing Strategy

### Unit Tests for Builder

```c
void test_builder_simple_select(void) {
    sql_builder_v2 *b = sql_builder_create();
    sql_add_select(b, "n.name", "name");
    sql_add_from(b, "nodes", "n");
    char *sql = sql_builder_finalize(b);
    CU_ASSERT_STRING_EQUAL(sql, "SELECT n.name AS name FROM nodes n");
    free(sql);
    sql_builder_free(b);
}

void test_builder_left_join_where(void) {
    sql_builder_v2 *b = sql_builder_create();
    sql_add_from(b, "nodes", "n");
    sql_add_join(b, JOIN_LEFT, "edges", "e", "n.id = e.source_id");
    sql_add_where(b, "e.label = 'KNOWS'");
    char *sql = sql_builder_finalize(b);
    // WHERE correctly placed after LEFT JOIN
    CU_ASSERT_STRING_CONTAINS(sql, "LEFT JOIN edges e");
    CU_ASSERT_STRING_CONTAINS(sql, "WHERE e.label");
    sql_builder_free(b);
}

void test_builder_cte(void) {
    sql_builder_v2 *b = sql_builder_create();
    sql_add_cte(b, "path_cte", "SELECT ...");
    sql_add_from(b, "path_cte", "p");
    char *sql = sql_builder_finalize(b);
    CU_ASSERT_TRUE(strncmp(sql, "WITH path_cte AS", 16) == 0);
    sql_builder_free(b);
}
```

### Integration Testing

- All existing tests must pass
- Focus on complex patterns:
  - OPTIONAL MATCH with WHERE
  - Multiple MATCH clauses
  - Variable-length relationships
  - WITH clause chaining
  - UNION queries

## Alternatives Considered

1. **Keep patching existing systems**: Rejected - complexity keeps growing
2. **Use existing SQL builder library**: None suitable for embedded C
3. **String templates**: Less flexible, still needs assembly logic
4. **Query IR before SQL**: Overkill, adds another layer

## Implementation Plan

### Task 1: Dynamic Buffer Utility
- Create `dynamic_buffer` struct and helpers
- Unit tests for buffer operations

### Task 2: SQL Builder Core
- Implement `sql_builder_v2` and all `sql_add_*` functions
- Implement `sql_builder_finalize()`
- Comprehensive unit tests

### Task 3: Integration Scaffolding
- Add builder to transform context
- Create wrapper macros for gradual migration
- No behavior changes yet

### Task 4: Migrate MATCH
- Convert transform_match.c to use new builder
- Special attention to OPTIONAL MATCH
- Run full test suite

### Task 5: Migrate RETURN
- Convert transform_return.c
- Run tests

### Task 6: Migrate Remaining
- WITH, CREATE, DELETE, SET, MERGE clauses
- Variable-length paths
- Run tests after each

### Task 7: Remove Legacy
- Delete old sql_buffer usage
- Delete sql_builder struct
- Delete cte_prefix handling
- Final cleanup and test