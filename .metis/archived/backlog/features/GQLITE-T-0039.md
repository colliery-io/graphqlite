---
id: implement-starts-with-ends-with
level: task
title: "Implement STARTS WITH, ENDS WITH, CONTAINS as infix operators"
short_code: "GQLITE-T-0039"
created_at: 2025-12-25T16:34:15.449767+00:00
updated_at: 2025-12-25T16:34:15.449767+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Implement STARTS WITH, ENDS WITH, CONTAINS as infix operators

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective **[REQUIRED]**

Add STARTS WITH, ENDS WITH, and CONTAINS as infix operators per Cypher spec (currently only available as functions).

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

## Acceptance Criteria **[REQUIRED]**

- [ ] `RETURN 'hello' STARTS WITH 'he'` returns true
- [ ] `RETURN 'hello' ENDS WITH 'lo'` returns true
- [ ] `RETURN 'hello' CONTAINS 'ell'` returns true
- [ ] `MATCH (n) WHERE n.name STARTS WITH 'A' RETURN n` filters correctly
- [ ] Existing functions `startsWith()`, `endsWith()`, `contains()` continue to work
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
- Functions: ✅ `startsWith()`, `endsWith()`, `contains()` implemented (transform_func_string.c lines 214-258)
- Operators: ❌ Cannot write `n.name STARTS WITH 'A'`
- Tokens: ❌ STARTS, ENDS keywords not defined

### Technical Approach
1. Add tokens in cypher_scanner.l: `STARTS`, `ENDS` (CONTAINS may conflict - check)
2. Add token declarations in cypher_gram.y
3. Add grammar rules:
   ```c
   | expr STARTS WITH expr  { /* reuse startsWith function logic */ }
   | expr ENDS WITH expr    { /* reuse endsWith function logic */ }
   | expr CONTAINS expr     { /* reuse contains function logic */ }
   ```
4. In transform, convert to same SQL as existing functions:
   - STARTS WITH → `str LIKE prefix || '%'`
   - ENDS WITH → `str LIKE '%' || suffix`
   - CONTAINS → `INSTR(str, substr) > 0`

### Files to Modify
- src/backend/parser/cypher_scanner.l
- src/backend/parser/cypher_gram.y
- src/backend/transform/transform_expr_ops.c (or create new AST node types)

### Alternative Approach
Could create synthetic function call AST nodes during parsing to reuse existing transform_func_string.c logic entirely

## Status Updates **[REQUIRED]**

*To be added during implementation*