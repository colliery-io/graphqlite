---
id: expose-execute-merge-clause-with
level: task
title: "Expose execute_merge_clause_with_varmap out-parameter variant"
short_code: "GQLITE-T-0193"
created_at: 2026-04-18T21:42:59.116087+00:00
updated_at: 2026-04-18T22:09:24.789676+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Expose execute_merge_clause_with_varmap out-parameter variant

## Objective

Add an `execute_merge_clause_with_varmap` variant that exposes the MERGE handler's internal `variable_map` through an out-parameter, mirroring the existing `execute_create_clause_with_varmap` signature. Foundation every subsequent task in GQLITE-I-0036 builds on.

## Size: S (≤1 day)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] New function `int execute_merge_clause_with_varmap(cypher_executor *executor, cypher_merge *merge, cypher_result *result, variable_map **out_var_map)` declared in the executor header and implemented in `src/backend/executor/executor_merge.c`.
- [ ] Existing `execute_merge_clause` / `execute_merge_clause_with_vars` become thin wrappers that discard the out map (free it before returning).
- [ ] Ownership contract: on success, caller owns `*out_var_map` and must `free_variable_map()` it. On error, the function frees it and sets `*out_var_map = NULL`.
- [ ] All existing MERGE call sites continue to pass without change.
- [ ] Full functional suite passes with no regression.

## Implementation Notes

- Model on `execute_create_clause_with_varmap` (executor_create.c); same signature shape, same ownership discipline.
- The var_map inside `execute_merge_clause_with_vars` is already built up with node and edge bindings during MERGE execution. Hoist the `free_variable_map(var_map)` and return it via out-param instead.
- Several nested code paths inside MERGE (node-only, rel-only, match+merge). All should contribute to the same var_map that gets exposed. Verify via debug test.

## Unblocks

- GQLITE-T-0195, GQLITE-T-0196.

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0036]]

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

*To be added during implementation*