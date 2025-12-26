---
id: implement-in-membership-operator
level: task
title: "Implement IN membership operator"
short_code: "GQLITE-T-0038"
created_at: 2025-12-25T16:34:15.385098+00:00
updated_at: 2025-12-25T23:53:09.140215+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Implement IN membership operator

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Add support for the IN membership operator so expressions like `n.status IN ['active', 'pending']` work.

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

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `RETURN 5 IN [1, 2, 5, 10]` returns true
- [ ] `RETURN 'x' IN ['a', 'b', 'c']` returns false
- [ ] `MATCH (n) WHERE n.status IN ['active', 'pending'] RETURN n` filters correctly
- [ ] `WITH [1,2,3] AS list RETURN 2 IN list` works with variables
- [ ] Parser and transform tests added

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

## Implementation Notes

### Current State
- Token: ✅ IN token defined (cypher_gram.y line 91)
- Precedence: ✅ Defined (line 150: `%left IN IS`)
- Current usage: Only in FOREACH, list predicates (all/any/none/single), reduce, list comprehensions
- Membership: ❌ Cannot write `value IN [1, 2, 3]` as binary operator

### Technical Approach
1. Add `BINARY_OP_IN` to binary_op_type enum (cypher_ast.h)
2. Add grammar rule in cypher_gram.y:
   ```c
   | expr IN expr      { $$ = (ast_node*)make_binary_op(BINARY_OP_IN, $1, $3, @2.first_line); }
   ```
3. Add transform handler in transform_expr_ops.c:
   - For literal lists: `expr IN (val1, val2, val3)`
   - For variable lists: `expr IN (SELECT value FROM json_each(list_expr))`

### Files to Modify
- src/include/parser/cypher_ast.h
- src/backend/parser/cypher_gram.y
- src/backend/transform/transform_expr_ops.c

### Risk Considerations
Grammar conflict possible with existing `x IN list` usage in FOREACH/list predicates - may need precedence adjustment or context-sensitive parsing

## Status Updates **[REQUIRED]**

*To be added during implementation*