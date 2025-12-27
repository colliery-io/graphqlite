---
id: migrate-unwind-clause-ctes-to
level: task
title: "Migrate UNWIND clause CTEs to unified builder"
short_code: "GQLITE-T-0057"
created_at: 2025-12-27T04:37:24.346760+00:00
updated_at: 2025-12-27T13:19:13.549590+00:00
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

# Migrate UNWIND clause CTEs to unified builder

## Phase 2: Migrate CTEs - UNWIND Clause

**Depends on**: GQLITE-T-0056 (Migrate varlen CTEs)

## Overview

Migrate `transform_unwind.c` to use `sql_cte()` for UNWIND expansion.

## Current State (after rollback)

- Uses `append_cte_prefix()` for CTE
- Uses `append_sql()` for SELECT/FROM
- Works but uses legacy buffer

## Target State

- Build CTE query in local `dynamic_buffer`
- Call `sql_cte(ctx->unified_builder, name, query, false)`
- Use `sql_select()` and `sql_from()` for the outer query
- Single unified code path

## Key Functions

1. `transform_unwind_clause()` - Main transformation

## File

`src/backend/transform/transform_unwind.c`

## UNWIND Transformation

```cypher
UNWIND [1, 2, 3] AS x RETURN x
```
→
```sql
WITH _unwind_0 AS (
  SELECT 1 AS value UNION ALL SELECT 2 UNION ALL SELECT 3
)
SELECT _unwind_0.value AS x FROM _unwind_0
```

## Steps

1. Build CTE query (UNION ALL or json_each) in local buffer
2. Call `sql_cte(ctx->unified_builder, cte_name, query, false)`
3. Call `sql_select(ctx->unified_builder, expr, alias)`
4. Call `sql_from(ctx->unified_builder, cte_name, NULL)`
5. Free local buffer

## Success Criteria

- UNWIND tests pass
- No duplicate CTEs
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