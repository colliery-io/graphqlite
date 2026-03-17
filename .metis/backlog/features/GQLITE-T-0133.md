---
id: call-subquery-support
level: task
title: "CALL subquery support"
short_code: "GQLITE-T-0133"
created_at: 2026-03-17T13:40:58.710070+00:00
updated_at: 2026-03-17T18:58:20.298435+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/blocked"


exit_criteria_met: false
initiative_id: NULL
---

# CALL subquery support

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Implement `CALL { subquery }` syntax for inline subquery evaluation. Subquery can read/write and returns results to the outer query. Also implement `COUNT { MATCH pattern }` and `COLLECT { MATCH pattern RETURN expr }` subquery expressions. `CALL` keyword exists in grammar but has no rules. Coverage matrix Sections 3, 6.5.

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

## Status Updates

### Blocked — Scope too large for single task

**Assessment**: CALL { subquery } requires changes across every layer:
1. **Grammar**: New `CALL '{' clause_list '}'` rule as a clause type — the `CALL` keyword exists but has no production rules
2. **AST**: New `AST_NODE_CALL_SUBQUERY` node type with embedded clause list
3. **Transform**: New `transform_call_subquery.c` — must generate a CTE from the inner clause list, manage variable import (outer vars visible in subquery) and export (subquery RETURN vars join outer scope)
4. **Executor**: New dispatch pattern in `query_dispatch.c` — must handle CALL as a clause that can appear mid-pipeline (after MATCH, before RETURN)
5. **Variable scope**: Subquery has its own scope but imports outer variables — the `transform_var_context` needs scope nesting

**Recommendation**: Decompose into a Metis initiative with 3-4 subtasks:
- T1: Grammar + AST for CALL { }
- T2: Transform (CTE generation with scope import/export)
- T3: Executor dispatch integration
- T4: COUNT { } and COLLECT { } subquery expressions

**Not blocked by anything external** — just needs to be properly scoped as an initiative rather than a single task.