---
id: graphstats-field-naming-and-types
level: task
title: "GraphStats field naming and types (nodes/edges -> node_count/edge_count as usize)"
short_code: "GQLITE-T-0117"
created_at: 2026-03-17T01:30:34.347273+00:00
updated_at: 2026-03-17T02:34:32.019505+00:00
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

# GraphStats field naming and types (nodes/edges -> node_count/edge_count as usize)

## Objective

Rename `GraphStats` fields from `nodes`/`edges` (i64) to `node_count`/`edge_count` (usize) to follow Rust conventions.

## Backlog Item Details

### Type
- [x] Tech Debt - Code improvement or refactoring

### Priority
- [x] P3 - Low (when time permits)

### Technical Debt Impact
- **Current Problems**: `i64` for counts forces `as usize` casts; `nodes`/`edges` names are ambiguous (could mean the items themselves)
- **Benefits of Fixing**: Idiomatic Rust API, no unnecessary casts
- **Risk Assessment**: Breaking change for Rust consumers — needs semver bump

## Current State

**Rust:** `GraphStats { pub nodes: i64, pub edges: i64 }` — values come from SQLite `COUNT(*)` which returns i64.

**Python:** `stats()` returns `dict` with `{'nodes': int, 'edges': int}` — Python doesn't have usize so naming is the only concern.

**Also affects:** `CacheStatus { pub nodes: Option<i64>, pub edges: Option<i64> }` — same naming/type issue.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `GraphStats` fields renamed to `node_count: usize` and `edge_count: usize`
- [ ] `CacheStatus` fields renamed similarly
- [ ] Python `stats()` dict keys updated to match (`node_count`, `edge_count`)
- [ ] Document as breaking change for next semver

## Status Updates

### Implementation Complete
- **Rust**: `GraphStats` fields renamed `nodes`→`node_count`, `edges`→`edge_count`
- **Rust**: `CacheStatus` fields renamed similarly, with `#[serde(alias = "nodes")]` for C extension JSON compat
- **Python**: `stats()` keys updated to `node_count`/`edge_count`
- **Python**: `load_graph`/`reload_graph` results remapped from C extension keys
- **Tests**: All integration tests updated, 25 Rust + 226 Python pass