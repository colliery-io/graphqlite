---
id: document-and-enumerate-existing
level: task
title: "Document and enumerate existing query patterns"
short_code: "GQLITE-T-0068"
created_at: 2025-12-27T17:40:37.807415+00:00
updated_at: 2025-12-27T17:53:19.171630+00:00
parent: GQLITE-I-0026
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: GQLITE-I-0026
---

# Document and enumerate existing query patterns

## Objective

Analyze `cypher_executor.c` if-else chain and document all existing query patterns.

## Deliverables

1. **Pattern inventory** - table of all patterns with:
   - Pattern name
   - Required clauses
   - Forbidden clauses
   - Current handler (inline vs named function)
   - Line numbers in current code

2. **Pattern registry array** in `query_dispatch.c`:
```c
static const query_pattern patterns[] = {
    { "UNWIND+CREATE", CLAUSE_UNWIND|CLAUSE_CREATE, CLAUSE_RETURN|CLAUSE_MATCH, ... },
    { "MATCH+RETURN", CLAUSE_MATCH|CLAUSE_RETURN, CLAUSE_CREATE|CLAUSE_SET, ... },
    // ... all patterns
    { "GENERIC", 0, 0, execute_generic_query, 0 },  // fallback
    { NULL, 0, 0, NULL, 0 }  // sentinel
};
```

## Known patterns to document

| Pattern | Required | Forbidden | Handler |
|---------|----------|-----------|---------|
| UNWIND+CREATE | UNWIND, CREATE | RETURN, MATCH | inline (~70 lines) |
| MATCH+RETURN | MATCH, RETURN | CREATE, SET, DELETE, MERGE | transform pipeline |
| MATCH+OPTIONAL+RETURN | MATCH, OPTIONAL, RETURN | CREATE, SET, DELETE | transform pipeline |
| MATCH+CREATE | MATCH, CREATE | - | execute_match_create_query |
| MATCH+SET | MATCH, SET | - | execute_match_set_query |
| MATCH+DELETE | MATCH, DELETE | - | execute_match_delete_query |
| MATCH+MERGE | MATCH, MERGE | - | execute_match_merge_query |
| CREATE only | CREATE | MATCH | direct insert |
| RETURN only | RETURN | MATCH | transform pipeline |

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] All patterns from if-else chain identified
- [ ] Pattern registry compiles
- [ ] No patterns missed (verified by code review)
- [ ] Priority ordering correct (specific patterns before general)

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[GQLITE-I-0026]]

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