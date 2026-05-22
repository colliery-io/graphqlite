---
id: e2-relocate-finalize-sql
level: task
title: "E2: relocate finalize_sql_generation to end of transform_single_query_sql"
short_code: "GQLITE-T-0311"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T13:14:49.460013+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0313, GQLITE-T-0314, GQLITE-T-0315, GQLITE-T-0316, GQLITE-T-0317]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E2: relocate finalize_sql_generation to end of transform_single_query_sql

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

Move the two `finalize_sql_generation` call sites inside
`transform_return.c` (~line 483 and ~674) out of the transform layer
and into the end of `transform_single_query_sql`. Once handlers
(E4-E8) stop relying on mid-flow finalize, this becomes a single
deterministic finalize at the transform→execute boundary.

## Blocked by

E4-E8 (T-0313 through T-0317) must land first. A prior naive E2
attempt (2026-05-20) crashed TCK from 3422 → 854 because write-only
queries, CALL subqueries, MERGE pipelines, and UNION branches all
expected finalize to have run by the time their post-clause path
ran. Until those handlers are decoupled, this relocation must not
ship.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `finalize_sql_generation` removed from `transform_return.c`.
- [ ] Single explicit `finalize_sql_generation` call at the end of
  `transform_single_query_sql` (right before
  `prepend_cte_to_sql`).
- [ ] TCK pass count is ≥ start-of-task baseline.
- [ ] 944/944 unit, functional clean.

## Affected files

- `src/backend/transform/transform_return.c` — remove the two
  finalize calls
- `src/backend/transform/cypher_transform.c` —
  `transform_single_query_sql` gets the new finalize call

## Status

todo. Land E4-E8 first.