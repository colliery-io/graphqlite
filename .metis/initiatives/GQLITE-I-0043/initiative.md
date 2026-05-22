---
id: transform-expression-to-string
level: initiative
title: "transform_expression to string-returning API — eliminate ctx-&gt;sql_buffer scratchpad"
short_code: "GQLITE-I-0043"
created_at: 2026-05-20T14:19:22.886591+00:00
updated_at: 2026-05-20T14:19:22.886591+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: XL
initiative_id: transform-expression-to-string
---

# transform_expression to string-returning API

## Context

The transform layer's expression machinery — `transform_expression`
(in `src/backend/transform/transform_return.c:714`) and its dispatched
function transforms (`transform_func_*.c`, `transform_expr_ops.c`,
`transform_expr_predicate.c`) — writes SQL fragments into
`ctx->sql_buffer` as a scratch area. Callers that need an isolated
expression string (for inlining into a column projection, a WHERE
condition, a SET value, etc.) use a buffer-swap-and-capture pattern via
`transform_expression_to_string` (defined statically in
transform_return.c at line 92):

```c
char *transform_expression_to_string(ctx, expr) {
    char *saved = ctx->sql_buffer;          // pointer swap
    size_t saved_size = ctx->sql_size;
    size_t saved_cap  = ctx->sql_capacity;
    char *tmp = malloc(4096);
    tmp[0] = '\0';
    ctx->sql_buffer = tmp; ctx->sql_size = 0; ctx->sql_capacity = 4096;
    transform_expression(ctx, expr);        // writes into tmp via append_sql
    char *captured = strdup(ctx->sql_buffer);
    free(ctx->sql_buffer);
    ctx->sql_buffer = saved; ctx->sql_size = saved_size; ctx->sql_capacity = saved_cap;
    return captured;
}
```

This is a working architecture but it has costs:

1. **Hidden coupling.** Every recursive call inside transform_expression
   implicitly writes to `ctx->sql_buffer`. Reading the code requires
   understanding the swap protocol.
2. **Allocation churn.** Each capture call malloc+frees a 4KB temp
   buffer.
3. **The trio deprecation noise.** ~917 `append_sql` /
   `append_string_literal` calls in the expression tree generate
   deprecation warnings on every compile (per
   `docs/internal/sql-migration-inventory.md` section 3). Contributors
   are misled into thinking the calls are wrong.
4. **API ambiguity.** The output destination is dynamic (depends on the
   caller's buffer swap). Static analysis can't follow it.

The clean shape is to make expression transforms **return a string**
(or write into a caller-provided `dynamic_buffer *`). Callers
explicitly own the destination; the implicit scratchpad goes away.

This initiative does that rewrite. It depends on **GQLITE-I-0042**
(executor finalize-sequencing) because the new contract needs a stable
transform/execute boundary to land against.

## Goals

- **G1**: `transform_expression` becomes
  `char *transform_expression(cypher_transform_context *ctx, ast_node *expr)`
  — returns malloc'd string the caller owns, OR
  `int transform_expression_into(cypher_transform_context *ctx, ast_node *expr, dynamic_buffer *out)`
  — writes into caller-owned buffer.
- **G2**: Every dispatched function transform
  (`transform_pagerank_function`, `transform_date_function`,
  `transform_string_function`, etc. — currently ~80 functions) follows
  the same shape. The dispatch table in
  `transform_func_dispatch.c` updates accordingly.
- **G3**: `ctx->sql_buffer` is no longer used as a write target by
  expression-tree code. After the migration, `sql_buffer` exists only
  as the final assembled SQL produced by `finalize_sql_generation` for
  prepare/execute.
- **G4**: The legacy trio (`append_sql`, `append_string_literal`) is
  deleted entirely. The deprecation warning disappears with it.
- **G5**: Zero TCK regression.

## Non-Goals

- Removing `unified_builder` or `sql_builder` — they stay as the
  canonical typed assembly.
- Changing the AST or parser layer.
- Refactoring the executor (that's I-0042 — this initiative depends on
  it).
- Performance optimization beyond what the rewrite naturally gives us.

## Detailed Design

### Surface area

By call count (`docs/internal/sql-migration-inventory.md` post-S5+S6):

| File                                | Calls | Notes                                |
| ----------------------------------- | ----: | ------------------------------------ |
| `transform_func_temporal.c`         |   283 | EMIT/EMIT_TIME_BASE macros — heavy   |
| `transform_expr_ops.c`              |   167 | Binary ops, NOT, property access     |
| `transform_return.c` (in expression)|   112 | transform_expression body itself     |
| `transform_func_math.c`             |    56 | Math fns + noarg + atan2             |
| `transform_expr_predicate.c`        |    48 | EXISTS / list pred / reduce          |
| `transform_func_string.c`           |    40 | String fns                           |
| `transform_func_aggregate.c`        |    40 | count/sum/avg/stdev/percentile       |
| `transform_func_list.c`             |    38 | list ops, range, collect             |
| `transform_func_geo.c`              |    38 | point fns                            |
| `transform_func_typeconv.c`         |    28 | toString/toInteger/etc.              |
| `transform_func_entity.c`           |    20 | id/labels/properties/keys            |
| `transform_func_path.c`             |    15 | nodes/relationships/startNode/...    |
| `transform_func_json.c`             |    14 | json.* helpers                       |
| `transform_func_graph.c`            |     7 | algorithm dispatch                   |
| `cypher_transform.c` (helpers)      |    11 | trio impls + UNION orchestration     |
| `transform_set.c` / etc.            |     2 | trailing comment refs only           |

Total: ~917 trio calls in the expression tree.

### Choice: return-string vs write-into-buffer

**Option A — return string.** Each transform returns a malloc'd char*.
Caller is responsible for freeing.

```c
char *str = transform_expression(ctx, expr);
sql_select(b, str, alias);
free(str);
```

Pros: explicit ownership, easy to test, mirrors existing
`transform_expression_to_string` shape.
Cons: many malloc/free pairs (~10 per row of a typical RETURN).

**Option B — write-into-buffer.** Each transform takes a `dynamic_buffer *`
output and writes into it.

```c
dynamic_buffer out; dbuf_init(&out);
transform_expression_into(ctx, expr, &out);
sql_select(b, dbuf_get(&out), alias);
dbuf_free(&out);
```

Pros: fewer allocations, can reuse buffer across multiple transforms,
buffer growth amortized.
Cons: caller manages dbuf lifecycle, slightly more verbose.

**Recommendation: Option B.** The expression tree is recursive — every
nested expression would otherwise produce its own allocation. With
write-into-buffer, callers can pre-allocate one dbuf and feed it
through. The recursive calls inside transform_expression also become
write-into-buffer (passing the same out down).

### New signatures

```c
/* Top-level expression transform. Writes the SQL for `expr` into `out`.
 * Returns 0 on success, -1 on transform error (ctx->error_message is
 * set by callees). `out` may already contain text — this function
 * appends to it. */
int transform_expression_into(cypher_transform_context *ctx,
                              ast_node *expr, dynamic_buffer *out);

/* Convenience: capture as a malloc'd string. Caller frees. Wraps
 * transform_expression_into internally. */
char *transform_expression_str(cypher_transform_context *ctx, ast_node *expr);

/* Function-transform dispatch — every transform_<func>_function
 * follows this signature, replacing the current (ctx, func_call) -> int
 * via ctx->sql_buffer pattern: */
typedef int (*transform_func_into_handler)(cypher_transform_context *ctx,
                                            cypher_function_call *func,
                                            dynamic_buffer *out);
```

### Migration approach

Per-file migration, one function at a time. Inside transform_expression's
giant switch, each `case AST_NODE_*` branch is independently
migratable: the case takes (ctx, expr) and either writes to ctx->sql_buffer
(legacy) or to out (new). A transitional `case` can do both — write
old recursive results to a local dbuf then copy out.

Easier path: add `transform_expression_into` as a fresh implementation
that DOESN'T touch ctx->sql_buffer. Migrate the cases in batches.
When all cases are migrated, delete the old transform_expression.

### Dispatch table

`transform_func_dispatch.c` currently maps function name → handler
function (signature `int (*)(ctx, func)`). Add a parallel
`transform_func_into_table` with the new signature. Migrate handlers
one at a time, removing each from the old table as it lands in the new.

When old table is empty, delete it.

## Alternatives Considered

- **Leave the scratchpad pattern.** Working but expensive in terms of
  maintainability and the constant deprecation noise. The user
  pushback is specifically about wanting the trio gone.
- **Thread-local active output buffer.** Hack — keeps the implicit
  destination problem, just moves the storage. Rejected.
- **Macro-expand append_sql into write-to-passed-buffer.** Would change
  every call site's signature implicitly. Hard to review.
- **Generate the code via a code-gen step.** Overkill for ~80
  function transforms.

## Implementation Plan

### Phase 1 — Add new API surface

- **X1** ✅ (2026-05-21): Added `transform_expression_into(ctx, expr,
  dynamic_buffer*)` and `transform_expression_str(ctx, expr)` in
  `src/include/transform/cypher_transform.h` +
  `src/backend/transform/cypher_transform.c`. Both delegate to the
  existing `cypher_transform_capture_expression` scratchpad path —
  zero behavioral change, durable infrastructure. The case-by-case
  X2 migrations route through these wrappers; X5 deletes the legacy
  path along with append_sql / sql_buffer / sql_size / sql_capacity.

### Phase 2 — Migrate cases in transform_expression

For each `case AST_NODE_*` in transform_expression's switch (~30 cases):

- **X2.1** ✅ (2026-05-21): AST_NODE_LITERAL native dbuf emitter
  (`transform_literal_into` in cypher_transform.c). Handles
  INTEGER/DECIMAL/STRING/BOOLEAN/NULL with byte-identical output to
  the legacy switch case. `transform_expression_into` routes LITERAL
  AST nodes directly; everything else still falls through to the
  scratchpad. TCK unchanged at 3461.
- **X2.2** ✅ (2026-05-21): AST_NODE_PARAMETER native dbuf emitter
  (`transform_parameter_into`). Named `$x` → `:x` with
  `register_parameter` side effect; positional `$` → `?`.
  Byte-identical to legacy. TCK unchanged.
- **X2.2**: AST_NODE_IDENTIFIER — needs path-var, alias resolution.
- **X2.3**: AST_NODE_PROPERTY — needs sub-expression recurse.
- **X2.4**: AST_NODE_PARAMETER — straightforward.
- **X2.5**: AST_NODE_BINARY_OP — recurse on operands.
- **X2.6**: AST_NODE_FUNCTION_CALL — dispatch to migrated handlers
  (depends on Phase 3).
- **X2.7**: AST_NODE_LIST, AST_NODE_MAP — recurse on items.
- ... one task per case.

### Phase 3 — Migrate function-transform dispatch

- **X3.1**: Define `transform_func_into_handler` signature; add
  parallel `transform_func_into_table` in `transform_func_dispatch.c`.
- **X3.2–X3.16**: Migrate one transform_func_*.c file per task.
  Smallest first: graph (7), json (14), path (15), entity (20),
  typeconv (28), geo (38), list (38), aggregate (40), string (40),
  math (56), predicate (48), expr_ops (167), temporal (283).
- **X3.17**: Delete the old dispatch table.

### Phase 4 — Migrate expression callers

- **X4.1**: `transform_return.c` callers — already use
  `transform_expression_to_string`; switch to new API.
- **X4.2**: `transform_set.c` value-expression capture — use new API
  directly (no swap-and-restore needed).
- **X4.3**: `transform_with.c`, `transform_match.c` WHERE handling.
- **X4.4**: executor sites that call transform_expression on a fresh
  ctx (`executor_set.c`, `executor_call_subquery.c`).

### Phase 5 — Delete legacy

- **X5.1**: Confirm zero callers of `transform_expression`
  (old version), `append_sql`, `append_string_literal`.
- **X5.2**: Delete the old functions. Delete the deprecation
  attributes. Delete `ctx->sql_buffer` / `sql_size` / `sql_capacity`
  fields (now genuinely unused).
- **X5.3**: Delete `transform_expression_to_string` helper.
- **X5.4**: Delete the raw_output drain shim (now also unused) — its
  job is subsumed by `finalize_sql_generation` always running.

## Exit Criteria

- [ ] `grep -r 'append_sql\b\|append_string_literal\b' src/backend/`
      returns no matches.
- [ ] `cypher_transform_context` no longer has `sql_buffer`,
      `sql_size`, or `sql_capacity` fields.
- [ ] `transform_expression`'s only signature is the new
      `(ctx, expr, dbuf*)` form.
- [ ] All ~80 transform_func_* handlers follow the same new shape.
- [ ] TCK pass count ≥ baseline at initiative start.
- [ ] `angreal test unit && angreal test functional` clean.
- [ ] No deprecation warnings during build.

## Operational Notes

- **Depends on GQLITE-I-0042.** Without idempotent finalize and
  caller-side sequencing, the executor handlers can't cleanly hand
  expression contexts down to the new API.
- The migration is a multi-week effort. Each per-file migration commit
  should land separately with TCK-clean verification.
- Best executed via Ralph loops — each transform_func_* file is a
  natural unit, and the migration pattern is mechanical once Phase 1
  is in place.
- After this lands, the `sql_builder` layer has no remaining
  competitor — `unified_builder` is the only structured output, and
  `sql_buffer` is just a string field that holds the final assembled
  SQL for sqlite3_prepare_v2.
- Estimated effort: **2–4 weeks** of focused work. About 30 tasks if
  decomposed by phase + per-file batches.