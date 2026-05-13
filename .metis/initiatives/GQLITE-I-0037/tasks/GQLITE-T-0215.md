---
id: tck-10-aggregation-ordering-and
level: task
title: "TCK-10: Aggregation, ordering, and DISTINCT determinism audit"
short_code: "GQLITE-T-0215"
created_at: 2026-05-13T12:51:09.695737+00:00
updated_at: 2026-05-13T12:51:09.695737+00:00
parent: GQLITE-I-0037
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0037
---

# TCK-10: Aggregation, ordering, and DISTINCT determinism audit

## Parent Initiative

[[GQLITE-I-0037]]

## Objective

Audit aggregation semantics, ORDER BY stability, LIMIT/SKIP determinism, and DISTINCT behavior — places where SQL backing exposes ordering choices that openCypher specifies more tightly.

## Acceptance Criteria

- [ ] Probes cover: implicit grouping in `RETURN` with aggregates and non-aggregate keys; `count(*)` vs `count(expr)` null handling; `collect(...)` with nulls and ordering; `DISTINCT` on maps/lists/nodes; `ORDER BY` ties (spec allows any order but result must be stable across re-runs of same query/data); `ORDER BY` on null values (spec: nulls last for ASC, nulls first for DESC); `SKIP` / `LIMIT` interaction with `ORDER BY`.
- [ ] Probes are re-run multiple times to detect non-determinism in tied orderings.
- [ ] Audit report at `docs/tck/audit-aggregation-<date>.md` documents probe / expected / actual / pass-fail-flaky.
- [ ] Bugs filed for spec deviations and for non-determinism that the spec forbids.

## Implementation Notes

### Technical Approach
Non-determinism that the spec *allows* (e.g. ordering when no `ORDER BY` given) is not a bug, but should still be noted so users aren't surprised.

### Dependencies
Independent of TCK-06/07; can run in parallel after [[GQLITE-T-0210]].

## Status Updates

*To be added during implementation*
