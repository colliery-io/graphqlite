---
id: a4-write-path-dispatcher-honors
level: task
title: "A4: Write-path dispatcher honors WITH/WHERE between CREATE and RETURN"
short_code: "GQLITE-T-0328"
created_at: 2026-05-23T10:54:54.956711+00:00
updated_at: 2026-05-23T14:17:03.882057+00:00
parent: GQLITE-I-0044
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0044
---

# A4: Write-path dispatcher honors WITH/WHERE between CREATE and RETURN

## Parent Initiative

[[GQLITE-I-0044]] — Phase A quick win #4.

## Objective

The dispatcher pattern `UNWIND+CREATE+RETURN` (and likely sibling
write patterns) silently ignores intermediate `WITH/WHERE` clauses
between the CREATE and RETURN. The CREATE produces a set of bound
maps, and RETURN projects them — but any filter the user wrote in
between is dropped.

## Repro

```cypher
UNWIND [1, 2, 3, 4, 5] AS x
CREATE (n:N {num: x})
WITH n
WHERE n.num % 2 = 0
RETURN n.num AS num;
```

**Expected:** 2 rows (num=2, num=4). 5 nodes created (side effect),
3 filtered out.
**Actual:** 5 rows (all values). WITH/WHERE silently ignored.

## Affected scenarios

- Create6 [5] Filtering after creating nodes affects the result
  set but not the side effects.
- Create6 [7] Aggregating in WITH after creating nodes.
- Create6 [12] Filtering after creating relationships.
- Create6 [14] Aggregating in WITH after creating relationships.

Estimated **+3 TCK** (some Create6 failures use aggregation
which may need more work).

## Implementation plan

1. Find `handle_unwind_create_return` in `query_dispatch.c`.
   Currently builds `maps[0..n_maps]` from the CREATE loop and
   projects directly.
2. Detect intermediate WITH/WHERE clauses in the query AST. Either
   restrict the pattern (forbid CLAUSE_WITH for this handler) and
   route through generic transform, OR honor it here.
3. Honoring inline is preferred for now (less invasive):
   - After the CREATE loop produces `maps`, iterate WITH clauses
     in order.
   - For each WITH-WHERE, filter `maps` using the predicate.
   - For aggregating WITH, more work needed — defer to B4 if it
     starts hitting hard issues.
4. The predicate evaluator needs a public surface. Currently
   `evaluate_ast_with_context` is `static` in
   `executor_set.c`. Either:
   - Expose it via a new public header
     (`executor_internal.h` or a new
     `executor_predicate.h`).
   - Wrap it in a public `executor_eval_predicate` function.
5. Run TCK to confirm Create6 wins + no regressions on existing
   write-path scenarios.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Create6 [5] passes (5 nodes created, 2 returned).
- [ ] Create6 [12] passes (variant with rels).
- [ ] Side effects preserved (CREATE still creates all rows).
- [ ] Filter applied to the projection only.
- [ ] No regression on existing CREATE/UNWIND/RETURN tests.
- [ ] `angreal test unit && angreal test functional` clean.

## Affected files

- `src/backend/executor/query_dispatch.c` — primary.
- `src/backend/executor/executor_set.c` — expose
  `evaluate_ast_with_context` if going that route.
- Possibly `src/include/executor/executor_internal.h` for the
  new public predicate-eval surface.

## Out of scope

- Aggregating WITH (e.g. `WITH count(*) AS c`) — that's part of
  B4 (full WITH/WHERE/SKIP/LIMIT/ORDER thread-through). A4
  only does the simple predicate filter case.
- MERGE+WITH+RETURN, MATCH+SET+WITH+RETURN — similar shape but
  different handlers; B4 will sweep them.

## Effort

M — needs a public predicate evaluator + dispatcher edit.

## Status Updates

### 2026-05-23 — Completed (partial scope)

Implemented as planned:

1. **`executor_set.c::executor_eval_predicate`** — public wrapper
   around the static `evaluate_ast_with_context`. Returns 1/0/-1
   for pass/drop/error. Coerces INTEGER/REAL/TEXT result to bool;
   NULL → drop (Cypher three-valued).

2. **`handle_unwind_create_return`** — after the CREATE loop
   populates `maps`, scan the query's clause list for WITH
   clauses with a WHERE. For each, evaluate the predicate
   against every map and drop those that fail. Aggregating WITH
   (any item with `count`/`sum`/etc.) is skipped — that's B4's
   territory.

**TCK: 3565 → 3566 (+1):**
- Create6 [5] Filtering after creating nodes affects the result
  set but not the side effects. ✓

Other Create6 scenarios still fail but for DIFFERENT root
causes — NOT the predicate filter:

- **Create6 [7]** Aggregating in WITH — skipped in A4 by
  design (`agg_with` check). B4 territory.
- **Create6 [12]** Filtering after creating relationships —
  this query has `CREATE ()-[r:R …]->() WITH r WHERE r.num … RETURN r.num`.
  Even without my filter, `r.num` returns NULL in the projection
  — the UNWIND+CREATE handler doesn't put the rel variable
  `r` into the var_map, so subsequent `r.num` lookups fail.
  My filter then sees all-NULL predicates and drops everything.
  **The underlying bug is the rel-var omission in the dispatcher,
  not the filter.** Tracked under T-0183 (UNWIND $param in write
  paths) family — file a new task if T-0183 doesn't cover this
  exact shape.
- **Create6 [14]** Aggregating in WITH after rels — B4 + rel-var
  combined.

Net: A4 mechanism works; remaining Create6 failures are out
of A4's scope.

944/944 unit, functional clean. 0 regressions.