---
id: temporal-types-and-functions-date
level: task
title: "Temporal types and functions (date, time, datetime, duration construction and arithmetic)"
short_code: "GQLITE-T-0131"
created_at: 2026-03-17T13:40:21.332883+00:00
updated_at: 2026-03-17T19:12:13.684239+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Temporal types and functions (date, time, datetime, duration construction and arithmetic)

*This template includes sections for various types of tasks. Delete sections that don't apply to your specific use case.*

## Parent Initiative **[CONDITIONAL: Assigned Task]**

[[Parent Initiative]]

## Objective

Implement full temporal support: `date({year, month, day})`, `date(string)`, `time({...})`, `datetime({...})`, `localdatetime({...})`, `duration({...})`, `duration(string)`, `duration.between()`, `duration.inMonths/Days/Seconds()`, temporal truncation, and temporal arithmetic (`date + duration`, `datetime - duration`). Currently only `date()`, `time()`, `datetime()` return current values. This is the single largest spec gap. Coverage matrix Sections 7.10, 5.9, 8.1.

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

## Status Updates

### Implementation Complete

**Map-based construction (new):**
- `date({year: 2024, month: 3, day: 15})` → `"2024-03-15"` via printf/json_extract
- `time({hour: 14, minute: 30, second: 45})` → `"14:30:45"`
- `datetime({year: 2024, month: 6, day: 15, hour: 10, minute: 30})` → `"2024-06-15T10:30:00"`

**Duration type (new):**
- `duration({days: 5, hours: 3})` → JSON object `{"years":0,"months":0,"days":5,"hours":3,...}`
- `duration(string)` → passthrough for ISO 8601 strings

**Epoch conversion (new):**
- `datetimeFromEpoch(seconds)` → `datetime(seconds, 'unixepoch')`
- `datetimeFromEpochMillis(ms)` → `datetime(ms/1000, 'unixepoch')`

**Duration utility functions (new):**
- `durationInDays(t1, t2)` → integer days via julianday difference
- `durationInSeconds(t1, t2)` → integer seconds
- `durationInMonths(t1, t2)` → approximate months (30.44 days/month)
- `durationBetween(t1, t2)` → JSON duration object

**Truncation (new):**
- `dateTruncate(unit, temporal)` → `date(temporal, 'start of ' || unit)`

**Also added:** `localtime()` as alias for `time()`

**Not implemented:** ISO 8601 duration string parsing (P1Y2M3D), temporal property types in storage (stored as text). These would need custom C functions.

**Tests**: 880 unit, 226 Python pass