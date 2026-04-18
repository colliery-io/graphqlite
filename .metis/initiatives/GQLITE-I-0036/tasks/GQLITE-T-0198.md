---
id: cross-clause-dispatch-combination
level: task
title: "Cross-clause dispatch combination smoke tests"
short_code: "GQLITE-T-0198"
created_at: 2026-04-18T21:43:05.946950+00:00
updated_at: 2026-04-18T22:42:44.197666+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Cross-clause dispatch combination smoke tests

## Objective

Add a focused regression test file covering the cross-product of write clauses × trailing clauses, so future regressions of this dispatcher gap are caught immediately. Feeds into GQLITE-I-0035 (semantic coverage matrix).

## Size: S (≤1 day)

## Depends on

- GQLITE-T-0194, T-0195, T-0196, T-0197 (the functional fixes that make these combinations pass).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] New file `tests/functional/41_dispatch_threading.sql` (or additions to `39_issue_regression_tests.sql`) with at least one test per cell of the matrix, each verifying both the write and a read-back:
  - Write shape: `CREATE`, `MERGE`, `MATCH+CREATE`, `MATCH+MERGE`, `MATCH+MATCH+CREATE`, `MATCH+MATCH+MERGE`.
  - Trailing clause: `SET n.x = v` (scalar), `SET n += {…}` (map-merge), `SET n += $p` (parameterized), `ON CREATE SET n.x = $p` (internal, MERGE only).
  - Value source: literal, `$param`.
  - Target: node variable, relationship variable.
- [ ] Every cell either has a passing test or is explicitly marked "N/A" with a one-line reason.
- [ ] Runs as part of `angreal test functional`.
- [ ] Negative test: pre-existing error behavior for genuinely unbound variables still produces a clean error, not a crash.

## Implementation Notes

- Short, descriptive test names referencing the originating ticket, e.g. `-- #61.2 T-0186: $param in CREATE rel props`.
- Align with the matrix document from GQLITE-I-0035 — these cells go into that matrix as "covered by test 41_dispatch_threading.sql:LINE".

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