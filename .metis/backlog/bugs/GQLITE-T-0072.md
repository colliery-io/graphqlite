---
id: fix-sql-injection-vulnerability-in
level: task
title: "Fix SQL injection vulnerability in executor_merge.c"
short_code: "GQLITE-T-0072"
created_at: 2025-12-27T20:34:10.266740+00:00
updated_at: 2025-12-27T20:34:10.266740+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#bug"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Fix SQL injection vulnerability in executor_merge.c

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Migrate executor_merge.c from string interpolation to parameterized queries to eliminate SQL injection vulnerability.

## Priority
- [x] P0 - Critical (security vulnerability)

## Details

### Current Problem
Property keys from the AST (user input) are interpolated directly into SQL strings without escaping:

```c
// Lines 77-81 in executor_merge.c
offset += snprintf(sql + offset, sizeof(sql) - offset,
    " JOIN %s np%d ON n.id = np%d.node_id"
    " JOIN property_keys pk%d ON np%d.key_id = pk%d.id AND pk%d.key = '%s'"
    " AND np%d.value = %s",
    prop_table, i, i,
    i, i, i, i, pair->key,  // ← NOT ESCAPED!
    i, value_str);
```

### Correct Pattern (from cypher_schema.c)
```c
sqlite3_bind_text(stmt, 2, label, -1, SQLITE_STATIC);  // Safe
```

### Files to Modify
- `src/backend/executor/executor_merge.c` - Lines 26-87, 118-176

### Impact Assessment
- **Affected Users**: All users of MERGE with property matching
- **Reproduction**: `MERGE (n:Label {key'; DROP TABLE nodes;--: 'value'})`
- **Expected vs Actual**: Should reject/escape malicious input, currently interpolates directly

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