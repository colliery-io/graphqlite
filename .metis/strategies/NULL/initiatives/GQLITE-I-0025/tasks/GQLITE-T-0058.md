---
id: migrate-with-clause-ctes-to
level: task
title: "Migrate WITH clause CTEs to unified builder"
short_code: "GQLITE-T-0058"
created_at: 2025-12-27T04:37:24.578487+00:00
updated_at: 2025-12-27T13:22:18.785051+00:00
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

# Migrate WITH clause CTEs to unified builder

## Phase 2: Migrate CTEs - WITH Clause

**Depends on**: GQLITE-T-0057 (Migrate UNWIND CTEs)

## Overview

Migrate `transform_with.c` to use `sql_cte()` for WITH clause subqueries.

## Current State (after rollback)

- Uses `append_cte_prefix()` for CTE
- Captures current sql_buffer as CTE body
- Works but uses legacy buffer

## Target State

- Finalize current unified_builder as CTE body
- Call `sql_cte(ctx->unified_builder, name, body, false)`
- Reset builder for next clause chain
- Single unified code path

## Key Functions

1. `transform_with_clause()` - WITH clause transformation

## File

`src/backend/transform/transform_with.c`

## WITH Transformation

```cypher
MATCH (n) WITH n.name AS name RETURN name
```
→
```sql
WITH _with_0 AS (
  SELECT n.name AS name FROM nodes n
)
SELECT _with_0.name AS name FROM _with_0
```

## Steps

1. Finalize current builder state via `sql_builder_to_string()`
2. Call `sql_cte(ctx->unified_builder, cte_name, finalized_sql, false)`
3. Reset builder for subsequent clauses
4. Register projected variables for the new scope

## Complexity

WITH clause is tricky because:
- It captures everything before it as a CTE
- It creates a new variable scope
- Multiple WITH clauses chain together

## Success Criteria

- WITH clause tests pass
- Chained WITH clauses work
- Variable scoping correct
- No `append_cte_prefix()` calls

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0025]]

## Objective **[REQUIRED]**

{Clear statement of what this task accomplishes}

## Backlog Item Details **[CONDITIONAL: Backlog Item]**

{Delete this section when task is assigned to an initiative}

### Type
- [ ] Bug - Production issue that needs fixing
- [ ] Feature - New functionality or enhancement  
- [ ] Tech Debt - Code improvement or refactoring
- [ ] Chore - Maintenance or setup work

### Priority
- [ ] P0 - Critical (blocks users/revenue)
- [ ] P1 - High (important for user experience)
- [ ] P2 - Medium (nice to have)
- [ ] P3 - Low (when time permits)

### Impact Assessment **[CONDITIONAL: Bug]**
- **Affected Users**: {Number/percentage of users affected}
- **Reproduction Steps**: 
  1. {Step 1}
  2. {Step 2}
  3. {Step 3}
- **Expected vs Actual**: {What should happen vs what happens}

### Business Justification **[CONDITIONAL: Feature]**
- **User Value**: {Why users need this}
- **Business Value**: {Impact on metrics/revenue}
- **Effort Estimate**: {Rough size - S/M/L/XL}

### Technical Debt Impact **[CONDITIONAL: Tech Debt]**
- **Current Problems**: {What's difficult/slow/buggy now}
- **Benefits of Fixing**: {What improves after refactoring}
- **Risk Assessment**: {Risks of not addressing this}

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] {Specific, testable requirement 1}
- [ ] {Specific, testable requirement 2}
- [ ] {Specific, testable requirement 3}

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