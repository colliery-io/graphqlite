---
id: e3-relocate-finalize-in-cypher
level: task
title: "E3: relocate finalize in cypher_transform_query + remove raw_output drain shim"
short_code: "GQLITE-T-0312"
created_at: 2026-05-22T00:12:47.008348+00:00
updated_at: 2026-05-22T13:19:41.222578+00:00
parent: GQLITE-I-0042
blocked_by: [GQLITE-T-0311]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# E3: relocate finalize in cypher_transform_query + remove raw_output drain shim

## Status Updates

**2026-05-22 — landed (TCK-neutral, 0 regressions).** Commit
`77599cd`.

Three coordinated changes that together remove the drain shim:

1. `sql_builder_to_string` gained a pure-DML emit path. When
   `select_count==0` AND `from` is empty BUT `raw_output` is non-
   empty, it returns the raw_output content directly (leading ws/
   `;` stripped). Previously returned NULL for write-only queries.

2. `transform_single_query_sql` + `cypher_transform_query` finalize
   gates widened to include `!dbuf_is_empty(&raw_output)`.
   Write-only queries now hit the new pure-DML emit path inside
   finalize, populating sql_buffer via the builder.

3. The explicit drain blocks in `cypher_transform_query` (~line
   730) and `cypher_transform_generate_sql` (~line 977) are
   deleted.

The T-0310 mixed-DML+SELECT split path (raw_output non-empty AND
sql_size > 0 from a SELECT body) is retained — that's still the
SET/REMOVE compound shape needing pre_exec_dml.

**I-0042 G3 met**: `unified_builder` is the sole assembly point;
`sql_buffer` is populated via `finalize_sql_generation` only.

TCK 3482 (unchanged). 944/944 unit, functional clean.

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

## Acceptance Criteria

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