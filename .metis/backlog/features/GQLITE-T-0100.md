---
id: capability-metadata-compatibility
level: task
title: "Capability metadata + compatibility mode"
short_code: "GQLITE-T-0100"
created_at: 2026-02-07T02:09:59.563003+00:00
updated_at: 2026-02-07T02:09:59.563003+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Capability metadata + compatibility mode

**GitHub Issue**: [#17](https://github.com/colliery-io/graphqlite/issues/17)

## Objective

Expose a `capabilities()` API that returns the Cypher dialect version, supported syntax flags, and JSON1 availability so clients can detect features at runtime instead of guessing.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement

### Priority
- [ ] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Clients can programmatically detect supported features and adapt behavior instead of brittle fallbacks
- **Business Value**: Enables forward-compatible client libraries; reduces support issues from version mismatches
- **Effort Estimate**: S - Lightweight metadata struct, no parser or execution changes

## Acceptance Criteria

- [ ] `capabilities()` API returns Cypher dialect version
- [ ] Response includes supported syntax flags (bracket access, list literals, map/list properties, backticks, JSON path)
- [ ] Response includes JSON1 availability
- [ ] Optional `enable_neo4j_compat` compatibility mode toggle
- [ ] Capability info is versioned and documented

## Implementation Notes

### Technical Approach
- Add a lightweight metadata struct to the Rust API
- Expose through C extension, Rust bindings, and Python bindings
- Default to current behavior for backward compatibility
- Version the capabilities schema for future extensibility

### Dependencies
- Should be implemented after GQLITE-T-0096 and GQLITE-T-0097 so the capability flags reflect actual supported features

### Risk Considerations
- Minimal risk — additive API with no behavioral changes
- Need to keep capability flags in sync as new features land

## Status Updates

*To be added during implementation*