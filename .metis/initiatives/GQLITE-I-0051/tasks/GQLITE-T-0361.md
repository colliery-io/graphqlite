---
id: f5-return-n-materialisation-entity
level: task
title: "F5: RETURN n materialisation — entity JSON pass-through and typed-table property object"
short_code: "GQLITE-T-0361"
created_at: 2026-09-07T01:25:08.446195+00:00
updated_at: 2026-09-07T01:25:08.446195+00:00
parent: GQLITE-I-0051
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0051
---

# F5: RETURN n materialisation — entity JSON pass-through and typed-table property object

## Parent Initiative

[[GQLITE-I-0051]]

## Objective **[REQUIRED]**

Perf review finding 5: `RETURN n` costs ~35 us and ~300 mallocs per node (757 ms at 20K). The SQL scans all of `property_keys` per node and probes ten indexes per key; the executor then parses the JSON back into an agtype tree and serialises it twice.

## Acceptance Criteria **[REQUIRED]**

- [ ] Implemented per the design below with regression tests on SQL/plan shape or behaviour (no timing assertions in CI).
- [ ] `tests/performance/python/sweep.sh` numbers recorded in the PR description before/after.
- [ ] Full matrix green: unit, functional, python, rust, TCK (pass count unchanged).

## Implementation Notes

1. Pass entity JSON through verbatim (extension.c already has a verbatim path for `{`/`[` values).
2. Build the property object from the five typed tables by `node_id` joined to `property_keys` (measured 104 ms vs 182 ms for the SQL alone); consolidate the template, currently copied at eight sites (`transform_return.c:313`, `executor_result_project.c:455`, ...).
3. Size the output buffer once and serialise once.

Expected ~757 ms -> ~120 ms at 20K nodes; also shrinks finding 10's memory amplification.

Source: `docs/internal/performance-review.md` (PR #118).

## Status Updates **[REQUIRED]**

- 2026-09-06: created from the review; not started (phase 2+).
