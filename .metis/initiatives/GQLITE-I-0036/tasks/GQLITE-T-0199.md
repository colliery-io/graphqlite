---
id: close-out-blocked-issue-61-bugs
level: task
title: "Close out blocked issue-61 bugs and promote regression tests"
short_code: "GQLITE-T-0199"
created_at: 2026-04-18T21:43:07.066413+00:00
updated_at: 2026-04-18T22:43:16.544264+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Close out blocked issue-61 bugs and promote regression tests

## Objective

After the dispatcher fixes land, re-validate each previously-blocked sub-bug of issue #61, transition the tickets to completed, and promote their repros from the expected-failures file to the regression-pass file.

## Size: XS (≤2 hours)

## Depends on

- GQLITE-T-0193 through T-0197 (all functional fixes).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Re-run the original repros from:
  - GQLITE-T-0188 (CREATE/MERGE + bare SET) — confirm properties persist.
  - GQLITE-T-0189 (MATCH+MATCH+MERGE+SET rel var) — confirm no "Unbound variable" error.
  - GQLITE-T-0190 (MATCH+MATCH+CREATE rel) — confirm no phantom node; traversal returns real target properties.
- [ ] Each ticket gets a status-update entry referencing this task and the resolving PR/commit.
- [ ] Transition T-0188, T-0189, T-0190 from `blocked` to `completed`.
- [ ] If any repro is still failing, file a follow-up rather than force-completing.
- [ ] Move repros from `tests/functional/11_issue_repro_expected_failures.sql` (if present) to `tests/functional/39_issue_regression_tests.sql`, matching the existing promotion pattern (see that file's header).
- [ ] Final full functional suite run — no regressions.

## Implementation Notes

- Some repros may still fail because they hit GQLITE-T-0185 (UNWIND transform) or GQLITE-T-0191 (multi-property MATCH alias reuse) — **out of scope** for GQLITE-I-0036. If a repro is blocked by one of those, leave the ticket blocked with a pointer to the responsible bug.

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