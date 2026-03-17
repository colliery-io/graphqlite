---
id: get-node-returns-opaque-value
level: task
title: "get_node returns opaque Value without property accessors"
short_code: "GQLITE-T-0116"
created_at: 2026-03-17T01:30:32.987839+00:00
updated_at: 2026-03-17T02:22:47.456768+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# get_node returns opaque Value without property accessors

## Objective

Improve the `Value` enum in Rust bindings to support property access on `Object` variants, so `get_node` results are ergonomic to work with.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P3 - Low (when time permits)

### Business Justification
- **User Value**: `get_node` returns `Option<Value>` where `Value::Object(HashMap)` requires pattern matching to extract fields. A `.get("field")` method would be much more ergonomic.
- **Effort Estimate**: S

## Current State

**Rust `Value` enum has:**
- `is_null()`, `as_bool()`, `as_i64()`, `as_f64()`, `as_str()` — primitive accessors
- No `.get(key)` for `Object` variant
- No `.index(i)` for `Array` variant
- Callers must `match` on `Value::Object(map)` then access the `HashMap`

**Python:** Returns `dict` directly from `get_node` — already ergonomic with `node["properties"]["title"]`.

**Note:** The Rust `Row` type already has `get_value(column)` and typed `get::<T>(column)`. The gap is only on `Value` itself when working with nested objects.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `Value::get(&self, key: &str) -> Option<&Value>` for Object variant (returns None for non-Object)
- [ ] `Value::index(&self, i: usize) -> Option<&Value>` for Array variant
- [ ] Implement `std::ops::Index<&str>` for Value (panics on missing key, like serde_json)
- [ ] Tests for accessor methods

## Status Updates

### Implementation Complete
- Added `Value::get(key)` for Object field access
- Added `Value::get_index(i)` for Array element access
- Added `Value::as_array()`, `Value::as_object()` for variant access
- Implemented `Index<&str>` for Value (panics on missing key, like serde_json)
- All 25 Rust tests pass