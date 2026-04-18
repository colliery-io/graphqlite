---
id: wire-handle-merge-to-run-trailing
level: task
title: "Wire handle_merge to run trailing SET with MERGE var_map"
short_code: "GQLITE-T-0195"
created_at: 2026-04-18T21:43:01.933004+00:00
updated_at: 2026-04-18T22:13:21.250658+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Wire handle_merge to run trailing SET with MERGE var_map

## Objective

Make `MERGE ... SET ...` (no ON CREATE / ON MATCH clauses) dispatch to `execute_set_operations` with the MERGE clause's `variable_map`. Currently the SET clause is silently dropped.

## Size: S (≤1 day)

## Resolves

- GQLITE-T-0188 partial (MERGE half).

## Depends on

- GQLITE-T-0193 (requires `execute_merge_clause_with_varmap`).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `handle_merge` in `src/backend/executor/query_dispatch.c` detects trailing SET via `find_set_clause(query)`, invokes `execute_merge_clause_with_varmap` (from GQLITE-T-0193), then runs `execute_set_operations(executor, set, merge_vars, result)`. Free var_map on both success and error.
- [ ] ON CREATE / ON MATCH SET inside the MERGE continues to run before the trailing SET (already wired from the T-0187 fix).
- [ ] Regression test added covering: `MERGE (n:L {k:v}) SET n.x = 1`, `MERGE (n:L {k:v}) SET n += {a:1,b:2}`, `MERGE (n:L {k:v}) ON CREATE SET n.y = 2 SET n.x = 1` (combines internal and trailing SET).
- [ ] Node MERGE+SET and edge-MERGE+SET both work.
- [ ] Full functional suite passes.

## Implementation Notes

- Same pattern as GQLITE-T-0194. Be careful: ON CREATE SET runs *inside* `execute_merge_clause`; trailing SET runs *after* — both read the same var_map.

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