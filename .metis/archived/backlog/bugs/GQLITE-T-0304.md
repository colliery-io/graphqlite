---
id: booleans-inside-json-list-literals
level: task
title: "Booleans inside JSON list literals render as integers (json_array drops subtype)"
short_code: "GQLITE-T-0304"
created_at: 2026-05-20T16:16:14.406957+00:00
updated_at: 2026-05-20T20:08:06.283115+00:00
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

# Booleans inside JSON list literals render as integers (json_array drops subtype)

## Reproducer

```sql
.load build/graphqlite.dylib
SELECT cypher('RETURN [true, false, true] AS l');
-- Got:      [{"l":[1,0,1]}]
-- Expected: [{"l":[true,false,true]}]

SELECT cypher('RETURN [1]+(2 IN [3])+4 AS c');
-- Got:      [{"c":[1,0,4]}]
-- Expected: [{"c":[1,false,4]}]
```

Map literals likely have the analogous bug (`json_object` drops
element subtype).

## Context — surfaced 2026-05-19

Discovered while landing M13 (boolean JSON via subtype, T-0298). M13
fixed top-level boolean rendering — `RETURN 1=1` now emits JSON
`true` via the `GQL_SUBTYPE_BOOLEAN` tag. But when a boolean lives
inside a JSON array, SQLite's `json_array(...)` serializes the
argument by SQLite's storage type (int 0/1 → JSON number), losing
the subtype tag.

## Root cause

`transform_return.c:1113` (AST_NODE_LIST case in `transform_expression`)
emits `json_array(...)`. Boolean literals (LITERAL_BOOLEAN, line 1088)
emit as `1`/`0` integers. json_array sees integers, emits JSON
numbers. Same for runtime boolean expressions (e.g. `2 IN [3]`) — the
int+subtype reaches json_array but subtype is dropped at the
SQLite-function boundary.

## Fix sketch — Option B (custom list UDF, recommended)

Add `_gql_list(...)` UDF that constructs a JSON array honoring the
per-arg `sqlite3_value_subtype`. When subtype == GQL_SUBTYPE_BOOLEAN,
emit JSON `true`/`false`; otherwise route through normal type-based
formatting.

```c
void gql_list_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    dynamic_buffer out; dbuf_init(&out);
    dbuf_append(&out, "[");
    for (int i = 0; i < argc; i++) {
        if (i > 0) dbuf_append(&out, ",");
        if (sqlite3_value_subtype(argv[i]) == GQL_SUBTYPE_BOOLEAN) {
            const char *t = (const char*)sqlite3_value_text(argv[i]);
            dbuf_append(&out, t && !strcmp(t, "true") ? "true" : "false");
        } else {
            /* route by SQLite type */
        }
    }
    dbuf_append(&out, "]");
    sqlite3_result_text(ctx, dbuf_get(&out), -1, SQLITE_TRANSIENT);
    dbuf_free(&out);
}
```

Then `transform_return.c` AST_NODE_LIST emits `_gql_list(...)`
instead of `json_array(...)`. Same change in `transform_foreach.c`
and `transform_func_path.c` (path JSON arrays). Add `_gql_map(...)`
for the analogous map-literal fix.

Option A (less complete): emit boolean LITERALS as `json('true')` /
`json('false')`. Works for literals but doesn't help runtime boolean
expressions inside lists.

## Impact

- `Precedence3 [4]/[5]` (list+comparison precedence tests)
- Any TCK scenario embedding booleans in lists/maps
- Probably ~8-12 scenarios when audited

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `RETURN [true, false, true]` → `[true,false,true]`
- [x] `RETURN [1, 1=1, 3]` → `[1,true,3]`
- [x] `RETURN [1, 2 IN [3], 4]` → `[1,false,4]`
- [x] `RETURN {x: true, y: false, z: 1, w: 2=2}` → `{"x":true,"y":false,"z":1,"w":true}`
- [x] Nested lists: `[[1,2],[3,4]]` preserved (JSON subtype propagation)
- [x] Strings, nulls, ints, floats, whole-floats all unchanged
- [x] `[NOT true, NOT false]` → `[false,true]`
- [x] All 937 unit tests + functional suite pass

## Status Updates

**2026-05-20** — Completed.

**Three landed changes:**

1. New UDFs `_gql_list` / `_gql_map` in `runtime/udf_helpers.c`. Build a JSON array/object manually using `dynamic_buffer`. Per-arg behavior:
   - `GQL_SUBTYPE_BOOLEAN` → emit `true`/`false`
   - INTEGER → `%lld`; FLOAT → `%.17g` plus `.0` if no decimal point
   - TEXT — if it looks like JSON (starts with `[`/`{` after whitespace, or carries `GQL_SUBTYPE_JSON` subtype 0x4A), embed raw; otherwise JSON-encode as a string (escape `\"`, `\\`, control chars).
   - Both UDFs tag their output with `GQL_SUBTYPE_JSON` so nested `_gql_list(_gql_list(...))` chains embed instead of quoting.
   - Registered with `SQLITE_RESULT_SUBTYPE` flag and nargs = -1.

2. `transform_return.c` AST_NODE_LIST / AST_NODE_MAP now emit `_gql_list(...)` / `_gql_map(...)` instead of `json_array(...)` / `json_object(...)`. List item / map value transformation goes through new `transform_list_item_value()` helper that wraps boolean-yielding expressions (comparisons, AND/OR/XOR, NOT, IN, STARTS WITH, ...) in `_gql_bool_str(CASE WHEN ... THEN 1 ELSE 0 END)` so the subtype reaches the JSON constructor.

3. `LITERAL_BOOLEAN` in `transform_expression` now emits `_gql_bool_str(0/1)` instead of bare `0`/`1`, ensuring boolean literals carry the subtype wherever they appear in expressions (not just inside list/map literals).

**Carry-over benefit:** boolean expressions and literals now flow with their subtype intact through every reduction. Top-level boolean items still wrap (line 280's existing logic), but the inner wrap means nested booleans in lists/maps/function args also render correctly.

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