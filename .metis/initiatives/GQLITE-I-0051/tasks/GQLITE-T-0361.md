---
id: f5-return-n-materialisation-entity
level: task
title: "F5: RETURN n materialisation — entity JSON pass-through and typed-table property object"
short_code: "GQLITE-T-0361"
created_at: 2026-09-07T01:25:08.446195+00:00
updated_at: 2026-09-07T02:22:40.488875+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F5: RETURN n materialisation — entity JSON pass-through and typed-table property object

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 5: `RETURN n` costs ~35 us and ~300 mallocs per node (757 ms at 20K). The SQL scans all of `property_keys` per node and probes ten indexes per key; the executor then parses the JSON back into an agtype tree and serialises it twice.

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

1. Pass entity JSON through verbatim (extension.c already has a verbatim path for `{`/`[` values).
2. Build the property object from the five typed tables by `node_id` joined to `property_keys` (measured 104 ms vs 182 ms for the SQL alone); consolidate the template, currently copied at eight sites (`transform_return.c:313`, `executor_result_project.c:455`, ...).
3. Size the output buffer once and serialise once.

Expected ~757 ms -> ~120 ms at 20K nodes; also shrinks finding 10's memory amplification.

Source: the review findings section of [[GQLITE-I-0051]] (originally PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).- 2026-09-06: implemented. New `src/include/entity_json_sql.h` is the single definition of node/edge JSON SQL (typed-table UNION ALL by entity id, json() re-applied at the aggregate because subtypes do not survive the compound, ORDER BY key_id keeps key order); all 14 template copies replaced (transform_return.c x9, transform_func_entity.c x2, transform_func_path.c x1, executor_result_project.c x2). Executor leaves agtype cells NULL for entity JSON so extension.c renders the text verbatim; DELETE derives id/kind from the JSON when no agtype cell (`entity_ref_from_json`). Edge JSON keys are now startNode/endNode at the SQL level (previously agtype normalised startNodeId). Golden diff of 26 entity queries: semantically identical (only agtype spacing and a stored real rendering as 2.0 instead of 2). Release 20K: RETURN n 409 -> 89 ms, RETURN r 366 -> 62 ms. Unit 950/950, functional 10/10, Python 400, Rust 302, TCK 3788.
