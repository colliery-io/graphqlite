---
id: tck-09-type-coercion-and-agtype
level: task
title: "TCK-09: Type coercion and agtype boundary audit"
short_code: "GQLITE-T-0214"
created_at: 2026-05-13T12:51:09.018955+00:00
updated_at: 2026-05-13T12:51:09.018955+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-09: Type coercion and agtype boundary audit

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Audit how GraphQLite coerces between Cypher types (integer / float / string / boolean / list / map / node / relationship / path) and how values cross the agtype boundary in stored properties — an area where TCK coverage is uneven and project-specific risk is high (separate property tables per type).

## Acceptance Criteria

- [ ] Probes cover: integer/float mixing in arithmetic and comparison; string<->numeric coercion (spec: no implicit); list element heterogeneity; map round-trip through property storage; node/relationship identity equality; explicit `toString` / `toInteger` / `toFloat` / `toBoolean` edge cases (null inputs, overflow, malformed strings).
- [ ] Probes cover property storage round-trips: writing each agtype, reading back, asserting type and value preservation across the typed property tables (`node_props_text` / `_int` / `_real` / `_bool`).
- [ ] Audit report at `docs/tck/audit-types-<date>.md` records probe / expected / actual / pass-fail and links bugs.
- [ ] Bugs filed with reproductions for every deviation.

## Implementation Notes

### Technical Approach
Pay special attention to JSON-as-text properties accessed via `json_extract` — boundary between SQLite's type system and Cypher's is a known risk area.

### Dependencies
Independent of TCK-06/07; can run in parallel after [[GQLITE-T-0210]].

## Status Updates

*To be added during implementation*
