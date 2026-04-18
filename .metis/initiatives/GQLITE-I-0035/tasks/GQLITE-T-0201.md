---
id: fill-node-rel-symmetry-gaps-int
level: task
title: "Fill node/rel symmetry gaps: int/real/bool/json traversal read-back"
short_code: "GQLITE-T-0201"
created_at: 2026-04-18T23:05:56.508488+00:00
updated_at: 2026-04-18T23:08:03.399624+00:00
parent: GQLITE-I-0035
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0035
---

# Fill node/rel symmetry gaps: int/real/bool/json traversal read-back

## Objective

Fill the node/rel endpoint property-access gaps in Section 2 of the matrix. Currently only TEXT is covered for `s.k / r.k / t.k` traversal read-back; INTEGER, REAL, BOOLEAN, JSON, and LIST are GAP.

## Size: S (≤1 day)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] One passing test in `tests/functional/39_issue_regression_tests.sql` per scalar type × endpoint position (source / relationship / target).
- [ ] Each test creates a path `(a)-[r]->(b)` where both `a`, `r`, `b` carry properties of that scalar type, then reads all three back in a single RETURN.
- [ ] `docs/testing/semantic-coverage-matrix.md` Section 2 updated to link each new test.
- [ ] Full functional suite passes.

## Implementation Notes

- JSON values stored as TEXT via `json_extract` — assert the read-back as a JSON text, not a native object, to match the extension's current surface.
- LIST is currently stored as JSON text; same assertion form.
- Boolean read-back returns `1`/`0` integers in some paths; document the expected form in the test comment.

## Dependencies

- None.

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