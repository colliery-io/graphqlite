---
id: migrate-transform-return-c-to
level: task
title: "Migrate transform_return.c to unified builder"
short_code: "GQLITE-T-0055"
created_at: 2025-12-27T04:37:23.915209+00:00
updated_at: 2025-12-27T12:57:58.147094+00:00
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

# Migrate transform_return.c to unified builder

## Phase 1: Migrate READ Queries - RETURN Clause

**Depends on**: GQLITE-T-0054 (Migrate MATCH)

## Overview

Migrate `transform_return.c` to use unified builder for SELECT, ORDER BY, LIMIT/OFFSET.

## Current State

- Uses `append_sql()` for SELECT expressions
- Uses `append_sql()` for ORDER BY
- Uses `append_sql()` for LIMIT/OFFSET
- Assembles via string concatenation

## Target State

- Use `sql_select()` for each return item
- Use `sql_order_by()` for ORDER BY expressions
- Use `sql_set_limit()` for LIMIT/OFFSET
- Assembly handled by `sql_builder_to_string()`

## Key Functions to Migrate

1. `transform_return_clause()` - Main entry point
2. `transform_return_items()` - Column expressions
3. ORDER BY handling
4. LIMIT/OFFSET handling
5. DISTINCT handling

## File

`src/backend/transform/transform_return.c`

## Steps

1. Replace `append_sql(ctx, "SELECT ...")` with `sql_select(ctx->unified_builder, expr, alias)`
2. Replace ORDER BY with `sql_order_by(ctx->unified_builder, expr, desc)`
3. Replace LIMIT with `sql_set_limit(ctx->unified_builder, limit, offset)`
4. Handle DISTINCT via builder flag or prefix
5. Update `finalize_sql_generation()` to assemble from unified_builder

## Success Criteria

- All RETURN tests pass
- ORDER BY, LIMIT, SKIP work correctly
- DISTINCT works correctly
- No `append_sql()` calls for SELECT/ORDER/LIMIT in this file

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