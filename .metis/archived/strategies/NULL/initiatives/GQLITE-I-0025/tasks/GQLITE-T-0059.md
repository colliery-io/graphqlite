---
id: migrate-write-clauses-to-unified
level: task
title: "Migrate WRITE clauses to unified builder"
short_code: "GQLITE-T-0059"
created_at: 2025-12-27T04:37:36.876591+00:00
updated_at: 2025-12-27T13:59:52.649648+00:00
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

# Migrate WRITE clauses to unified builder

## Phase 3: Migrate WRITE Queries

**Depends on**: Phase 2 tasks (all CTE migrations complete)

## Overview

Migrate CREATE, DELETE, SET, MERGE clauses to use unified builder where applicable.

## Complexity

WRITE clauses are different from READ because:
- They generate INSERT/UPDATE/DELETE statements, not SELECT
- They may generate multiple statements
- MERGE generates conditional INSERT OR UPDATE

## Files

- `src/backend/transform/transform_create.c`
- `src/backend/transform/transform_delete.c`
- `src/backend/transform/transform_set.c`
- `src/backend/transform/transform_merge.c`

## Approach Options

### Option A: Separate Builder for WRITE

Create a write-specific builder that handles INSERT/UPDATE/DELETE:
- `sql_insert()`, `sql_update()`, `sql_delete()`
- Different assembly logic

### Option B: Extend Unified Builder

Add INSERT/UPDATE/DELETE support to existing unified builder:
- `sql_set_statement_type(builder, STMT_INSERT)`
- Conditional assembly based on statement type

### Option C: Keep WRITE Using append_sql()

WRITE clauses are simpler and less prone to the multi-buffer issues.
Could keep using `append_sql()` for these.

## Recommendation

Start with Option C (keep as-is) and revisit if needed. The main pain points were in READ queries with OPTIONAL MATCH and CTEs.

## Success Criteria

- All CREATE/DELETE/SET/MERGE tests pass
- No regressions from Phase 1/2 changes

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