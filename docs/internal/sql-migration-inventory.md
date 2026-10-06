# SQL Generation API Migration Inventory (I-0039 S1–S3)

Snapshot taken at the start of the I-0039 migration (2026-05-19). Goal:
collapse the three coexisting SQL-output APIs in
`src/backend/transform/` into one (`sql_builder`).

## S1 — `append_sql` family call sites

Total: **977** calls across the legacy trio
(`append_sql` / `append_identifier` / `append_string_literal`).

Per file (highest first):

| File                                 | Calls |
| ------------------------------------ | ----: |
| `transform_func_temporal.c`          |   283 |
| `transform_expr_ops.c`               |   167 |
| `transform_return.c`                 |   112 |
| `transform_func_math.c`              |    56 |
| `transform_expr_predicate.c`         |    48 |
| `transform_func_string.c`            |    40 |
| `transform_func_aggregate.c`         |    40 |
| `transform_func_list.c`              |    38 |
| `transform_func_geo.c`               |    38 |
| `transform_func_typeconv.c`          |    28 |
| `transform_set.c`                    |    27 |
| `transform_func_entity.c`            |    20 |
| `transform_create.c`                 |    16 |
| `transform_func_path.c`              |    15 |
| `transform_func_json.c`              |    14 |
| `transform_remove.c`                 |    11 |
| `cypher_transform.c`                 |    11 |
| `transform_func_graph.c`             |     7 |
| `transform_delete.c`                 |     6 |

Notes:
- `transform_func_temporal.c` dominates because of the EMIT/EMIT_TIME_BASE
  macros expanded across the truncate/duration code paths. The macros
  internally call `append_sql`. Migration must keep the macro abstraction
  or inline the SQL emission.
- The five small files (`transform_set`, `transform_delete`,
  `transform_remove`, `transform_create`, `transform_func_graph`,
  `transform_delete`) total ~73 calls — natural first migration batch
  (S5–S6 of the initiative).

## S2 — `pending_prop_joins` writes & invariants

> **Superseded 2026-10-05 (S14, GQLITE-T-0268).** The description below was
> the 2026-05-19 snapshot and it misread the buffer: `pending_prop_joins`
> never held CTEs and `prepend_cte_to_sql` never touched it. The buffer held
> raw ` LEFT JOIN node_props_{int,real,text} AS _prop_N_* ON ...` fragments
> produced by `transform_func_aggregate.c` for aggregates on properties, and
> they were spliced into the **JOIN section**: at the end of
> `transform_return_clause` via `sql_join_raw`, and in
> `transform_with_clause` right after the borrowed `joins_clause` of the CTE
> body. Deferral is needed because `transform_with_clause` takes
> `sql_builder_get_joins()` (a pointer into the `joins` dbuf, NULL when
> empty) *before* the projection items run.
>
> S14 moved the buffer into `sql_builder` as the typed `deferred_joins`
> section — `sql_join_deferred()` (writer, `transform_func_aggregate.c`),
> `sql_builder_flush_deferred_joins()` (RETURN), `sql_builder_get_deferred_joins()`
> + `sql_builder_clear_deferred_joins()` (WITH; and the start-of-clause
> discards that replaced `reset_pending_prop_joins`). `sql_builder_reset`
> clears it; `sql_builder_to_string` does not emit it. Generated SQL is
> byte-identical (TCK 3799/3799, 0 regressions, 0 gains). After S14 the
> `cypher_transform_context` fields and the `*_pending_prop_join*` helpers
> are dead; S15 (GQLITE-T-0269) deletes them.
>
> `sql_pre_cte` (S13) is unaffected: it remains the CTE-prefix section fused
> with the user CTE buffer in front of the final SQL.

Original snapshot (2026-05-19), kept for history:

Three writers, three readers, one CTE-prepend invariant.

**Writers / readers** (file:line):

- `transform_return.c:55`  — `add_pending_prop_join()` impl
- `transform_return.c:45`  — `get_pending_prop_joins()` impl
- `transform_return.c:50`  — `get_pending_prop_joins_len()` impl
- `transform_return.c:37`  — `reset_pending_prop_joins()` impl
- `transform_return.c:{136,479}` — resets after consuming
- `transform_func_aggregate.c:200` — `add_pending_prop_join(ctx, …)`
- `transform_with.c:{215,486,488,491}` — read + reset across WITH boundary

**Prepend invariant** (as written in 2026-05; see the note above):
`prepend_cte_to_sql(ctx)` runs at end of transformation and splices the
builder's CTE buffers onto the front of the SQL buffer. Defined in
`cypher_transform.c`. Called from:

- `cypher_transform.c` (main transform entry points: query, EXPLAIN,
  UNION variants of both)
- `executor_match.c`
- `executor_call_subquery.c`
- `executor_merge_pipeline.c`

## S3 — Capability gap analysis

`sql_builder` already exposes the API needed to cover the legacy trio:

| Legacy shape                              | sql_builder API                                     | Status |
| ----------------------------------------- | --------------------------------------------------- | ------ |
| `append_sql(ctx, "SELECT %s …", expr)`    | `sql_select(b, expr, alias)`                        | ✓      |
| `append_sql(ctx, "FROM %s %s", t, a)`     | `sql_from(b, table, alias)`                         | ✓      |
| `append_sql(ctx, "WHERE %s", cond)`       | `sql_where(b, cond)`                                | ✓      |
| `append_sql(ctx, " AND %s", more_cond)`   | `sql_where(b, cond)` (appends with AND)             | ✓      |
| `append_sql(ctx, "LEFT JOIN … ON …")`     | `sql_join(b, SQL_JOIN_LEFT, t, a, on)` /            | ✓      |
|                                           | `sql_join_raw(b, raw)`                              |        |
| `append_sql(ctx, "GROUP BY %s", expr)`    | `sql_group_by(b, expr)`                             | ✓      |
| `append_sql(ctx, "ORDER BY %s%s", e, d)`  | `sql_order_by(b, expr, desc)`                       | ✓      |
| `append_sql(ctx, "LIMIT %d OFFSET %d")`   | `sql_limit(b, lim, off)` / `sql_limit_expr(...)`    | ✓      |
| `append_identifier(ctx, name)`            | identifiers folded into other API surfaces; for     | partial|
|                                           | raw appends use `dbuf_appendf(&b->scratch, …)`      |        |
| `append_string_literal(ctx, value)`       | `escape_sql_string(value)` returns malloc'd literal;| ✓      |
|                                           | feed into existing setters                          |        |
| `append_sql(ctx, "WITH cte AS (…)")`      | `sql_cte(b, name, query, recursive)`                | ✓      |
| `add_pending_prop_join(ctx, sql)`         | `sql_join_deferred(b, raw)` + flush/get/clear (S14) | ✓      |
| INSERT/UPDATE/DELETE shapes               | `write_builder_*` API in sql_builder.{c,h}          | ✓      |

**Remaining gaps (updated after S5 pre-flight, 2026-05-19):**

1. **pending_prop_joins** — closed by S13 (`sql_pre_cte`, CTE prefix) and
   S14 (`deferred_joins`, the section the buffer actually needed).

2. **DML statement emission** — discovered during the S5 pre-flight.
   `transform_delete.c` (6 calls), `transform_remove.c` (11), and
   `transform_set.c` (27) emit compound DML statements separated by
   `; ` (e.g. `DELETE FROM node_props_text WHERE ...; DELETE FROM
   nodes WHERE ...`). The `sql_builder` typed sections (select/from/
   joins/where/group_by/order_by/limit/cte) model SELECT-shape
   queries; they have no notion of multi-statement output, nor do
   they cleanly express compound semicolon-separated DML.

   The `write_builder` API in sql_builder.h covers single DML
   statements (write_insert_values, write_delete, write_delete_where_in,
   write_raw) but writes to its own buffer, not the unified builder.
   Three resolution paths:

   - **Extension A (recommended):** add a `sql_raw(sb, fmt, ...)`
     API writing into a new "raw output" dbuf section, with
     `sql_builder_to_string` emitting it after the typed sections.
     This is the "thin shim" pattern the initiative spec rejected
     — but DML emission doesn't fit the typed model, so a raw
     output channel is unavoidable for these files.
   - **Extension B:** route DML through write_builder per-statement;
     concat its output after the SELECT-shaped output during
     finalize. Requires plumbing write_builders into
     cypher_transform_context.
   - **Extension C:** keep DML files on the legacy trio and only
     target SELECT-shaped files (transform_match, transform_with,
     transform_return, transform_expr_ops, transform_func_*) in
     S5–S12. Legacy `append_sql` stays for the 44 DML calls.

   This gap MUST be resolved before S5 can execute meaningfully on
   the DML files. Estimated: 1 day for Extension A including TCK
   verification.

   **Extension A implementation attempt (2026-05-19):** Added
   `dbuf raw_output` to sql_builder, `sql_raw()` API, and migrated
   transform_delete.c's 6 calls. Result: TCK regressed -181
   (3358 → 3177), error count tripled. **Root cause** is deeper than
   the API gap: the executor's per-clause handlers
   (handle_match_delete, handle_match_set, etc. in query_dispatch.c)
   call `finalize_sql_generation` AFTER transforming each clause that
   produces typed SQL (MATCH), then expect subsequent clauses
   (DELETE/SET/REMOVE) to append onto the already-finalized
   `sql_buffer` via `append_sql`. Routing DML through unified_builder's
   raw_output instead means the DML statements stay in unified_builder
   AFTER finalize has already serialized the SELECT into sql_buffer —
   so they never reach the executed SQL.

   **Full fix shape:** defer finalize_sql_generation until ALL clauses
   are transformed. Each handler in query_dispatch.c must change to
   "transform all clauses → finalize once → execute". Currently
   handlers interleave transform/execute per clause, which is
   incompatible with structured assembly.

   This is a non-trivial executor refactor — touching every
   handle_match_* function. Estimated: 3-5 days including TCK
   regression hunting on the per-handler flow changes.

   Until the executor refactor lands, **S5–S12 should target
   SELECT-shaped files only** (transform_match, transform_with,
   transform_return, transform_expr_ops, transform_func_*). The 44
   DML calls in set/delete/remove stay on the legacy trio.

3. **Expression scratchpad — NOT a migration target.** Discovered
   2026-05-20 during S7 investigation. The bulk of the remaining
   ~917 trio calls live inside `transform_expression` (in
   `transform_return.c:714+`) and its dispatched function
   transforms (`transform_func_*.c`, `transform_expr_ops.c`,
   `transform_expr_predicate.c`). These calls write into
   `ctx->sql_buffer` not as the final output destination but as a
   **swappable scratch buffer** that the calling code redirects via
   `transform_expression_to_string` (a buffer-swap-and-capture
   helper that has lived in transform_return.c since the early
   days).

   The outer `transform_return_clause` function itself has ZERO
   append_sql calls — it already uses `sql_select() /
   sql_order_by() / sql_limit_expr()` against `unified_builder`,
   passing captured expression strings as values. The expression
   dispatch tree using `append_sql` is the internal scratchpad API
   for that capture flow, not legacy DML.

   This reframes the inventory: the migration goal should be
   "move all WRONG-destination writes to sql_builder" rather than
   "delete every trio call". The wrong-destination writes are the
   DML-emitting transforms (set/delete/remove/create), which are
   now done.

   **Practical S5-S12 endpoint:** keep the trio for internal
   expression-scratchpad use. Either:
   - Remove the `__attribute__((deprecated))` markers on the trio
     so the warnings stop, OR
   - Rename the trio to e.g. `expr_buf_appendf` /
     `expr_buf_append_id` / `expr_buf_append_str_literal` and
     re-export under those names without the deprecation. The
     existing transform_expression machinery uses these via
     ctx->sql_buffer scratchpad with no need to migrate.

**The macro abstraction in transform_func_temporal.c (EMIT,
EMIT_TIME_BASE, EMIT_HH/MM/SS)** is currently `append_sql`-based.
During migration these macros should be rewritten to call into
`sql_builder`'s SQL accumulator (`dbuf_appendf(&b->select, …)` or
equivalent typed setter), or replaced with helper inline functions
that accept a `sql_builder *`.

## S4 — Deprecation status

`append_sql`, `append_identifier`, `append_string_literal` are marked
`__attribute__((deprecated))` in `src/include/transform/cypher_transform.h`
as of the I-0039 S4 commit. New code attempting to use them generates
a compile-time warning; existing call sites warn but continue to
compile so the per-file migration (S5–S12) can be done incrementally
without breaking the build.

## Migration order (recap from I-0039)

1. **S5** — `transform_set.c`, `transform_delete.c`, `transform_remove.c`
   (44 calls combined — smallest batch)
2. **S6** — `transform_create.c`, `transform_foreach.c`,
   `transform_unwind.c` (16 + foreach + unwind)
3. **S7** — `transform_match.c` (high call count, hot path; also fixes
   varlen alias + OPTIONAL MATCH bugs per the initiative spec, ~37 TCK)
4. **S8** — `transform_with.c`
5. **S9** — `transform_return.c` (112 calls)
6. **S10** — `transform_expr_ops.c` (167 calls),
   `transform_expr_predicate.c` (48 calls)
7. **S11** — `transform_func_string/math/path/entity/aggregate/graph.c`
   (~180 calls combined)
8. **S12** — `transform_func_temporal.c` (283 calls — largest single
   file; the EMIT macro family needs `sql_builder`-aware variants),
   `transform_func_list/geo/json/typeconv.c` (~118 calls).

Each migration commit must show zero TCK scenario delta and clean
unit + functional runs.
