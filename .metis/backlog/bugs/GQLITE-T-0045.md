---
id: fix-array-index-syntax-items-idx
level: task
title: "Fix array index syntax (items[idx]) not supported after WITH"
short_code: "GQLITE-T-0045"
created_at: 2025-12-26T03:16:29.456577+00:00
updated_at: 2025-12-26T03:30:02.446840+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Fix array index syntax (items[idx]) not supported after WITH

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Add support for array index syntax (`items[idx]`) in expressions after WITH clause.

## Reproduction

```cypher
WITH ['a', 'b', 'c'] AS items
UNWIND range(0, size(items) - 1) AS idx
RETURN idx, items[idx] AS item
-- Expected: 3 rows with index and corresponding item
-- Actual: Parser error - unexpected '[', expecting end of file
```

## Root Cause

The parser doesn't recognize array index access syntax (`identifier[expr]`) when the identifier is a projected variable from a WITH clause. The subscript operator needs to be supported in the expression grammar.

## Files
- `src/backend/parser/cypher_gram.y` - Parser grammar
- `src/backend/transform/transform_expr_ops.c` - Expression transformation

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria
- [ ] `items[0]` syntax works after WITH projection
- [ ] `items[idx]` works with variable index
- [ ] Python test `test_unwind_with_index` passes

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

### Resolution (Completed)
Fixed by adding subscript expression support to the parser and transformer:
- Added `AST_NODE_SUBSCRIPT` and `cypher_subscript` struct to `cypher_ast.h`
- Added `make_subscript()` function to `cypher_ast.c`
- Added grammar rules for `IDENTIFIER '[' expr ']'` and `'(' expr ')' '[' expr ']'` in `cypher_gram.y`
- Added transform case for `AST_NODE_SUBSCRIPT` in `transform_return.c` using SQLite's `json_extract()`

All Python tests pass (98 passed, 3 xfailed for OPTIONAL MATCH).