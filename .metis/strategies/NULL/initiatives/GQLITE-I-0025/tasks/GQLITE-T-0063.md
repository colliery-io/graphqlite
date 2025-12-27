---
id: eliminate-select-replacement
level: task
title: "Eliminate SELECT * replacement pattern in WITH/UNWIND/RETURN"
short_code: "GQLITE-T-0063"
created_at: 2025-12-27T14:23:21.001652+00:00
updated_at: 2025-12-27T17:06:55.344263+00:00
parent: GQLITE-I-0025
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: GQLITE-I-0025
---

# Eliminate SELECT * replacement pattern in WITH/UNWIND/RETURN

## Parent Initiative

[[GQLITE-I-0025]] - Unified SQL Builder Architecture

## Objective

Eliminate the SELECT * replacement pattern that currently exists in transform_return.c and transform_with.c. This pattern involves string manipulation to find "SELECT *" in the sql_buffer and replace it with actual columns - a fragile approach that should be replaced with proper unified builder usage.

## Problem Statement

### Current Flow (problematic)
```
MATCH clause:
  → sql_from(), sql_join(), sql_where() on unified_builder
  → select_count remains 0 (no columns specified)

WITH/UNWIND clause:
  → finalize_sql_generation() called
  → sql_builder_to_string() outputs "SELECT * FROM ..."
  → sql_buffer now contains "SELECT *"

RETURN clause:
  → Finds "SELECT *" via strstr()
  → String manipulation to replace "*" with actual columns
```

### Root Cause
`finalize_sql_generation()` is called before WITH/UNWIND, which triggers `sql_builder_to_string()` when `select_count == 0`, outputting "SELECT *" as a placeholder.

### Why This Is Bad
1. String manipulation is fragile and error-prone
2. Two code paths for building SELECT columns
3. Complex logic in transform_return.c (lines 416-537)
4. Similar logic duplicated in transform_with.c (lines 124-289)

## Proposed Solution

### New Flow
```
MATCH clause:
  → sql_from(), sql_join(), sql_where() on unified_builder
  → unified_builder holds FROM/JOIN/WHERE state (no SELECT yet)

WITH clause:
  → Extract FROM/JOIN/WHERE from unified_builder (NOT as SQL string)
  → Build CTE body with explicit SELECT columns from WITH items
  → Add CTE via sql_cte()
  → Reset builder, set up for next clause

RETURN clause:
  → Add SELECT columns via sql_select()
  → finalize_sql_generation() called ONCE at the end
  → Proper SQL output with all columns
```

### Key Changes

#### 1. Add builder state extraction functions to sql_builder.c
```c
// Get FROM clause content without generating full SQL
const char *sql_builder_get_from(sql_builder *b);

// Get JOIN clauses content
const char *sql_builder_get_joins(sql_builder *b);

// Get WHERE clause content  
const char *sql_builder_get_where(sql_builder *b);
```

#### 2. Refactor transform_with.c
- Don't call finalize_sql_generation() before WITH
- Extract FROM/JOIN/WHERE from builder directly
- Build CTE with explicit SELECT columns from WITH items
- No more SELECT * string searching/replacing

#### 3. Refactor transform_unwind.c
- Similar pattern to WITH
- Extract builder state, build CTE explicitly

#### 4. Refactor cypher_transform.c
- Remove finalize_sql_generation() calls before WITH/UNWIND
- Only call finalize_sql_generation() once after RETURN

#### 5. Clean up transform_return.c
- Remove SELECT * replacement logic (lines 416-537)
- Unified builder path handles all cases

## Files to Modify

| File | Changes |
|------|---------|
| src/backend/transform/sql_builder.c | Add state extraction functions |
| src/include/transform/sql_builder.h | Declare new functions |
| src/backend/transform/transform_with.c | Refactor to use builder state directly |
| src/backend/transform/transform_unwind.c | Refactor to use builder state directly |
| src/backend/transform/cypher_transform.c | Remove early finalize_sql_generation() calls |
| src/backend/transform/transform_return.c | Remove SELECT * replacement logic |

## Implementation Steps

### Phase 1: Add Builder State Extraction
1. Add `sql_builder_get_from()`, `sql_builder_get_joins()`, `sql_builder_get_where()` to sql_builder.c
2. Add declarations to sql_builder.h
3. Unit tests for new functions

### Phase 2: Refactor WITH Clause
1. Remove finalize_sql_generation() call before WITH in cypher_transform.c
2. Update transform_with.c to extract builder state directly
3. Build CTE body with explicit columns
4. Test: MATCH ... WITH ... RETURN queries

### Phase 3: Refactor UNWIND Clause
1. Remove finalize_sql_generation() call before UNWIND in cypher_transform.c
2. Update transform_unwind.c similarly
3. Test: MATCH ... UNWIND ... RETURN queries

### Phase 4: Clean Up RETURN
1. Remove SELECT * replacement logic from transform_return.c
2. Ensure unified builder path handles all cases
3. Test: All query patterns

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] No strstr() calls looking for "SELECT *" in transform files
- [ ] finalize_sql_generation() only called once per query (after RETURN)
- [ ] All 716 C tests pass
- [ ] All 160 Python tests pass
- [ ] SELECT columns always specified explicitly, never using "*" placeholder

## Risk Considerations

- **High complexity**: WITH clause has complex logic for aggregates, GROUP BY
- **Edge cases**: UNION queries, nested WITH, aggregation in WITH
- **Mitigation**: Incremental approach, extensive testing after each phase

## Status Updates

*To be added during implementation*