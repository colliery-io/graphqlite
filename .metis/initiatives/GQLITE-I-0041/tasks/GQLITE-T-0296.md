---
id: c11-repeat-variant-collapse-for
level: task
title: "C11: Repeat variant collapse for execute_match_* (8 variants → canonical set)"
short_code: "GQLITE-T-0296"
created_at: 2026-05-19T14:49:38.711488+00:00
updated_at: 2026-05-19T14:49:38.711488+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C11: Repeat variant collapse for execute_match_* (8 variants → canonical set)

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

Same exercise as C8–C10 but for the 8 MATCH variants. Canonical signature pattern mirrors merge (single function with optional input/output var_map). This is the prerequisite for C12.

## Acceptance Criteria

- [ ] Canonical `execute_match_*` function set defined (likely one for read-only MATCH, one for MATCH-into-varmap).
- [ ] Every caller migrated.
- [ ] Old variants deleted.
- [ ] `bind_match_clause_into_varmap` supports ALL MATCH shapes including relationship-only patterns (currently a known gap).
- [ ] Build green, TCK delta zero.

## Status Updates

*To be added during implementation*
