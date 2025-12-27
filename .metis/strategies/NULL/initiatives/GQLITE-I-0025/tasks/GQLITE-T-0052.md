---
id: remove-legacy-sql-buffer-sql
level: task
title: "Remove legacy sql_buffer, sql_builder struct, and cte_prefix"
short_code: "GQLITE-T-0052"
created_at: 2025-12-26T20:34:31.848840+00:00
updated_at: 2025-12-27T14:17:52.731858+00:00
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

# Remove legacy sql_buffer, sql_builder struct, and cte_prefix

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0025]]

## Objective

Remove legacy SQL generation mechanisms that are no longer needed.

## Status: PARTIALLY COMPLETE

### What Was Removed
- ✅ `cte_prefix`, `cte_prefix_size`, `cte_prefix_capacity` - removed from context
- ✅ `append_cte_prefix()` - removed, all CTEs use sql_cte()
- ✅ Old `sql_builder` struct (from_clause, join_clauses, using_builder) - was already removed
- ✅ `grow_builder_buffer()` helper - removed (was only used by append_cte_prefix)

### What Remains (by design)
- `sql_buffer` - Still needed as final output buffer passed to sqlite3_prepare_v2()
- `append_sql()` - Still needed for expression building and WRITE operations
- `finalize_sql_generation()` - Assembles unified_builder into sql_buffer
- `prepend_cte_to_sql()` - Prepends CTEs from unified_builder to final SQL

### Architecture After Cleanup
```
unified_builder (sql_cte, sql_select, sql_from, sql_join, sql_where, sql_order_by, sql_limit)
        ↓
finalize_sql_generation() → sql_buffer
        ↓
prepend_cte_to_sql() → sql_buffer (with CTEs prepended)
        ↓
sqlite3_prepare_v2(sql_buffer)
```

### Why sql_buffer Can't Be Removed
1. It's the final output passed to SQLite
2. Expression building uses it as scratch space (via transform_expression_to_string buffer swap)
3. WRITE operations (CREATE/SET/DELETE) build SQL directly into it

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] cte_prefix and append_cte_prefix() removed
- [x] Old sql_builder struct already gone
- [x] Architecture simplified from 3 paths to 2
- [x] All tests pass (716 C, 160 Python)
- [x] Clean compile

## Test Cases **[CONDITIONAL: Testing Task]**

{Delete unless this is a testing task}

### Test Case 1: {Test Case Name}
- **Test ID**: TC-001
- **Preconditions**: {What must be true before testing}
- **Steps**: 
  1. {Step 1}
  2. {Step 2}
  3. {Step 3}
- **Expected Results**: {What should happen}
- **Actual Results**: {To be filled during execution}
- **Status**: {Pass/Fail/Blocked}

### Test Case 2: {Test Case Name}
- **Test ID**: TC-002
- **Preconditions**: {What must be true before testing}
- **Steps**: 
  1. {Step 1}
  2. {Step 2}
- **Expected Results**: {What should happen}
- **Actual Results**: {To be filled during execution}
- **Status**: {Pass/Fail/Blocked}

## Documentation Sections **[CONDITIONAL: Documentation Task]**

{Delete unless this is a documentation task}

### User Guide Content
- **Feature Description**: {What this feature does and why it's useful}
- **Prerequisites**: {What users need before using this feature}
- **Step-by-Step Instructions**:
  1. {Step 1 with screenshots/examples}
  2. {Step 2 with screenshots/examples}
  3. {Step 3 with screenshots/examples}

### Troubleshooting Guide
- **Common Issue 1**: {Problem description and solution}
- **Common Issue 2**: {Problem description and solution}
- **Error Messages**: {List of error messages and what they mean}

### API Documentation **[CONDITIONAL: API Documentation]**
- **Endpoint**: {API endpoint description}
- **Parameters**: {Required and optional parameters}
- **Example Request**: {Code example}
- **Example Response**: {Expected response format}

## Implementation Notes **[CONDITIONAL: Technical Task]**

{Keep for technical tasks, delete for non-technical. Technical details, approach, or important considerations}

### Technical Approach
{How this will be implemented}

### Dependencies
{Other tasks or systems this depends on}

### Risk Considerations
{Technical risks and mitigation strategies}

## Status Updates **[REQUIRED]**

*To be added during implementation*