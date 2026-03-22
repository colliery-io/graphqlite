---
id: investigate-clotho-integration
level: task
title: "Investigate Clotho integration: count(), OPTIONAL MATCH, property-match reported as broken"
short_code: "GQLITE-T-0140"
created_at: 2026-03-22T00:45:57.695444+00:00
updated_at: 2026-03-22T00:45:57.695444+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#bug"


exit_criteria_met: false
initiative_id: NULL
---

# Investigate Clotho integration: count(), OPTIONAL MATCH, property-match reported as broken

## Objective

Investigate why Clotho (colliery-io/clotho) reported `count()`, `OPTIONAL MATCH`, and `{key: value}` property-match as broken, when all three work correctly at the C extension level on both v0.3.7 and current main.

## Background

**Reported from Clotho integration sessions (v0.3.7, rusqlite 0.31).** The report claims 8 of 33 queries failed (24% error rate), including:

1. **`count()` aggregate** — "returns column headers but no rows." Cannot reproduce at C level on v0.3.7 or current; returns `[{"total":3}]` correctly.
2. **`{key: 'value'}` property-match** — "syntax error on `:`". Cannot reproduce at C level on v0.3.7 or current; parses and executes correctly.
3. **`OPTIONAL MATCH`** — "returns empty result set." Cannot reproduce at C level; correctly returns rows with `null` for unmatched optionals.

### Reproduction Results

Tested all three on v0.3.7 (via `git checkout v0.3.7 && make extension`) and current main:

| Bug | v0.3.7 | v0.3.10 (current) |
|-----|--------|-------------------|
| count() empty | Works correctly | Works correctly |
| {key: value} syntax error | Works correctly | Works correctly |
| OPTIONAL MATCH empty | Works correctly | Works correctly |

### Suspected Root Cause: Rust Bindings

Since the C extension works, the issue likely lives in the Rust binding layer or Clotho's usage of it. Analysis of `bindings/rust/src/` found several potential data-loss paths:

- **`connection.rs:131`** — If SQLite returns `NULL`, `query_row` gets `None` → `CypherResult::empty()` silently. Could happen if Clotho uses a different execution path.
- **`result.rs:424-425`** — Empty string from extension → silent empty result.
- **`result.rs:437-444`** — Non-JSON strings (e.g., malformed error messages) are silently wrapped as `{"result": "..."}` instead of raising an error.
- **`result.rs:450-451`** — Empty JSON arrays → silent empty result, no way to distinguish from "query returned no matches."

Clotho may also be using `conn.query_row` or `conn.execute` directly instead of the `cypher()` wrapper, which could bypass result parsing.

## Acceptance Criteria

- [ ] Reproduce the failures in the Rust integration test harness, or confirm they are Clotho-side
- [ ] If Rust binding issue: fix silent empty results — return errors instead of empty `CypherResult`
- [ ] If Clotho-side: document correct usage patterns and close
- [ ] Add Rust integration tests for count(), OPTIONAL MATCH with nulls, and property-match syntax

## Implementation Notes

### Investigation Steps

1. Write Rust integration tests that mirror the exact Clotho queries
2. Check if Clotho pins an older graphqlite-rs version that might have bugs since fixed
3. Check if Clotho uses `execute()` instead of `query_row()` for read queries (would discard results)
4. Review Clotho's `Cargo.lock` for graphqlite version

### Risk Considerations
- May require changes in the Clotho repo rather than graphqlite
- Silent empty results are a usability hazard regardless — worth hardening even if not the root cause

## Status Updates

### 2026-03-21: Rust binding path verified — not the cause

**Investigation complete.** Added 5 Rust integration tests mirroring exact Clotho query patterns (`test_clotho_bug1` through `test_clotho_bug5` + `test_clotho_pattern_predicate_in_where`). All pass through the full Rust binding path (connection → query_row → JSON parsing → CypherResult).

**Key findings:**
- `count()` with WHERE filter: works — returns `{"total":3}` correctly
- `{key: 'value'}` property-match: works — parses and executes correctly
- `OPTIONAL MATCH` with null: works — returns rows with `null` for unmatched optionals, `Option<String>` extracts correctly
- `(a)--(b)` undirected: works on current (was fixed in v0.3.8)
- Pattern predicates: works with updated extension binary

**Stale cache finding:** The bundled extension binary caching in `platform.rs` (versioned filename + size-only check) can serve stale binaries if the binary is rebuilt without a version bump. This caused a false-negative during testing. Not likely the Clotho root cause, but a potential footgun.

**Verdict:** The issue is Clotho-side, not in graphqlite or its Rust bindings. Recommend:
1. Check Clotho's Cargo.lock for pinned graphqlite version
2. Check if Clotho constructs queries with incorrect escaping
3. Check if Clotho uses rusqlite `execute()` for reads (would discard results)
4. Close this ticket once Clotho is investigated