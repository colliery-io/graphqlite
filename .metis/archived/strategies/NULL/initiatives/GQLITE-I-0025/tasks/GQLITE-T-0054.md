---
id: migrate-transform-match-c-to
level: task
title: "Migrate transform_match.c to unified builder"
short_code: "GQLITE-T-0054"
created_at: 2025-12-27T04:37:23.731482+00:00
updated_at: 2025-12-27T04:46:57.397378+00:00
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

# Migrate transform_match.c to unified builder

## Phase 1: Migrate READ Queries - MATCH Clause

**Depends on**: GQLITE-T-0053 (Rollback)

## Overview

Migrate `transform_match.c` to use unified builder for ALL SQL generation, not just CTEs.

## Current State

- Uses `append_sql()` for SELECT, FROM, WHERE
- Uses `sql_builder` struct for OPTIONAL MATCH (deferred WHERE)
- Mode flag `using_builder` switches between systems

## Target State

- Use `sql_from()` for FROM clauses
- Use `sql_join()` for JOIN clauses (including LEFT JOIN for OPTIONAL MATCH)
- Use `sql_where()` for WHERE conditions
- No mode flags, single code path

## Key Functions to Migrate

1. `transform_match_clause()` - Main entry point
2. `transform_match_pattern()` - Pattern processing
3. `build_node_match_sql()` - Node table FROM/JOIN
4. `build_relationship_match_sql()` - Edge table JOIN
5. OPTIONAL MATCH handling (LEFT JOIN generation)

## File

`src/backend/transform/transform_match.c`

## Steps

1. Replace `append_sql(ctx, "FROM nodes ...")` with `sql_from(ctx->unified_builder, "nodes", alias)`
2. Replace join generation with `sql_join(ctx->unified_builder, JOIN_INNER, table, alias, condition)`
3. Replace OPTIONAL MATCH LEFT JOIN with `sql_join(ctx->unified_builder, JOIN_LEFT, ...)`
4. Replace WHERE with `sql_where()` / `sql_where_and()`
5. Remove `using_builder` flag checks
6. Test OPTIONAL MATCH with WHERE thoroughly

## Success Criteria

- All MATCH tests pass
- OPTIONAL MATCH with WHERE works correctly
- No `append_sql()` calls for FROM/JOIN/WHERE in this file

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