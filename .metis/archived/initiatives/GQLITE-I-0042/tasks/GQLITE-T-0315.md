---
id: e6-handle-match-remove-two-pass
level: task
title: "E6: handle_match_remove two-pass refactor"
short_code: "GQLITE-T-0315"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T12:44:42.490325+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0310]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E6: handle_match_remove two-pass refactor

## Status Updates

**2026-05-22 — landed (+3 TCK).** Property-REMOVE was already
handled by the synth-match-without-WHERE pattern from earlier
iterations. Label-REMOVE needed a separate fix: when REMOVE strips
labels, the post-REMOVE re-MATCH of `(n:LABEL)` finds 0 rows
(label is gone).

Fix (commit d04910a): mutate-then-restore the synth pattern.
Walk the REMOVE items; for each label item, find the matching
node pattern in match->pattern and strip the label from its
labels list. Run synth-match (now label-less). Restore the
stripped labels in reverse order so the AST is unchanged.

The variable binding is preserved (re-match by structure WITHOUT
the removed label finds the same nodes); RETURN reads CURRENT
post-REMOVE state for `labels(n)` and untouched properties alike.

TCK 3479 → 3482 (+3):
  + Remove3 [10]  Skipping and limiting after REMOVE label
  + Remove3 [11]  Skipping/limiting after REMOVE label
  + Remove3 [13]  Aggregating in RETURN after REMOVE label

Only one Remove test still fails: Remove1 [6] — OPTIONAL MATCH +
REMOVE r.num where r is unbound (null). That's a different code
path (OPTIONAL MATCH null handling for REMOVE), out of T-0315
scope.

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

Same shape as E5 (T-0314) but for REMOVE. Pre-REMOVE MATCH captures
the var_map; REMOVE strips labels/properties via
`execute_remove_operations`; post-REMOVE RETURN projects from the
captured ids.

Synth-match-without-WHERE post-REMOVE RETURN already landed (commit
fe3db6d, iteration 30) and parallels the SET light fix.

## Blocked by

T-0310 — same DML/SELECT split architecture issue as T-0314.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] T-0310 landed.
- [ ] All Remove tests in TCK still pass (Remove1 through Remove3,
  Remove4 if applicable).
- [ ] Multi-row REMOVE+RETURN scenarios behave correctly (same
  category as Set6).

## Affected files

- `src/backend/executor/query_dispatch.c` — handle_match_remove
- `src/backend/transform/transform_remove.c` — emission goes through
  the new DML buffer
- `src/backend/executor/executor_remove.c` — already has
  `execute_remove_operations` with the canonical shape