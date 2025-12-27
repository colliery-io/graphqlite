---
id: replace-if-else-chain-with-table
level: task
title: "Replace if-else chain with table-driven dispatch"
short_code: "GQLITE-T-0070"
created_at: 2025-12-27T17:40:38.404857+00:00
updated_at: 2025-12-27T18:47:43.458020+00:00
parent: GQLITE-I-0026
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: GQLITE-I-0026
---

# Replace if-else chain with table-driven dispatch

## Objective

Replace the 500+ line if-else chain in `cypher_executor_execute_ast()` with table-driven dispatch.

## Before (500+ lines)

```c
int cypher_executor_execute_ast(...) {
    if (unwind && create && !return) { ... }
    else if (match && return && !create) { ... }
    else if (match && optional && return) { ... }
    // ... 20+ more patterns
}
```

## After (~30 lines)

```c
int cypher_executor_execute_ast(...) {
    // Graph algorithms handled separately (already well-structured)
    if (is_graph_algorithm_query(query)) {
        return execute_graph_algorithm(executor, query, result);
    }
    
    // Pattern-based dispatch
    clause_flags present = analyze_query_clauses(query);
    const query_pattern *pattern = find_matching_pattern(present);
    
    if (!pattern) {
        set_result_error(result, "Unsupported query pattern");
        return -1;
    }
    
    LOG_DEBUG("Executing via pattern: %s", pattern->name);
    return pattern->handler(executor, query, result, present);
}
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] If-else chain completely removed
- [x] All dispatch goes through pattern registry
- [x] Graph algorithm dispatch preserved (don't change)
- [x] All 748/749 C tests pass (1 pre-existing failure)
- [x] All 160 Python tests pass
- [x] No performance regression

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0026]]

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

### 2025-12-27: Completed

**Implementation Summary:**
- Replaced 500+ line if-else chain in `cypher_executor_execute_ast()` with single `dispatch_query_pattern()` call
- Fixed `handle_generic_transform` to properly find and use return clause for `build_query_results()`
- Added return value check for `dispatch_query_pattern()` to properly propagate errors

**Files Modified:**
- `src/backend/executor/cypher_executor.c` - Replaced if-else chain with dispatch call
- `src/backend/executor/query_dispatch.c` - Fixed generic handler's return clause handling

**Test Results:**
- 748/749 C tests passing (1 pre-existing failure unrelated to dispatch)
- All 160 Python tests passing