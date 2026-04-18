---
id: thread-match-merge-var-map-through
level: task
title: "Thread MATCH+MERGE var_map through to trailing SET"
short_code: "GQLITE-T-0196"
created_at: 2026-04-18T21:43:03.298158+00:00
updated_at: 2026-04-18T22:35:43.548052+00:00
parent: GQLITE-I-0036
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0036
---

# Thread MATCH+MERGE var_map through to trailing SET

## Objective

Thread the MATCH+MERGE composite var_map into a trailing SET so `MATCH (a) MERGE (a)-[r:T]->(b) SET r.x = ...` stops raising `Unbound variable in SET: r`.

## Size: M (1-3 days)

## Resolves

- GQLITE-T-0189.

## Depends on

- GQLITE-T-0193 (needs the MERGE varmap variant).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `handle_match_merge` in `src/backend/executor/query_dispatch.c` detects a trailing SET clause and threads the combined MATCH + MERGE bindings into `execute_set_operations`.
- [ ] `MATCH (a:N) MERGE (a)-[r:T {k:$v}]->(b:N) SET r.x = $y` persists `r.x` with the bound parameter value.
- [ ] `MATCH (a) MERGE (a)-[r:T]->(b) SET r.x = 1, r.y = 2` persists both properties.
- [ ] Regression tests added.
- [ ] Confirm `MATCH+MERGE+RETURN` (if allowed) still works.
- [ ] Full functional suite passes.

## Implementation Notes

- Current `handle_match_merge` calls `execute_match_merge_query` then optionally `execute_match_return_query` for RETURN. SET insertion lands between them.
- The var_map produced inside `execute_match_merge_query` needs to be exposed. Prefer a new `execute_match_merge_query_with_varmap` variant (mirrors CREATE/MERGE wrapper pattern) over refactoring the existing signature.
- `execute_match_merge_query` reads MATCH bindings then calls into MERGE. Both MATCH-side and MERGE-side bindings must end up in the returned var_map.

## Interactions

- Once GQLITE-T-0197 lands (multi-MATCH aggregation), the same handler needs to union *all* MATCH bindings. Keep this task's SET-wiring code agnostic to match-count; treat returned var_map as "the scope before the trailing SET".

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