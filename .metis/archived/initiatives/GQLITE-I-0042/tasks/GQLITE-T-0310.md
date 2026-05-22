---
id: i-0042-architecture-split-dml
level: task
title: "I-0042 architecture: split DML/SELECT at builder boundary for MATCH+SET+WITH+RETURN"
short_code: "GQLITE-T-0310"
created_at: 2026-05-21T20:30:00+00:00
updated_at: 2026-05-22T01:40:52.180828+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: true

tags:
  - "#task"
  - "#tech-debt"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# Split DML / SELECT at the sql_builder boundary

## Status Updates

**2026-05-21 iter 6 — LANDED (narrow scope).** Option (c) plus
narrow `INSERT OR REPLACE INTO`-only guard. TCK 3468 → 3469 (+1).

The infrastructure shipped:
- `sql_builder_take_raw_output` / `sql_builder_has_raw_output` /
  `sql_builder_clear_raw_output` builder API.
- `cypher_query_result.pre_exec_dml` field; `cypher_free_result`
  frees it.
- `cypher_transform_context.cte_prefix_len` records the CTE prefix
  size when `prepend_cte_to_sql` runs. The DML split copies that
  exact prefix onto `pre_exec_dml` so CTE-bound refs resolve.
- `handle_generic_transform` runs `sqlite3_exec(pre_exec_dml)`
  before stepping the prepared SELECT.
- `transform_with_clause` preserves `raw_output` across the
  `sql_builder_reset` call (was wiping the DML).

The narrowing: only `INSERT OR REPLACE INTO` DML (transform_set's
pattern — self-contained `INSERT ... SELECT ... FROM ...`) is
split. Plain `INSERT INTO` (transform_create's per-row pattern)
and naked `DELETE FROM` (transform_remove / transform_delete) are
NOT split — they stay in the legacy compound form where prepare_v2
drops them after the SELECT. This matches baseline behavior for
Create3 / Remove3 / Delete6.

Gain: Set6 [5] `MATCH+SET+WITH+WHERE+RETURN` now correctly
applies the SET before the SELECT sees the values.

Acceptance criteria:
- [x] T-0310 infrastructure landed (builder API + result field +
  executor exec + WITH preservation + cte_prefix_len).
- [x] Set6 [5] passes.
- [ ] Set6 [7], [19], [21] still fail — these have aggregations
  (`WITH SUM(n.num)` etc.) and other patterns that don't fit the
  narrow `INSERT OR REPLACE INTO` shape, OR they use REMOVE rather
  than SET. Need follow-on work to widen the safe-split set:
  rewrite transform_remove / transform_delete to emit self-contained
  DML (T-0311 etc.).

T-0310 stays open — infrastructure is in but the safe-split set
needs to grow. Filed follow-on guidance in I-0042 design.

**2026-05-21 iter 7 — widening attempts (REMOVE, edges).** Tried to
widen the safe-split set to unlock Set6 [7]/[19]/[21] and Remove3:

1. **Rewrote transform_remove** to emit self-contained DELETEs
   (`DELETE FROM node_props_X WHERE key_id = ... AND node_id IN
   (SELECT alias.id FROM ... WHERE matchwhere)`). Also extended
   the guard in cypher_transform_query to accept `DELETE FROM`.
   Result: Set6 [5] kept, but Remove3 [12]/[14] failed because
   running REMOVE-label before the post-WITH SELECT changes the
   label state — the SELECT re-MATCHes `(n:N)` and finds no rows.
   This is the **post-write re-MATCH semantics** issue (Cypher
   spec wants WITH-bound IDs preserved across writes); requires
   the snapshot-id-capture pattern from T-0314 (option (c) in
   I-0042 doc).
2. **Set6 [19]/[21] (relationships)** — pre-existing transform_set
   bug. `generate_property_update` always emits `node_id` and
   `node_props_*` regardless of whether the variable is a node or
   edge. SET on relationships writes to the wrong table → no
   effect → SELECT still sees pre-SET values. Out of T-0310 scope.
3. **Set6 [7] (aggregation)** — `WITH count(r) AS c` after SET.
   Same shape as [5] but with aggregation; needs the post-SET
   read path to work for `count(*)` too. Closer to [19] than to
   [5] — likely the same re-MATCH issue.

Reverted both widening attempts. T-0310 stays at +1 (Set6 [5]).

**What T-0310 actually unblocks beyond +1:**
The infrastructure is the prerequisite for:
- T-0314 (E5: handle_match_set true two-pass) — needs the
  pre_exec_dml channel to run pre-WITH writes.
- transform_set edge-aware emission — separate task; once that
  lands, Set6 [19]/[21] light up via the existing T-0310 path.
- transform_remove / transform_delete with snapshot-id capture
  via T-0297/T-0314 — unlocks Remove3 / Delete6 families.

T-0310's job is done: builder API + result.pre_exec_dml + ctx.
cte_prefix_len + raw_output WITH-preservation + executor exec path.
Moving to completed.



**2026-05-21 attempt (iter 5)** — Tried the proposed approach: split
raw_output into `cypher_query_result.pre_exec_dml`, exec it via
`sqlite3_exec` in `handle_generic_transform` before stepping the
SELECT. Set6 [5] (`MATCH+SET+WITH+RETURN`) WORKS in isolation —
SQL is split correctly, UPDATE runs first, SELECT sees post-SET
state.

**But TCK regressed -14 (3468 → 3454).** Root cause traced:

1. When the DML references CTE-bound variables (e.g. `_with_0.n.id`
   in a `MATCH+WITH+DELETE+RETURN` query), the DML alone can't
   resolve the reference — the CTE definition lives in the SELECT's
   `WITH` prefix.
2. Attempted fix: copy the CTE prefix to pre_exec_dml. Problem: the
   "find SELECT keyword" heuristic to identify the prefix boundary
   matches `SELECT` inside CTE bodies (e.g. `WITH _with_0 AS (SELECT ...) SELECT ...`
   — the first `SELECT ` is inside the CTE). Lost more scenarios
   than it fixed (Delete6, Remove4 family).
3. Prepending the full CTE to the DML changes the INSERT semantics:
   the INSERT references the CTE in unintended ways (the INSERT's
   own FROM clause + the CTE's FROM clause can produce ambiguous
   column refs).

**Deeper architectural assessment:** the `unified_builder` doesn't
preserve enough structure to cleanly split into (CTE | DML | SELECT)
trihalves. The pre-existing `ctx->sql_buffer` scratchpad and
`prepend_cte_to_sql` boundary-prepending pattern blur the line
between "where does the CTE end and the SELECT body start." A
proper fix needs one of:

  a. **Restructure the builder** to return all three halves
     explicitly (`get_cte_prefix`, `get_dml`, `get_select_body`)
     — the executor composes the final SQL string per half.
  b. **Stop using `ctx->sql_buffer`** entirely; the builder owns
     all assembly state. (This is `I-0043` territory.)
  c. **Track the CTE prefix length** in the builder when
     `prepend_cte_to_sql` runs, so a downstream splitter knows
     exactly where the SELECT body starts.

Option (c) is the minimal-risk path but still requires the builder
to be the sole source of CTE truth. Iter-5 ran out of iteration
budget; reverted full back to baseline 3468.

**Recommendation for the next attempt:** prototype option (c) —
add a `cte_prefix_len` field to the builder, set it inside
`prepend_cte_to_sql`, and use that as the canonical split point.
The retry should preserve the iter-5 work (the executor exec path
and the WITH-reset preservation logic) and only swap the
"find-SELECT-keyword" heuristic for the explicit length.

## Why this matters

The current sql_builder produces a single `char *` containing the
entire compound SQL. For queries that combine SET/DELETE/REMOVE with
a subsequent read (MATCH+SET+WITH+RETURN, MATCH+SET+RETURN-with-
aggregation, etc.), this means:

  WITH _with_0 AS (...) SELECT ... FROM _with_0; INSERT OR REPLACE ...

`sqlite3_prepare_v2` only consumes the first statement, so the
trailing DML is silently dropped — the SET never runs. Reordering
to `INSERT...; WITH _with_0 AS (...) SELECT ...` doesn't help either:
`prepend_cte_to_sql` writes the WITH at the **start** of the buffer,
so it lands in front of the INSERT, not the SELECT.

This is the architectural blocker that prevents the I-0042 E5 / E6
work (Set6 [5]/[7]/[19]/[21] family) from being a single-iteration
fix. The handlers themselves are fine; the SQL-assembly side needs
to grow first.

## Affected paths

- MATCH+SET+WITH+RETURN  (Set6 [5]/[7])
- MATCH+SET+WITH+(aggregation)+RETURN  (Set6 [19]/[21])
- Same shapes with REMOVE / DELETE
- Anywhere `handle_generic_transform` is invoked with `raw_output`
  populated alongside SELECT columns.

## Root cause chain (iter-2 E5 attempt)

1. `transform_with_clause` calls `sql_builder_reset` which wipes
   raw_output. Iter 2 patched this with a save/restore but it's not
   the deepest issue.
2. `sql_builder_to_string` appends raw_output AFTER the SELECT body.
   `sqlite3_prepare_v2` only handles the first statement, so the
   DML drops silently.
3. `prepend_cte_to_sql` writes `WITH ...` at the buffer start, so
   any reorder breaks the WITH-binds-to-SELECT contract (`WITH x AS
   () INSERT ...; SELECT ...` is a parse error).
4. `cypher_query_result` carries a single `sqlite3_stmt *`. Even if
   the builder split the SQL, the executor has nowhere to keep a
   "pre-exec this DML then step that prepared stmt" pair.

## Proposed fix

Restructure at the builder / result-holder level:

1. `sql_builder` exposes a separate `to_dml_string()` (returns
   raw_output) and `to_select_string()` (returns SELECT body with
   CTEs correctly prepended ONLY to the SELECT half).
2. `cypher_query_result` gains a `char *pre_exec_dml;` field. The
   transform layer sets it when raw_output was non-empty; the
   prepared `stmt` only holds the SELECT.
3. `handle_generic_transform` calls `sqlite3_exec(db, pre_exec_dml)`
   before stepping `stmt`.

## Affected files

- `src/include/transform/sql_builder.h`
- `src/backend/transform/sql_builder.c`
- `src/include/transform/cypher_transform.h` (cypher_query_result)
- `src/backend/transform/cypher_transform.c`
- `src/backend/executor/query_dispatch.c` (handle_generic_transform)

## Test acceptance

```
MATCH (n:N) SET n.num = n.num + 1
WITH n WHERE n.num % 2 = 0
RETURN n.num AS num
```

Should return `[{num:2}, {num:4}, {num:6}, ...]` (post-SET filtered
even values). Currently returns the pre-SET filtered set or empty.

The TCK acceptance criteria mirror I-0042 E5: Set6 [5]/[7]/[19]/[21].

## Discovered

2026-05-21 during the I-0042 Ralph loop iter 2 attempt at E5.
Reverted the partial patch; this ticket captures the path forward.