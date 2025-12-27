---
id: implement-remove-clause-execution
level: task
title: "Implement REMOVE clause execution"
short_code: "GQLITE-T-0036"
created_at: 2025-12-25T16:34:15.255814+00:00
updated_at: 2025-12-25T23:08:37.522776+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Implement REMOVE clause execution

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Complete the REMOVE clause execution path so `MATCH (n) REMOVE n.prop` queries work end-to-end.

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

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `MATCH (n {name: 'x'}) REMOVE n.age RETURN n` executes successfully
- [ ] `MATCH (n) REMOVE n:Label` removes labels from nodes
- [ ] `MATCH (n) REMOVE n.prop1, n.prop2` removes multiple properties
- [ ] Unit tests added in tests/test_executor_remove.c
- [ ] Functional tests added in tests/functional/

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

## Implementation Notes

### Current State
- Parser: ✅ Parses REMOVE syntax (cypher_gram.y lines 548-587)
- AST: ✅ AST_NODE_REMOVE defined (cypher_ast.h line 25)
- Transform: ✅ transform_remove_clause() generates DELETE SQL (transform_remove.c)
- Executor: ❌ No `remove_clause` variable in executor dispatch (cypher_executor.c lines 117-161)

### Technical Approach
1. Add `cypher_remove *remove_clause` variable in clause scanning loop (cypher_executor.c ~line 125)
2. Add case for `AST_NODE_REMOVE` in the switch (line 130)
3. Add execution path for `MATCH...REMOVE` pattern (after line 300)
4. Route through transform layer similar to how WITH/UNWIND work

### Files to Modify
- src/backend/executor/cypher_executor.c
- src/backend/executor/executor_internal.h (if new function needed)

### Dependencies
None - transform layer already works

## Status Updates **[REQUIRED]**

*To be added during implementation*