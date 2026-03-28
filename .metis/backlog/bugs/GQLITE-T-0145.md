---
id: delete-return-count-always-returns
level: task
title: "DELETE + RETURN COUNT always returns 0"
short_code: "GQLITE-T-0145"
created_at: 2026-03-28T00:47:01.298353+00:00
updated_at: 2026-03-28T01:12:53.919973+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/active"


exit_criteria_met: false
initiative_id: NULL
---

# DELETE + RETURN COUNT always returns 0

**GitHub Issue**: #39
**Priority**: P2 - Medium

## Objective

Fix DELETE + RETURN COUNT to report the actual number of deleted entities instead of always returning 0.

## Bug Description

`MATCH (n:Temp) DETACH DELETE n RETURN COUNT(n) AS deleted_count` returns 0 even though nodes are successfully deleted. The count goes from 3 to 0 in a subsequent query, but the inline RETURN reports 0.

## Root Cause

In `query_dispatch.c` (lines 645-663), `handle_match_delete` first executes the DELETE (which removes the nodes), then re-executes the MATCH+RETURN query against the now-empty graph. COUNT operates on zero rows because the nodes no longer exist.

This is acknowledged in archived task `GQLITE-T-0110` line 107: "deleted entities won't be found by the re-query."

The fix requires capturing the matched row count before deletion and injecting it into the RETURN result.

## Reproduction

```cypher
CREATE (n:Temp {id: '1'})
CREATE (n:Temp {id: '2'})
CREATE (n:Temp {id: '3'})

MATCH (n:Temp) RETURN count(n) AS cnt  -- Returns 3
MATCH (n:Temp) DETACH DELETE n RETURN COUNT(n) AS deleted_count  -- Returns 0 (bug)
MATCH (n:Temp) RETURN count(n) AS cnt  -- Returns 0 (nodes gone)
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `DELETE n RETURN COUNT(n)` returns the number of deleted nodes
- [ ] `DETACH DELETE n RETURN COUNT(n)` returns the number of deleted nodes
- [ ] Simple `DELETE` without RETURN still works
- [ ] Repro test passes: `TestIssue39` in `test_issue_repro.py`, test 39b in `11_issue_repro.sql`

## Affected Files

- `src/backend/executor/query_dispatch.c` — `handle_match_delete` needs to capture pre-delete count
- `src/backend/executor/executor_delete.c` — may need to pass count info to result

## Status Updates

### 2026-03-27: Implementation complete

**Change:** `src/backend/executor/query_dispatch.c` — added `synthesize_delete_return()` function that detects when a RETURN clause after DELETE contains only COUNT aggregates, and synthesizes the result directly from `nodes_deleted + relationships_deleted` instead of re-querying the now-empty graph. Sets `data_types` to `SQLITE_INTEGER` so the JSON output renders as a number, not a string.

Falls back to the original re-query path for non-COUNT RETURN clauses.

**Test results:**
- 921/921 C unit tests pass
- `TestIssue39::test_delete_return_count` — PASSES (was returning 0, now returns 3)
- Cypher: `[{"deleted_count":3}]` (correct integer output)
- All 3 existing Python DELETE tests pass
- All 43 functional test files pass

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

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