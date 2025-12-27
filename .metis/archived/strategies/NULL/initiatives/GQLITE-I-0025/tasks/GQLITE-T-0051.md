---
id: migrate-remaining-clauses-with
level: task
title: "Migrate remaining clauses (WITH, CREATE, DELETE, SET, MERGE)"
short_code: "GQLITE-T-0051"
created_at: 2025-12-26T20:34:30.551046+00:00
updated_at: 2025-12-27T14:17:52.489175+00:00
parent: GQLITE-I-0025
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: GQLITE-I-0025
---

# Migrate remaining clauses (WITH, CREATE, DELETE, SET, MERGE)

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0025]]

## Objective

Migrate remaining transform files to unified sql_builder where beneficial.

## Status: PARTIALLY COMPLETE

### What Was Completed (via other tasks)
- ✅ transform_with.c - CTEs migrated to sql_cte(), saves/restores across reset
- ✅ transform_unwind.c - CTEs migrated to sql_cte(), uses sql_select(), sql_from()
- ✅ transform_foreach.c - CTEs migrated to sql_cte()
- ✅ transform_return.c - Unified builder path for MATCH+RETURN and standalone RETURN

### What Stays As-Is (by design)
- transform_create.c - Uses append_sql() for INSERT (different SQL structure)
- transform_set.c - Uses append_sql() for UPDATE (different SQL structure)  
- transform_delete.c - Uses append_sql() for DELETE (different SQL structure)
- transform_merge.c - Uses append_sql() for INSERT/UPDATE (different SQL structure)

### Remaining Legacy Paths in transform_return.c
- SELECT * replacement logic (string manipulation)
- RETURN after WITH (modifies existing SQL)
- Expression building (json_object, COLLECT, paths) - encapsulated, acceptable

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] transform_with.c migrated where beneficial
- [x] transform_unwind.c migrated where beneficial
- [x] WRITE clauses evaluated - keeping append_sql() by design
- [x] All tests pass (716 C, 160 Python)

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