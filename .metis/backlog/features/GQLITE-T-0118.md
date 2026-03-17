---
id: no-parameterized-query-support-in
level: task
title: "No parameterized query support in high-level Graph API"
short_code: "GQLITE-T-0118"
created_at: 2026-03-17T01:30:35.766058+00:00
updated_at: 2026-03-17T02:35:11.788292+00:00
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

# No parameterized query support in high-level Graph API

## Objective

Add parameterized query convenience method directly on the `Graph` type so users don't need to go through `graph.connection().cypher_builder()`.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P3 - Low (when time permits)

### Business Justification
- **User Value**: Reduces boilerplate for the most common operation. Important for Cypher injection safety.
- **Effort Estimate**: S

## Current State

**Rust:** `Graph` already has `query_builder()` which returns `CypherQuery` — this IS the parameterized query support on Graph! The reporter's claim that you must go through `graph.connection().cypher_builder()` appears to be **outdated or incorrect**. `query_builder` was added and is equivalent.

```rust
// This works today:
g.query_builder("MATCH (n {id: $id}) RETURN n")
    .param("id", node_id)
    .run()?;
```

**Python:** `Graph.query(cypher, params=None)` already accepts an optional params dict — fully parameterized.

**What's actually missing:** A one-liner convenience like `query_params(cypher, params_map)` that doesn't require the builder pattern. Minor ergonomic improvement only.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Verify `query_builder` is documented / discoverable
- [ ] Optionally add `query_params(cypher: &str, params: &[(&str, impl Into<Value>)]) -> Result<CypherResult>` convenience
- [ ] Update README/docs if `query_builder` is undocumented

## Status Updates

### Implementation Complete
- **Rust**: Added `Graph::query_params(cypher, &[("key", &json_value)])` convenience method
- **Python**: Already had `Graph.query(cypher, params=dict)` — no changes needed
- **Verified**: `query_builder()` already existed on Graph (reporter's claim was outdated)
- **Tests**: 26 Rust tests pass (including new doctest)