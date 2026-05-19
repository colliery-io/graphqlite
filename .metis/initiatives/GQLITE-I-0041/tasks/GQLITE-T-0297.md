---
id: c12-match-where-set-return-pre-set
level: task
title: "C12: MATCH+WHERE+SET+RETURN — pre-SET var_map capture (unblocks ~10 TCK; depends on C11)"
short_code: "GQLITE-T-0297"
created_at: 2026-05-19T14:49:52.790081+00:00
updated_at: 2026-05-19T14:49:52.790081+00:00
parent: GQLITE-I-0041
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0041
---

# C12: MATCH+WHERE+SET+RETURN — pre-SET var_map capture (unblocks ~10 TCK; depends on C11)

## Parent Initiative

[[GQLITE-I-0041]]

## Objective

TCK scenarios Set1 [1]/[2], Set2 [2], Set6 [5]/[6]/[7]/[19]/[20]/[21] (and similar Remove6) currently return zero rows because we re-run MATCH+RETURN after SET — the post-SET re-match re-applies the WHERE against the just-updated property and drops the row. Fix: in `handle_match_set`, capture the var_map at the MATCH+WHERE stage (pre-SET), apply SET, then project RETURN from the captured var_map via `project_return_row_from_var_map` (which I-0040 M3 splits out). Depends on C11 because `bind_match_clause_into_varmap` must support every MATCH shape — currently fails for relationship-only patterns, which was why an earlier attempt regressed -23 scenarios.

## Acceptance Criteria

- [ ] C11 (canonical execute_match_*) completed.
- [ ] `handle_match_set` switched to the var_map projection path when a RETURN follows.
- [ ] Set1 [1]/[2], Set2 [2], Set6 [5]/[7]/[19]/[21] now pass.
- [ ] No regressions on relationship-only MATCH+SET (currently fragile).

## Status Updates

*To be added during implementation*
