---
id: s7-migrate-transform-match-c-to
level: task
title: "S7: Migrate transform_match.c to sql_builder (fixes var-length alias bugs + OPTIONAL MATCH joins, ~37 TCK)"
short_code: "GQLITE-T-0261"
created_at: 2026-05-19T14:44:46.455771+00:00
updated_at: 2026-05-19T14:44:46.455771+00:00
parent: GQLITE-I-0039
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0039
---

# S7: Migrate transform_match.c to sql_builder (fixes var-length alias bugs + OPTIONAL MATCH joins, ~37 TCK)

## Parent Initiative

[[GQLITE-I-0039]]

## Objective

Hot path. `transform_match.c` is where the variable-length pattern SQL alias collisions (`ambiguous column name: n_3.id`, `no such column: _gql_default_alias_2.id`) live, and where the OPTIONAL MATCH JOIN-ON construction loses bound-var context. Migrating to `sql_builder` forces the alias-management and JOIN-ON code to use the builder's clause sections, which automatically resolves both bug classes. Net TCK gain estimated ~37 scenarios.

## Acceptance Criteria

- [ ] No deprecated-API warnings in transform_match.c.
- [ ] Var-length pattern TCK scenarios that errored with "no such column" / "ambiguous column name" pre-migration now pass (Match4 [1]/[5]/[7]; Match5 [19]/[21]/[23]/[28]/[29]; Match6 [15]; Match9 [1]–[5]/[9]).
- [ ] OPTIONAL MATCH bound-var scenarios that returned wrong row counts pre-migration now pass (Match7 [3]/[4]/[8]/[9]/[14]/[15]/[19]).
- [ ] `angreal test unit && angreal test functional` clean.
- [ ] TCK delta strictly positive (no regressions).

## Status Updates

*To be added during implementation*
