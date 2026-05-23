---
id: limit-skip-inside-with-applied-to
level: task
title: "LIMIT/SKIP inside WITH applied to outer SELECT instead of WITH CTE"
short_code: "GQLITE-T-0323"
created_at: 2026-05-23T05:17:00+00:00
updated_at: 2026-05-23T22:02:37.594769+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# LIMIT/SKIP inside WITH applied to outer SELECT, not WITH CTE

## Discovered

2026-05-22 during v0.5.0 work. Observed while triaging
ReturnSkipLimit2 [6].

## Repro

```cypher
UNWIND range(1, 3) AS i CREATE ({nr: i});
-- (3 nodes created)
MATCH (n) WITH n LIMIT 2 RETURN count(*) AS count;
```

**Expected:** 1 row with `count = 2` (LIMIT 2 applies to MATCH's
output via WITH, count then aggregates 2 rows).
**Actual:** 1 row with `count = 3` (LIMIT applies to the outer
SELECT which produces 1 count row anyway, so LIMIT is a no-op;
count counts all 3 MATCH'd nodes).

## Generated SQL (illustrative)

```sql
WITH _with_0 AS (SELECT alias.id AS n FROM nodes AS alias)
SELECT COUNT(*) AS count FROM _with_0 LIMIT 2;
```

The `LIMIT 2` is on the outer SELECT, not inside the `_with_0`
CTE definition where it semantically belongs.

## Correct SQL

```sql
WITH _with_0 AS (SELECT alias.id AS n FROM nodes AS alias LIMIT 2)
SELECT COUNT(*) AS count FROM _with_0;
```

The LIMIT lives inside the CTE so the CTE produces 2 rows. Then
count(*) over 2 rows = 2.

## Root cause

`transform_with.c`'s LIMIT/SKIP handler calls `sql_limit` /
`sql_limit_expr` on the unified builder, which writes LIMIT to
the **final assembled SQL**, not to the WITH CTE that's being
built. The WITH CTE is constructed inside the builder; the LIMIT
needs to be applied to the CTE's inner SELECT, not the outer
projection.

## Proposed fix

Two approaches:

1. **Inline LIMIT into the CTE definition.** When transform_with
   builds the CTE body via `sql_cte`, fold the LIMIT/OFFSET into
   that body's inner SELECT. The outer SELECT inherits no LIMIT
   from this WITH.

2. **Don't use a CTE; use a subquery as the FROM target.** If
   restructuring proves easier, lift the WITH-with-LIMIT into a
   `FROM (SELECT ... LIMIT N) _with_0` form instead of a CTE.

Approach 1 is closer to the current architecture.

## Affected scenarios

- ReturnSkipLimit2 [6] "LIMIT with an expression that does not
  depend on variables".
- Probably any test with `MATCH (n) WITH n SKIP/LIMIT … <more>`
  where the SKIP/LIMIT should bound the WITH's row set.
- Related: WithSkipLimit1 [1] / WithSkipLimit2 [2] — though those
  also have the inter-pattern CREATE ref bug (T-0322); fixing
  this one may unblock once T-0322 is fixed.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] ReturnSkipLimit2 [6] passes (1 row with count=2).
- [ ] Generated SQL: LIMIT/OFFSET appears inside CTE body, not
  outer SELECT.
- [ ] No regression on existing LIMIT/SKIP scenarios (e.g.
  RETURN-level LIMIT must still work).

## Affected files

- `src/backend/transform/transform_with.c`
- `src/backend/transform/sql_builder.c` (possibly — depends on
  whether the CTE-with-LIMIT shape needs a new builder primitive).

## Notes

This is a transform-layer fix only; executor doesn't need to
change. Estimated effort: S-M.

## Status Updates

### 2026-05-23 — Completed (partial scope)

In `transform_with.c`, inline `LIMIT N OFFSET K` into the CTE body
when WITH has LIMIT/SKIP **and no ORDER BY**. Suppress the
outer-builder `sql_limit()` for that case so the constraint isn't
double-applied. With ORDER BY also present, keep the existing
outer-builder path — pushing ORDER BY into the CTE has alias-
resolution / aggregate-handling issues that need more careful
work (a full ORDER BY push regressed 21 scenarios in iteration 2
before reverting).

**TCK: 3571 → 3573 (+2):**
- ReturnSkipLimit2 [6] LIMIT with an expression that does not
  depend on variables (1 row, count=2).
- Aggregation3 [2] No overflow during summation (different
  query shape that benefited from the CTE-LIMIT positioning).

Out of scope (left for follow-up):
- WITH ORDER BY + LIMIT/SKIP combined. Pushing ORDER BY into the
  CTE body needs re-doing alias resolution and aggregate handling
  inline. Current path keeps both on the outer SELECT — order is
  correct but counts may be off.

944/944 unit, functional clean. 0 regressions.