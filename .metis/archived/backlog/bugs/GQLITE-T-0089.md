---
id: keys-function-sql-generation-uses
level: task
title: "keys() function SQL generation uses broken EXISTS with UNION ALL"
short_code: "GQLITE-T-0089"
created_at: 2026-01-05T14:30:55.659606+00:00
updated_at: 2026-01-05T15:15:21.710597+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# keys() function SQL generation uses broken EXISTS with UNION ALL

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Fix the `keys()` function to return the correct list of property keys for a node.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [ ] P2 - Medium (nice to have)

### Impact Assessment
- **Affected Users**: Any user calling keys(n) on a node
- **Reproduction Steps**: 
  1. `CREATE (n:Person {name: "Alice", age: 30})`
  2. `MATCH (n:Person) RETURN keys(n)`
  3. Returns `[]` instead of `["name", "age"]`
- **Expected vs Actual**: 
  - Expected: `["name", "age"]`
  - Actual: `[]` (empty array)

### Root Cause

The generated SQL uses `EXISTS (SELECT 1 ... UNION ALL SELECT 1 ...)` pattern which doesn't work correctly in SQLite:

```sql
-- BROKEN (current implementation):
EXISTS (SELECT 1 FROM node_props_text WHERE node_id = n.id AND key_id = pk.id 
        UNION ALL 
        SELECT 1 FROM node_props_int WHERE node_id = n.id AND key_id = pk.id)

-- WORKING (should be):
EXISTS (SELECT 1 FROM node_props_text WHERE node_id = n.id AND key_id = pk.id)
OR EXISTS (SELECT 1 FROM node_props_int WHERE node_id = n.id AND key_id = pk.id)
```

### File Location
- `src/backend/transform/transform_func_entity.c` - `transform_keys_function()`

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n:Person) RETURN keys(n)` returns correct property keys
- [ ] `MATCH (n) UNWIND keys(n) AS key RETURN key` works correctly
- [ ] Regression test added for keys() function

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

### 2026-01-05: Completed
- Fixed SQL generation in `transform_func_entity.c` for both `keys()` and `properties()` functions
- Changed `EXISTS (... UNION ALL ...)` pattern to use separate `EXISTS ... OR EXISTS ...` checks
- SQLite doesn't handle EXISTS with UNION ALL correctly
- Added regression test `test_keys_function_regression`
- All 761 C unit tests pass
- Committed as 977d5e1