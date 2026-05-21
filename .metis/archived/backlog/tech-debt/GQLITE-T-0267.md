---
id: s13-add-typed-cte-prepend-section
level: task
title: "S13: Add typed CTE-prepend section to sql_builder API"
short_code: "GQLITE-T-0267"
created_at: 2026-05-19T14:45:37.924593+00:00
updated_at: 2026-05-20T22:57:32.834619+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# S13: Add typed CTE-prepend section to sql_builder API

## Parent Initiative

GQLITE-I-0039 (archived — moved to backlog 2026-05-20)

## Objective

Add a typed "pending CTE / prepend" clause section to `sql_builder` so `pending_prop_joins` has a target API to migrate into. Section needs the same prepend semantics as today's manual splicing — placed BEFORE any user-written CTEs, then the existing CTE section, then SELECT.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `sql_builder` has a new `pre_cte` typed buffer + `pre_cte_count`/`pre_cte_recursive` flags
- [x] `sql_pre_cte(b, name, query, recursive)` API documented in sql_builder.h
- [x] `sql_builder_to_string` still excludes both CTE buffers (emission owned by prepend_cte_to_sql for now)
- [x] `prepend_cte_to_sql` updated to fuse pre_cte (before) + user-cte (after) into a single WITH clause; detects RECURSIVE on either side
- [x] 5 new unit tests in tests/test_sql_builder.c (single, multiple, recursive flag, reset, to_string-excludes)
- [x] Existing callers unaffected — 942/942 unit tests pass, functional clean, TCK pass=3389 unchanged

## Status Updates

**2026-05-20** — Completed.

Implementation in `src/include/transform/sql_builder.h` + `src/backend/transform/sql_builder.c`:
- New `dynamic_buffer pre_cte` field, holds raw `name AS (query)` fragments separated by ", " — no leading WITH (assembled at emission time).
- `pre_cte_count` and `pre_cte_recursive` companion fields; init/reset/free wired through.
- `sql_pre_cte()` appends a fragment, comma-separates subsequent additions, tracks recursive flag.

`prepend_cte_to_sql` in `cypher_transform.c` rewritten to fuse both buffers:
- Detects "WITH RECURSIVE " or "WITH " prefix in the user CTE buffer and strips it.
- Emits a unified header (`WITH` / `WITH RECURSIVE`), then pre_cte fragments, then user fragments joined with ", ".
- Memory-safe — single allocation, prefix freed via dbuf_free.

T-0268 will migrate the `pending_prop_joins` writer in `transform_func_aggregate.c` (and the readers in transform_return.c et al.) to use `sql_pre_cte` instead of the raw splicing buffer.

## Status Updates

### 2026-05-20 — Still valid, prerequisite for S14-S15

This task is independent of the I-0042/I-0043 work and can be done
standalone. It adds infrastructure for the eventual
`pending_prop_joins` migration without requiring expression-tree
changes.

Suggested when picked up:
- Add a `dynamic_buffer pre_cte;` (or similar) section to
  sql_builder
- Update sql_builder_to_string to emit pre_cte before cte before
  select
- Add `sql_pre_cte(b, name, query)` API to write into it
- Unit tests in test_sql_builder.c

Then S14 can migrate add_pending_prop_join callers to use it.