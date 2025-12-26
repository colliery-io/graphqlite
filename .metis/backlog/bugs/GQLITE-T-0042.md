---
id: fix-with-clause-where-and
level: task
title: "Fix WITH clause WHERE and aggregation issues"
short_code: "GQLITE-T-0042"
created_at: 2025-12-26T02:47:07.078313+00:00
updated_at: 2025-12-26T02:55:00.051195+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Fix WITH clause WHERE and aggregation issues

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Fix two issues with the WITH clause transformation:
1. WHERE clause after WITH doesn't filter correctly due to SQLite TEXT vs INTEGER affinity
2. Aggregation functions in WITH (sum, count, etc.) don't group correctly

## Root Causes

### Issue 1: WHERE clause comparison fails

Property values are all cast to TEXT in the COALESCE expression used for property access. When comparing `age > 27` where `age` is TEXT ('30' or '25'), SQLite's type affinity rules cause unexpected behavior:

```sql
SELECT '25' > 27;  -- Returns 1 (true!) because TEXT has higher affinity than INTEGER
SELECT '30' > 27;  -- Returns 1 (true)
```

**Location**: `src/backend/transform/transform_with.c` lines 99-111 (COALESCE casts to TEXT)

### Issue 2: Aggregation doesn't group

The GROUP BY clause is being built with node IDs instead of the actual column values:
```c
group_by_len += snprintf(group_by_buffer + group_by_len,
                        sizeof(group_by_buffer) - group_by_len, "%s.id", alias);
```

This groups by each individual node, defeating the purpose of aggregation.

**Location**: `src/backend/transform/transform_with.c` lines 82-88 and 112-118

## Reproduction Steps

```cypher
-- Issue 1: WHERE doesn't filter
CREATE (n:Test {name: 'Alice', age: 30})
CREATE (n:Test {name: 'Bob', age: 25})
MATCH (n:Test) WITH n.name AS name, n.age AS age WHERE age > 27 RETURN name
-- Expected: Alice only
-- Actual: Both Alice and Bob

-- Issue 2: Aggregation doesn't group
CREATE (n:Test {category: 'A', value: 10})
CREATE (n:Test {category: 'A', value: 20})
CREATE (n:Test {category: 'B', value: 30})
MATCH (n:Test) WITH n.category AS cat, sum(n.value) AS total RETURN cat, total
-- Expected: A:30, B:30 (2 rows)
-- Actual: A:10, A:20, B:30 (3 rows)
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `WHERE age > 27` correctly filters when age is an integer property
- [ ] `WITH n.category AS cat, sum(n.value) AS total` groups by category correctly
- [ ] Python binding tests `test_with_where` and `test_with_aggregation` pass
- [ ] No regressions in existing WITH clause tests

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