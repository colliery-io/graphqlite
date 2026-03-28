---
id: size-labels-n-returns-string
level: task
title: "size(labels(n)) returns string length instead of list length"
short_code: "GQLITE-T-0147"
created_at: 2026-03-28T00:47:03.841649+00:00
updated_at: 2026-03-28T02:15:34.040563+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# size(labels(n)) returns string length instead of list length

**GitHub Issue**: #42
**Priority**: P2 - Medium

## Objective

Fix `size()` to return the element count when applied to list-returning functions like `labels()`, `keys()`, etc., instead of the string length of the JSON representation.

## Bug Description

`size(labels(n))` returns the character count of the JSON string (e.g., 12 for `["LabelA42"]`) instead of the number of labels (1). `size()` on literal lists works correctly.

## Root Cause

In `src/backend/transform/transform_func_string.c` (lines 54-68), `size()` only checks for `AST_NODE_LIST` (literal list syntax) to decide whether to use `json_array_length()`. When the argument is a function call like `labels()`, it falls through to `LENGTH()` (string length).

`labels()` in `transform_func_entity.c` returns a JSON array via `json_group_array()`, but `size()` doesn't recognize it as a list.

## Reproduction

```cypher
CREATE (a:LabelA {id: 'a'})
CREATE (b:LabelA:LabelB {id: 'b'})

MATCH (n:LabelA {id: 'a'}) RETURN size(labels(n)) AS sz  -- Returns 12 (bug), expected 1
MATCH (n {id: 'b'}) RETURN size(labels(n)) AS sz          -- Returns 23 (bug), expected 2
RETURN size([1, 2, 3]) AS sz                               -- Returns 3 (correct, control)
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `size(labels(n))` returns the number of labels
- [ ] `size([literal list])` still works correctly
- [ ] `size()` on strings still returns string length
- [ ] Repro tests pass: `TestIssue42` in `test_issue_repro.py`, tests 42a/42b in `11_issue_repro.sql`

## Affected Files

- `src/backend/transform/transform_func_string.c` — detect list-returning function calls in `size()` and use `json_array_length()` instead of `LENGTH()`

## Status Updates

### 2026-03-27: Implementation complete

**Change:** `src/backend/transform/transform_func_string.c` — extended `size()` handling to detect `AST_NODE_FUNCTION_CALL` arguments from known list-returning functions (`labels`, `keys`, `nodes`, `relationships`, `collect`, `range`, `tail`, `split`, `json_keys`) and use `json_array_length()` instead of `LENGTH()`.

**Test results:**
- 921/921 C unit tests pass
- `TestIssue42::test_size_labels_single` — PASSES (was returning 12, now returns 1)
- `TestIssue42::test_size_labels_multiple` — PASSES (was returning 23, now returns 2)
- `size("hello")` = 5 (string length still works)
- `size([1,2,3])` = 3 (literal list still works)
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