---
id: lists-maps-returned-as-escaped
level: task
title: "Lists/maps returned as escaped strings in RETURN n output"
short_code: "GQLITE-T-0111"
created_at: 2026-02-09T21:44:31.979701+00:00
updated_at: 2026-03-17T13:57:15.612384+00:00
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

# Lists/maps returned as escaped strings in RETURN n output

**GitHub Issue**: #22 (reported by @kynx)

## Objective

Fix JSON property values (lists, maps) being returned as double-escaped strings instead of proper JSON when using `RETURN n` (whole node return) or the `properties()` function.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P1 - High (important for user experience)

### Impact Assessment
- **Affected Users**: All users storing list or map properties
- **Reproduction Steps**:
  1. `SELECT cypher('CREATE (n:Person {id: "alice", pills: ["red", "blue"]}) RETURN n');`
  2. `SELECT cypher('MATCH (n {id: "alice"}) RETURN n');`
- **Expected vs Actual**:
  - **Expected**: `"pills": ["red","blue"]` (proper JSON array)
  - **Actual**: `"pills": "[\"red\",\"blue\"]"` (double-escaped string)
- **Note**: `RETURN n.pills` works correctly (returns raw JSON). Only `RETURN n` (whole node) is affected.

## Root Cause Analysis

When `RETURN n` builds the properties JSON via `json_group_object(pk.key, COALESCE(...))`, the value from `node_props_json` is returned as TEXT. SQLite's `json_group_object()` treats TEXT values as strings, wrapping them in quotes and escaping internal quotes. The JSON value needs to be wrapped with `json()` so SQLite recognizes it as raw JSON, not a string to be escaped.

**Primary bug** — Missing `json()` wrapper in all `json_group_object` COALESCE chains:

| File | Lines | Context |
|------|-------|---------|
| `transform_return.c` | 570 | WITH-passed node `RETURN n` |
| `transform_return.c` | 617 | Direct node variable `RETURN n` |
| `transform_return.c` | 595 | Edge variable `RETURN r` |
| `transform_return.c` | 898 | Map projection `n{.*}` |
| `transform_return.c` | 944 | Map projection individual property |
| `transform_func_entity.c` | 168 | `properties()` for edges |
| `transform_func_entity.c` | 185 | `properties()` for nodes |

**Secondary bug** — `agtype.c` missing `node_props_json`/`edge_props_json` entirely:

| Function | Line | Issue |
|----------|------|-------|
| `load_node_properties()` | 666-716 | Count + load queries omit `node_props_json` |
| `load_edge_properties()` | 765-815 | Count + load queries omit `edge_props_json` |

The AGType functions also lack a `'json'` type handler — they would need to parse JSON values into proper agtype structures instead of falling through to `agtype_value_create_string()`.

## Fix Approach

For the primary bug, change each JSON property subquery from:
```sql
(SELECT npj.value FROM node_props_json npj WHERE ...)
```
to:
```sql
(SELECT json(npj.value) FROM node_props_json npj WHERE ...)
```

For the AGType secondary bug, add `node_props_json`/`edge_props_json` UNION ALL arms to both `load_node_properties()` and `load_edge_properties()`, including a `'json'` type handler that parses the value as raw JSON.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `RETURN n` with list properties shows proper JSON arrays, not escaped strings
- [ ] `RETURN n` with map properties shows proper JSON objects, not escaped strings
- [ ] `RETURN r` with list/map properties on edges works correctly
- [ ] `properties()` function returns proper JSON for list/map values
- [ ] Map projection `n{.*}` returns proper JSON for list/map values
- [ ] `load_node_properties()` in agtype.c includes `node_props_json`
- [ ] `load_edge_properties()` in agtype.c includes `edge_props_json`
- [ ] Functional tests cover `RETURN n` with JSON properties (list AND map)
- [ ] All existing tests continue to pass

## Test-First Plan

Write failing tests first, then fix code until they pass.

### Functional Tests (add to `tests/functional/33_json_properties.sql`)

**TC-1: RETURN node with list property**
```sql
SELECT cypher('CREATE (n:ReturnTest {name: "tc1", tags: [1, 2, 3]}) RETURN n') as result;
-- Verify: "tags":[1,2,3]  (not "tags":"[1,2,3]")
```

**TC-2: RETURN node with string list property**
```sql
SELECT cypher('CREATE (n:ReturnTest {name: "tc2", colors: ["red", "blue"]}) RETURN n') as result;
-- Verify: "colors":["red","blue"]  (not "colors":"[\"red\",\"blue\"]")
```

**TC-3: RETURN node with map property**
```sql
SELECT cypher('CREATE (n:ReturnTest {name: "tc3", meta: {role: "admin", level: 5}}) RETURN n') as result;
-- Verify: "meta":{"role":"admin","level":5}  (not escaped string)
```

**TC-4: RETURN node with mixed scalar + JSON properties**
```sql
SELECT cypher('CREATE (n:ReturnTest {name: "tc4", age: 30, tags: ["a", "b"]}) RETURN n') as result;
-- Verify: "name":"tc4", "age":30, "tags":["a","b"] — scalars unaffected, JSON correct
```

**TC-5: RETURN edge with list property**
```sql
SELECT cypher('CREATE (a:ReturnTest {name: "tc5a"})-[r:TAGGED {labels: ["x", "y"]}]->(b:ReturnTest {name: "tc5b"}) RETURN r') as result;
-- Verify: "labels":["x","y"]  (not escaped string)
```

**TC-6: properties() function with JSON values**
```sql
SELECT cypher('MATCH (n:ReturnTest {name: "tc1"}) RETURN properties(n)') as result;
-- Verify: "tags":[1,2,3]  (not escaped)
```

**TC-7: Map projection {.*} with JSON values**
```sql
SELECT cypher('MATCH (n:ReturnTest {name: "tc2"}) RETURN n{.*}') as result;
-- Verify: "colors":["red","blue"]  (not escaped)
```

**TC-8: MATCH then RETURN node (not inline CREATE RETURN)**
```sql
SELECT cypher('MATCH (n:ReturnTest {name: "tc3"}) RETURN n') as result;
-- Verify: "meta":{"role":"admin","level":5} — tests the WITH-passed node path
```

**TC-9: Nested JSON — list of maps**
```sql
SELECT cypher('CREATE (n:ReturnTest {name: "tc9", entries: [{k: "a", v: 1}, {k: "b", v: 2}]}) RETURN n') as result;
-- Verify: "entries":[{"k":"a","v":1},{"k":"b","v":2}]
```

### Test Execution Order

1. Add all tests above as new section in `33_json_properties.sql`
2. Build extension, run `angreal test functional` — confirm TC-1 through TC-9 fail
3. Apply `json()` wrapper fix to transform files (7 locations)
4. Rebuild, rerun — confirm TC-1 through TC-9 pass
5. Apply agtype.c fix (2 functions)
6. Rebuild, run `angreal test unit` + `angreal test functional` — all green

## Implementation Notes

### Affected Files
- `src/backend/transform/transform_return.c` — 5 locations
- `src/backend/transform/transform_func_entity.c` — 2 locations
- `src/backend/executor/agtype.c` — 2 functions
- `tests/functional/33_json_properties.sql` — add RETURN n tests

### Risk Considerations
- The `json()` wrapper is safe — if the value is already valid JSON (which it is, enforced by `json_valid()` CHECK constraint on `node_props_json`), `json()` simply passes it through as a JSON value rather than a string.
- Need to verify that scalar-only nodes are unaffected (COALESCE should never reach the JSON arm for those).

## Status Updates

### Session 1 — Fix Implementation

**Tests written** (Section 12 in `tests/functional/33_json_properties.sql`):
- TC-12.1 through 12.9 covering RETURN n/r, properties(), map projection, edges, nested JSON

**Root cause deeper than initially analyzed** — three layers of bugs:

1. **SQL generation** (`transform_return.c`, `transform_func_entity.c`): `json_group_object` received JSON property values as TEXT, treating them as strings. Fixed by wrapping with `json()` — 8 locations total (6 in transform_return.c, 2 in transform_func_entity.c).

2. **AGType JSON parsing** (`agtype.c` `agtype_value_from_vertex_json`/`agtype_value_from_edge_json`): Used `sqlite3_column_type()` which returns SQLITE_TEXT for both strings and JSON arrays/objects. Fixed by querying `json_each`'s `type` column to detect "array"/"object" types. Also improved boolean handling.

3. **AGType property loading** (`agtype.c` `load_node_properties`/`load_edge_properties`): Completely missing `node_props_json`/`edge_props_json` UNION ALL arms. Added them with a new `AGTV_JSON` type that serializes without quoting.

4. **Map projection bool bug** (`transform_return.c` line 897): `CASE WHEN npb.value THEN 'true' ELSE 'false' END` returned `'false'` for NULL (no bool property), masking JSON values in COALESCE. Fixed to `CASE npb.value WHEN 1 THEN 'true' WHEN 0 THEN 'false' END` which returns NULL properly.

**New type added**: `AGTV_JSON` in agtype.h — stores raw JSON string, serialized without quoting. Used by `agtype_value_create_json()`.

**Files modified**:
- `src/backend/transform/transform_return.c` — 7 `json()` wrappers + bool CASE fix
- `src/backend/transform/transform_func_entity.c` — 2 `json()` wrappers
- `src/backend/executor/agtype.c` — new `agtype_value_create_json`, AGTV_JSON serializer/free, fixed `from_vertex_json`/`from_edge_json`, fixed `load_node_properties`/`load_edge_properties`
- `src/include/executor/agtype.h` — AGTV_JSON enum, `agtype_value_create_json` declaration
- `tests/functional/33_json_properties.sql` — 9 new tests in Section 12

**Test results**: 849 unit tests pass, all functional tests pass, GH issue #22 example verified.

### Session 2 — Verification

Verified task is fully complete. Commit `ff7e68a` ("fix: return JSON properties as proper arrays/objects in RETURN n (#22)") is on main. Working tree clean.

Re-ran full test suite to confirm:
- **Unit tests**: 849/849 pass
- **Functional tests**: All pass, no failures

All acceptance criteria met:
- [x] `RETURN n` with list properties → proper JSON arrays
- [x] `RETURN n` with map properties → proper JSON objects
- [x] `RETURN r` with list/map edge properties → correct
- [x] `properties()` function → proper JSON
- [x] Map projection `n{.*}` → proper JSON
- [x] `load_node_properties()` includes `node_props_json`
- [x] `load_edge_properties()` includes `edge_props_json`
- [x] Functional tests cover all cases (Section 12, TC-12.1 through 12.9)
- [x] All existing tests continue to pass