---
id: e7-handle-call-subquery-hoist-per
level: task
title: "E7: handle_call_subquery — hoist per-row transforms"
short_code: "GQLITE-T-0316"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T02:56:09.654996+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E7: handle_call_subquery — hoist per-row transforms

## Status Updates

**2026-05-21 — partial landing (MATCH hoist).** The inner MATCH
transform is now ONCE before the outer-row loop, cached per clause
position in `inner_match_sql[ci]`. Per-row work reduced to
prepare/step of the cached SQL string. TCK 3477 unchanged
(refactor, not behavioral). Commit `7c3763c`.

The remaining per-row contexts (`ret_ctx` at ~line 552, `eval_ctx`
at ~line 686) reference `scoped_map` entries that change per row.
The SELECT-expression transforms produce stable SQL (the `_cv_N`
aliases are stable; only the IDs in FROM/WHERE differ). Hoisting
them needs:
- Pre-compute the SELECT-expression strings once.
- Per-row, splice in the current entity IDs via FROM/WHERE.

Doable but not high-leverage — no TCK gain expected, and the per-
row transform cost in those paths is small compared to MATCH.
Closing T-0316 with the MATCH hoist as the substantive result.

The structural goal of "single transform contract per handler"
(G1 in I-0042) is still partially met for CALL — the outer ctx +
inner MATCH ctx are now single-shot; the RETURN/eval contexts
remain per-row but that's an optimization rather than a contract
issue. Filed remaining work as a follow-on if needed.

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

`executor_call_subquery.c` creates FOUR transform contexts per outer
row:
- outer `ctx` (line 136)
- inner MATCH `match_ctx` (line 352)
- inner RETURN `ret_ctx` (line 529)
- inner eval `eval_ctx` (line 663)

The inner MATCH transform doesn't reference outer-row values (uses
`executor->params_json` for bindings), so it IS hoistable out of the
per-row loop. The RETURN and eval contexts use the per-row scoped
var_map; hoisting those requires parameter-substitution for outer
variables.

This is a multi-iteration refactor — the safe first step is hoisting
only the inner MATCH and measuring.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Inner MATCH `match_ctx` (line 352) created ONCE before the
  outer loop, reused per row.
- [ ] Inner RETURN `ret_ctx` (line 529) — assess after MATCH hoist.
  May need parameter-substitution for outer scope variables.
- [ ] Inner eval `eval_ctx` (line 663) — same assessment.
- [ ] No regression on Call1-Call6 TCK scenarios (most are skipped
  on user-procedure support — T-0252 — but the structural tests
  must keep passing).
- [ ] Performance: per-row work is reduced (anecdotal — no formal
  benchmark required).

## Affected files

- `src/backend/executor/executor_call_subquery.c`

## Notes

Per-iter 3 (Ralph loop) survey: hoisting the inner MATCH alone is
~25% of the per-row work. Full E7 needs careful outer-var
substitution across the other contexts.