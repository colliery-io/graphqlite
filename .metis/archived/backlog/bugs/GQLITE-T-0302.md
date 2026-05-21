---
id: json-renderer-drops-0-from
level: task
title: "JSON renderer drops .0 from integral floats (e.g. 20 instead of 20.0)"
short_code: "GQLITE-T-0302"
created_at: 2026-05-20T16:16:11.824680+00:00
updated_at: 2026-05-20T19:51:19.824694+00:00
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

# JSON renderer drops .0 from integral floats (e.g. 20 instead of 20.0)

## Reproducer

```sql
.load build/graphqlite.dylib
SELECT cypher('CREATE ({price: 10.0}), ({price: 20.0}), ({price: 30.0})');
SELECT cypher('MATCH (n) RETURN percentileDisc(n.price, 0.5) AS p');
-- Got:      [{"p":20}]
-- Expected: [{"p":20.0}]
```

Generalizes to any whole-number float — `RETURN 20.0` returns `20`,
`RETURN 1.0` returns `1`, etc.

## Context — surfaced 2026-05-20

Discovered while landing M15 (percentile aggregates, GQLITE-T-0300).
Math is correct; JSON formatter is the issue. openCypher TCK
`Aggregation6 [1]/[2]` expect literal `20.0` and currently
match-fail because we emit `20`.

## Root cause

`src/extension.c:295-323` JSON renderer for `SQLITE_FLOAT` cells uses
`sqlite3_column_text(stmt, col)` whose `%g`-based formatting drops
trailing zeros: `20.0` → `"20"`. The override that DOES preserve
`.0` lives in `executor_match.c:391-405` but only fires for the
MATCH path; aggregate results don't go through it.

## Fix sketch

In `src/extension.c` JSON renderer, when `col_type == SQLITE_FLOAT`:
after copying val to json_result, scan for `.`/`e`/`E` and append
`.0` if absent. Mirror the fix on the agtype path in
`executor_match.c`.

## Impact

~6 TCK scenarios in Aggregation6; generalizes to any whole-float
RETURN.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `RETURN 1.0 AS x` returns `[{"x":1.0}]`
- [x] `RETURN 20.0 AS y` returns `[{"y":20.0}]`
- [x] `RETURN 1.5 AS z` returns `[{"z":1.5}]` (unchanged)
- [x] `MATCH (n) RETURN percentileDisc(n.price, 0.5)` returns `20.0` (was `20`)
- [x] Integer rendering unchanged (`RETURN 1 AS i` → `1`)
- [x] All 937 unit tests + functional suite pass

## Status Updates

**2026-05-20** — Completed.

Split the `SQLITE_INTEGER || SQLITE_FLOAT` branch in `src/extension.c` JSON renderer. For `SQLITE_FLOAT`: after copying the textual value, scan for `.`, `e`, or `E`; if absent (whole-number float), append `.0`. The agtype-path `%.17g` formatter in `executor_match.c` produces a no-dot string for whole floats (`"20"`), so the renderer-side fix is sufficient — the column type tags it as `SQLITE_FLOAT`, the renderer adds `.0`.

---

*Sections below are unused template scaffolding.*

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