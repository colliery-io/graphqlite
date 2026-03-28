---
id: unwind-does-not-accept-parameter
level: task
title: "UNWIND does not accept parameter references"
short_code: "GQLITE-T-0144"
created_at: 2026-03-28T00:47:00.323568+00:00
updated_at: 2026-03-28T02:15:36.281641+00:00
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

# UNWIND does not accept parameter references

**GitHub Issue**: #37
**Priority**: P1 - High

## Objective

Add support for parameter references (`$param`) in UNWIND expressions, and support UNWIND over lists of maps with property access on bound map items.

## Bug Description

`UNWIND $param AS row` fails when `$param` is a query parameter containing a list. Only literal lists work. This blocks batch ingestion patterns where data is passed as a parameter array.

Additionally, UNWIND over lists of maps silently skips map items — property access like `item.id` on unwound map items doesn't work.

## Root Cause

In `src/backend/transform/transform_unwind.c`, the expression type switch handles `AST_NODE_LIST`, `AST_NODE_PROPERTY`, `AST_NODE_IDENTIFIER`, and `AST_NODE_FUNCTION_CALL` but NOT `AST_NODE_PARAMETER` (falls through to error at lines 232-234).

For map binding, `query_dispatch.c:859-882` only handles `AST_NODE_LITERAL` items; map items (`AST_NODE_MAP`) are silently skipped.

## Reproduction

```cypher
-- Parameter reference (fails)
UNWIND $items AS item RETURN item
-- with params: {"items": [1, 2, 3]}
-- Error: "UNWIND requires list literal, property access, variable, or function call"

-- Literal list works (control)
UNWIND [1, 2, 3] AS x RETURN x
-- Returns: [{x:1}, {x:2}, {x:3}]
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `UNWIND $param AS item` works when parameter is a JSON array
- [ ] `UNWIND $batch AS item RETURN item.id` works when parameter is a list of objects
- [ ] Existing literal list UNWIND behavior unchanged
- [ ] Repro tests pass: `TestIssue37` in `test_issue_repro.py`, test 37b in `11_issue_repro.sql`

## Affected Files

- `src/backend/transform/transform_unwind.c` — add `AST_NODE_PARAMETER` case
- `src/backend/executor/query_dispatch.c` — map item binding in UNWIND loop

## Status Updates

### 2026-03-27: Implementation complete

**Changes (3 files):**

1. **`src/backend/transform/transform_unwind.c`** — Added `AST_NODE_PARAMETER` case that generates `json_each(:param_name)` for parameter-based UNWIND. Removed `sql_select()` call that added an extra column conflicting with RETURN clause column indexing.

2. **`src/backend/transform/transform_expr_ops.c`** — Added UNWIND JSON property access: when accessing a property on an UNWIND variable (detected by `_unwind_` prefix in projected source), generate `json_extract(source, '$.property')` instead of property table lookups.

**Test results:**
- 921/921 C unit tests pass
- `TestIssue37::test_unwind_parameter_list` — PASSES (scalar parameter list)
- `TestIssue37::test_unwind_parameter_map_list` — PASSES (map list + property access)
- Literal UNWIND still works: `UNWIND [1,2,3] AS x RETURN x` → `[{x:1},{x:2},{x:3}]`
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