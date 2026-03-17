---
id: leading-zero-strings-coerced-to
level: task
title: "Leading-zero strings coerced to integers in Cypher path"
short_code: "GQLITE-T-0121"
created_at: 2026-03-17T02:45:28.751626+00:00
updated_at: 2026-03-17T12:57:12.594512+00:00
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

# Leading-zero strings coerced to integers in Cypher path

## Objective

Fix string values with leading zeros (e.g., `"02134"`) being coerced to integers when passed through the Cypher path in `upsert_node`/`upsert_edge`.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P2 - Medium (nice to have)

### Impact Assessment
- **Affected Users**: Anyone storing zip codes, phone numbers, or other zero-prefixed string identifiers
- **Reproduction Steps**: 
  1. `g.upsert_node("n1", {"zipcode": "02134"}, "Place")`
  2. `g.get_node("n1")` — zipcode property is integer `2134`, not string `"02134"`
- **Expected vs Actual**: `"02134"` should stay as string. Becomes integer `2134`.

## Root Cause

**Python path**: `format_props()` in `utils.py` calls `_format_value()` which detects numeric-looking strings and passes them through unquoted. The C extension then parses `02134` as integer.

**Rust path** (now mitigated): The new `PropertyValue` type from GQLITE-T-0114 lets users explicitly pass `PropertyValue::Text("02134".into())`. But the `From<&str>` auto-detection still has this issue.

**Fix needed in Python**: `_format_value()` should NOT strip leading zeros — if a string starts with `0` and is longer than 1 char, it should be treated as text, not a number.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `upsert_node` with `{"zipcode": "02134"}` preserves string type
- [ ] Pure numeric strings like `"42"` still auto-detect as integer
- [ ] `"0"` still auto-detects as integer (single zero is valid)
- [ ] `"0.5"` still auto-detects as float
- [ ] Rust `From<&str> for PropertyValue` also fixed for leading-zero strings
- [ ] Tests added for edge cases

## Status Updates

### Implementation Complete
- **Root cause found**: `create_property_agtype_value()` in `executor_match.c` used `strtoll()` to parse `"02134"` as integer `2134` — no leading-zero check
- **C fix**: Added leading-zero check in `executor_match.c:create_property_agtype_value()` — strings starting with `0` followed by digits skip numeric parsing
- **Rust fix**: Added `has_leading_zero()` check in `utils.rs` for `format_value()` and `From<&str> for PropertyValue`
- **Python**: Already handled correctly — `format_props()` wraps all `str` values in quotes
- **Verified**: `"02134"` now returns as `"02134"` string, `42` still integer, `0.5` still float, `"0"` still integer, comparisons still work
- **Tests**: 849 unit, 226 Python, 213 Rust all pass