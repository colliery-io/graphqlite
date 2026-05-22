---
id: e3-relocate-finalize-in-cypher
level: task
title: "E3: relocate finalize in cypher_transform_query + remove raw_output drain shim"
short_code: "GQLITE-T-0312"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T13:14:58.342621+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0311]
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E3: relocate finalize in cypher_transform_query + remove raw_output drain shim

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

Same as E2 (T-0311) but for `cypher_transform_query`. Additionally,
DELETE the `drain raw_output → sql_buffer` shim added during the
I-0039 S5+S6 work. After E2 lands and the builder split (T-0310)
gives raw_output a proper home, the shim is dead code.

## Blocked by

T-0311 (E2) — same architectural prerequisites.

## Acceptance Criteria

## Acceptance Criteria

- [ ] `finalize_sql_generation` call relocated to a single explicit
  site in `cypher_transform_query`.
- [ ] The `drain raw_output → sql_buffer` block at
  `cypher_transform.c` lines ~730-739 and ~977-986 deleted.
- [ ] `unified_builder` is the sole assembly point; `sql_buffer` is
  populated exactly once.
- [ ] TCK pass count ≥ start-of-task baseline.
- [ ] 944/944 unit, functional clean.

## Affected files

- `src/backend/transform/cypher_transform.c`

## Status

todo. Land E2 (T-0311) first.