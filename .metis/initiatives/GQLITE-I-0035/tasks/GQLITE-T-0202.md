---
id: fill-on-match-set-and-set-plus
level: task
title: "Fill ON MATCH SET and SET-plus-equals on rel var gaps"
short_code: "GQLITE-T-0202"
created_at: 2026-04-18T23:05:57.682385+00:00
updated_at: 2026-04-18T23:08:48.545458+00:00
parent: GQLITE-I-0035
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0035
---

# Fill ON MATCH SET and SET-plus-equals on rel var gaps

## Objective

Fill the `ON MATCH SET` and `SET n += {..}` gaps for relationship variables in Section 1 and Section 3 of the matrix. The `ON CREATE SET` path is covered (T-0187); the symmetric `ON MATCH SET` is not. `SET r += {..}` has no test at all.

## Size: S (≤1 day)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Regression tests in `tests/functional/39_issue_regression_tests.sql` for:
  - `MERGE (a)-[r:T]->(b) ON MATCH SET r.k = v` (literal)
  - `MERGE (a)-[r:T]->(b) ON MATCH SET r.k = $p` (parameter)
  - `MERGE (a)-[r:T]->(b) SET r += {k1:v1, k2:v2}` (literal map)
  - `MERGE (a)-[r:T]->(b) SET r += $m` (parameterized map)
- [ ] Each test runs MERGE twice: first create, then re-MERGE to hit the MATCH branch, verify properties differ as expected.
- [ ] `docs/testing/semantic-coverage-matrix.md` Section 1 and Section 3 updated to link new tests.
- [ ] If any test reveals a bug, file a ticket and mark this task blocked on it (do not paper over).

## Notes

- If `SET r +=` reveals dispatcher holes similar to GQLITE-I-0036's, file a follow-up and log the RCA.

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0035]]

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