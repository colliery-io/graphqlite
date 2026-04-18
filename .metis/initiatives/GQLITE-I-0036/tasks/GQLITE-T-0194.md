---
id: wire-handle-create-to-run-trailing
level: task
title: "Wire handle_create to run trailing SET with CREATE var_map"
short_code: "GQLITE-T-0194"
created_at: 2026-04-18T21:43:00.640482+00:00
updated_at: 2026-04-18T22:11:34.936561+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Wire handle_create to run trailing SET with CREATE var_map

## Objective

Make `CREATE ... SET ...` dispatch to `execute_set_operations` with the CREATE clause's `variable_map`. Currently SET is silently dropped for bare `CREATE+SET` queries.

## Size: S (≤1 day)

## Resolves

- GQLITE-T-0188 partial (CREATE half).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `handle_create` in `src/backend/executor/query_dispatch.c` detects SET via `find_set_clause(query)`, switches to `execute_create_clause_with_varmap`, then calls `execute_set_operations(executor, set, create_vars, result)`. Frees var_map on both success and error.
- [ ] Preserves existing no-SET behavior exactly.
- [ ] Regression test added to `tests/functional/39_issue_regression_tests.sql` covering: `CREATE (n:L {k:v}) SET n.x = 1`, `CREATE (n:L) SET n += {k: v}`, `CREATE (n:L) SET n += $map`. All three must read back the expected properties.
- [ ] `CREATE+RETURN` (existing `handle_create_return`) continues to work unchanged.
- [ ] Full functional suite passes.

## Implementation Notes

- Precedent: UNWIND+CREATE+SET path at `query_dispatch.c:1471` does this with `execute_create_clause_with_varmap` → `execute_set_operations`. Copy the pattern.
- Decision: fold SET detection into existing `handle_create` rather than adding a new dispatch-table entry — keeps table from combinatorial growth. Document in a comment.
- Watch `CREATE+RETURN+SET`: existing dispatch routes to `handle_create_return`; that may need the same treatment. If not a valid query shape, leave out of scope and note.

## Dependencies

- None (independent of GQLITE-T-0193).

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