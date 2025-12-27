---
id: remove-legacy-cte-prefix-and
level: task
title: "Remove legacy cte_prefix and append_cte_prefix"
short_code: "GQLITE-T-0060"
created_at: 2025-12-27T04:37:37.098667+00:00
updated_at: 2025-12-27T14:04:27.588988+00:00
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

# Remove legacy cte_prefix and append_cte_prefix

## Phase 4: Cleanup - Remove Legacy Buffers

**Depends on**: All Phase 2 and Phase 3 tasks complete

## Overview

Remove the legacy `cte_prefix` buffer and `append_cte_prefix()` function once all CTEs use `sql_cte()`.

## What to Remove

### In `cypher_transform.h`

```c
// Remove from cypher_transform_context:
char *cte_prefix;
size_t cte_prefix_size;
size_t cte_prefix_capacity;
```

### In `cypher_transform.c`

```c
// Remove function:
void append_cte_prefix(cypher_transform_context *ctx, const char *format, ...);

// Remove from context creation:
ctx->cte_prefix = NULL;
ctx->cte_prefix_size = 0;
ctx->cte_prefix_capacity = 0;

// Remove from context cleanup:
free(ctx->cte_prefix);

// Simplify prepend_cte_to_sql():
// Should only use unified_builder->cte now
```

## Files to Modify

1. `src/backend/transform/cypher_transform.h` - Remove struct fields
2. `src/backend/transform/cypher_transform.c` - Remove function and usages
3. Any remaining callers of `append_cte_prefix()` (should be none after Phase 2)

## Verification

1. `grep -r "cte_prefix" src/` should return no results
2. `grep -r "append_cte_prefix" src/` should return no results
3. All tests pass

## Success Criteria

- `cte_prefix` completely removed from codebase
- `append_cte_prefix()` completely removed
- All 716 C unit tests pass
- All 160 Python tests pass
- Single code path for CTE generation

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