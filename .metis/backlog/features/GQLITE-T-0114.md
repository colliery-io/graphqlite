---
id: property-values-are-string-only-in
level: task
title: "Property values are string-only in upsert API"
short_code: "GQLITE-T-0114"
created_at: 2026-03-17T01:30:30.464617+00:00
updated_at: 2026-03-17T02:20:29.293351+00:00
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

# Property values are string-only in upsert API

## Objective

Accept typed `Value` variants (not just strings) in `upsert_node` and `upsert_edge` APIs across both Rust and Python bindings.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Numeric properties stored as strings means Cypher comparisons (`WHERE n.confidence > 0.8`) may silently fail or require casting. Forces consumers to handle string conversion at both write and read time.
- **Effort Estimate**: M

## Current State

**Rust:** `upsert_node` and `upsert_edge` accept `IntoIterator<Item = (K, V)>` where both `K: AsRef<str>` and `V: AsRef<str>`. The internal `format_value()` utility auto-types strings (detects integers, floats, booleans), but the API signature forces callers to pass everything as strings.

**Python:** `upsert_node(node_id, node_data: dict, label)` accepts a dict — Python's dynamic typing means values can be int/float/bool/str already. The Python binding may already handle this better than Rust.

**Note:** The auto-typing in `format_value()` means `("confidence", "0.87")` does get stored as a real. The issue is API ergonomics, not data loss. But edge cases exist (e.g., zip codes like "02134" would be stored as integer 2134).

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Rust `upsert_node`/`upsert_edge` accept `Value` enum or `impl Into<Value>` for property values
- [ ] Backward compatible — string pairs still work
- [ ] Python API unchanged (already accepts typed dicts)
- [ ] Bulk insert APIs also accept typed values

## Implementation Notes

### Technical Approach
Add a `PropertyValue` trait or use `Into<Value>` bound on the V generic parameter. Keep `AsRef<str>` as a blanket impl for backward compatibility.

## Status Updates

### Implementation Complete
- **New type**: `PropertyValue` enum with `Text`, `Integer`, `Float`, `Bool` variants in `utils.rs`
- **From impls**: `&str`, `String`, `i64`, `i32`, `f64`, `f32`, `bool`, `usize` all convert to `PropertyValue`
- **API change**: `V: AsRef<str>` → `V: Into<PropertyValue>` across `upsert_node`, `upsert_edge`, batch, and bulk APIs
- **Backward compatible**: `("key", "value")` pairs still work — `&str` auto-detects type via `From<&str>`
- **New usage**: `("score", PropertyValue::Float(0.87))` or `("count", 42_i64.into())`
- **Bulk insert**: Now uses `PropertyValue` variant to directly choose typed table (no string parsing)
- **Exported**: `PropertyValue` added to public API in `lib.rs`
- **Tests**: All 25 Rust tests pass