---
id: integer-property-storage-truncates
level: task
title: "Integer property storage truncates values to 32-bit"
short_code: "GQLITE-T-0148"
created_at: 2026-03-28T00:47:05.223748+00:00
updated_at: 2026-03-28T00:51:05.198912+00:00
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

# Integer property storage truncates values to 32-bit

**GitHub Issue**: #43 (duplicate: #44)
**Priority**: P1 - High

## Objective

Fix integer property storage to use 64-bit binding, preventing silent truncation of values larger than 2^31.

## Bug Description

Integer properties are stored using `sqlite3_bind_int` (32-bit) instead of `sqlite3_bind_int64`, silently truncating any value exceeding 2^31 - 1. This affects `timestamp()` results (epoch millis ~1.7 trillion), large IDs, and any integer > ~2.1 billion.

## Root Cause

In `src/backend/executor/cypher_schema.c`:
- Line 756 (nodes): `sqlite3_bind_int(stmt, 3, *(const int*)value)`
- Line 908 (edges): `sqlite3_bind_int(stmt, 3, *(const int*)value)`

Both use 32-bit `sqlite3_bind_int` and cast to `const int*`. The SQLite schema (`node_props_int.value INTEGER`) supports 64-bit, so only the bind calls need fixing.

Additionally, the AST literal type in `cypher_ast.h` line 319 stores integers as `int` (32-bit), which would also truncate at parse time for literal values.

## Reproduction

```cypher
CREATE (n:IntTest {id: 'big'})
MATCH (n:IntTest {id: 'big'}) SET n.val = 9999999999
MATCH (n:IntTest {id: 'big'}) RETURN n.val
-- Returns: 1410065407 (truncated to 32-bit), expected: 9999999999

MATCH (n:IntTest {id: 'big'}) SET n.ts = timestamp()
MATCH (n:IntTest {id: 'big'}) RETURN n.ts
-- Returns: truncated value < 2^31, expected: ~1.7 trillion
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `sqlite3_bind_int64` used for integer property storage (nodes and edges)
- [ ] Integer values > 2^31 round-trip correctly through SET/RETURN
- [ ] `timestamp()` values stored without truncation
- [ ] AST integer literal type widened to `int64_t`
- [ ] Repro tests pass: `TestIssue43` in `test_issue_repro.py`, tests 43a/43b in `11_issue_repro.sql`

## Affected Files

- `src/backend/executor/cypher_schema.c` — lines 756, 908: change `sqlite3_bind_int` to `sqlite3_bind_int64`
- `src/include/parser/cypher_ast.h` — widen `int integer` to `int64_t integer` in literal union
- `src/backend/parser/cypher_gram.y` — use `strtoll` for integer literal parsing

## Status Updates

### 2026-03-27: Implementation complete

**Changes made across 10 files:**

1. **`src/include/parser/cypher_ast.h`** — widened `int integer` to `int64_t integer` in literal union, added `<stdint.h>`, updated `make_integer_literal` signature
2. **`src/include/parser/cypher_scanner.h`** — widened token value `int integer` to `int64_t integer`, added `<stdint.h>`
3. **`src/backend/parser/cypher_scanner.l`** — `strtol` → `strtoll` (3 places: hex, octal, decimal)
4. **`src/backend/parser/cypher_ast.c`** — `make_integer_literal(int)` → `make_integer_literal(int64_t)`, debug printf `%d` → `%lld`
5. **`src/backend/executor/cypher_schema.c`** — `sqlite3_bind_int(stmt, 3, *(const int*)value)` → `sqlite3_bind_int64(stmt, 3, *(const int64_t*)value)` (both node and edge paths)
6. **`src/backend/executor/executor_merge.c`** — `sqlite3_bind_int` → `sqlite3_bind_int64` for LITERAL_INTEGER bindings
7. **`src/backend/transform/transform_match.c`** — `%d` → `%lld` with `(long long)` cast (4 places)
8. **`src/backend/transform/transform_return.c`** — `%d` → `%lld` with cast
9. **`src/backend/transform/transform_unwind.c`** — `%d` → `%lld` with cast
10. **Generated files** — `cypher_scanner.c`, `cypher_gram.tab.h` updated to match

**Test results:**
- 921/921 C unit tests pass
- 331/342 Python tests pass (11 failures are other issue repro tests, expected)
- All 12 original functional test files pass
- `TestIssue43::test_large_integer_preserved` — PASSES (was failing)
- `TestIssue43::test_timestamp_not_truncated` — PASSES (was failing)
- Cypher functional: `9999999999` round-trips correctly, `timestamp()` returns 1.77 trillion

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