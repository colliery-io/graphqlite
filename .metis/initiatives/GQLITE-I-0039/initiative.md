---
id: sql-generation-consolidation
level: initiative
title: "SQL Generation Consolidation — collapse three APIs into one"
short_code: "GQLITE-I-0039"
created_at: 2026-05-19T14:17:49.434969+00:00
updated_at: 2026-05-19T14:17:49.434969+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/discovery"


exit_criteria_met: false
estimated_complexity: M
initiative_id: sql-generation-consolidation
---

# SQL Generation Consolidation — collapse three APIs into one

## Context

The transform layer in `src/backend/transform/` currently emits SQL through
**three coexisting APIs**, the result of a stalled migration:

1. **Legacy `append_sql(ctx, fmt, ...)`** in
   `src/backend/transform/cypher_transform.c:132` — printf-style buffer growth
   into `cypher_transform_context.sql_buffer`. ~100+ direct call sites
   system-wide; ~432 calls in `transform_func_list.c` alone and ~104 in
   `transform_return.c`.
2. **New `sql_builder` API** in `src/backend/transform/sql_builder.c` (910 LOC)
   with `dbuf_appendf` / typed clause sections. The canonical direction, but
   adoption is partial.
3. **Hybrid `pending_prop_joins`** buffer
   (`cypher_transform_context.pending_prop_joins`, declared at
   `cypher_transform.h:58-60`) — manually concatenated and then prepended via
   `prepend_cte_to_sql()` at `cypher_transform.c:284-307`. Reimplements what
   `sql_builder` already does for CTEs.

In addition, `cypher_transform_context` (`cypher_transform.h:21-75`) carries
all three buffers as raw top-level fields. The struct is becoming a
god-object; the SQL-output portion is the most painful slice.

Source: architectural review of 2026-05-19 (Risk 2 + the SQL portion of
Risk 3). This is the highest-leverage mechanical fix identified.

## Goals

- **G1**: Delete `append_sql`, `append_identifier`, `append_string_literal`
  from `cypher_transform.c`. All transform files emit SQL through
  `sql_builder` only.
- **G2**: Fold `pending_prop_joins` into `sql_builder` as a typed CTE-prepend
  section. Delete the standalone buffer and `prepend_cte_to_sql` helper.
- **G3**: Extract a `transform_output` sub-struct from
  `cypher_transform_context` containing the `sql_builder *` and any remaining
  output state. The context no longer exposes raw SQL buffers as top-level
  fields.
- **G4**: Zero regression — TCK pass count unchanged, all unit tests pass, all
  functional tests pass.
- **G5**: After landing, new code physically cannot use the legacy API — it no
  longer exists in the header.

## Non-Goals

- Changing SQL semantics. Pure refactor; emitted SQL is byte-equivalent or
  semantically equivalent.
- Performance optimization. Same algorithmic cost, same allocation count
  approximately.
- Other `cypher_transform_context` cleanup — alias counters, mode flags,
  `query_type`, etc. (see Risk 3 of the review). Deferred.
- Renaming or changing signatures inside `sql_builder` itself. Treat it as
  the target, not under refactor.
- Python/Rust binding changes.

## Detailed Design

### Current shape

```c
/* cypher_transform.h:28-31 */
char *sql_buffer;               /* Generated SQL query */
size_t sql_size;
size_t sql_capacity;

/* cypher_transform.h:58-60 */
char *pending_prop_joins;
size_t pending_prop_joins_len;
size_t pending_prop_joins_cap;

/* cypher_transform.h:74 */
sql_builder *unified_builder;
```

Three independent stores. Code mixes appends to `sql_buffer` (via
`append_sql`), writes to `pending_prop_joins` (via `add_pending_prop_join`),
and writes to `unified_builder` (via `dbuf_appendf`). At finalize time,
`prepend_cte_to_sql` splices them.

### Target shape

```c
/* New: transform_output sub-struct */
typedef struct {
    sql_builder *builder;     /* sole SQL output */
    /* any remaining output-only state lives here */
} transform_output;

/* cypher_transform_context (after) */
transform_output *output;
```

`sql_builder` gains a typed CTE-prepend section (it already has clause
sections — SELECT, FROM, WHERE — but pending property joins are a special
CTE-prepend case). After this change, `sql_builder` is the single source of
truth.

### API capability gap

Before starting Phase 3 migrations, do a capability gap analysis: every
`append_sql()` call site emits some shape (raw string, format args, into a
logical position). Some shapes may not yet be covered by `sql_builder`. The
gap analysis (S1–S3 below) is the prerequisite for all later phases.

### Migration order

Migrate by clause-type file so that each PR is reviewable in isolation and
revertible without unwinding others. Order is chosen by call-site count and
risk:

- **Lowest risk first**: small files with simple append patterns
  (`transform_set.c`, `transform_delete.c`, `transform_remove.c`).
- **Medium**: `transform_match.c`, `transform_with.c`, `transform_unwind.c`,
  `transform_foreach.c`, `transform_create.c`.
- **Heavy**: `transform_return.c` (104 calls), `transform_func_list.c` (432
  calls), the rest of `transform_func_*.c` family, then `transform_expr_*.c`.

Each migration commit must touch only the named file plus header consumers,
pass `angreal test unit` and `angreal test functional` clean, and show zero
scenario delta on `angreal test tck`.

## Alternatives Considered

- **Keep `append_sql` as a thin shim over `sql_builder`.** Rejected —
  preserves the dual mental model and lets new code keep adopting the legacy
  idiom. Deletion is the only path that finishes the migration.
- **Big-bang rewrite in one PR.** Rejected — a several-thousand-line diff
  touching every transform file is unreviewable; one bad merge wipes the
  work. Per-file migration with regression-clean commits is the safer trade.
- **Defer until after TCK conformance work is done.** Rejected — SQL-writing
  churn collides directly with TCK fix patches in the same files. Doing this
  *first* makes TCK work easier; doing it *after* means every TCK fix has to
  be rewritten on the new API.
- **Leave `pending_prop_joins` as-is; only burn down `append_sql`.** Rejected
  — `pending_prop_joins` is half the problem. Without folding it into
  `sql_builder`, the context still has dual SQL state and a future migrator
  hits the same dead end.

## Implementation Plan

### Phase 1 — Inventory & instrument

- **S1**: Catalog every `append_sql` / `append_identifier` /
  `append_string_literal` call site. Per-file count, shape (format-string +
  arg types), logical position. Output:
  `docs/internal/sql-migration-inventory.md`.
- **S2**: Catalog every `pending_prop_joins` write and the single
  `prepend_cte_to_sql` invocation. Document the prepend invariant.
- **S3**: Capability gap analysis — for each unique shape in S1/S2, name the
  `sql_builder` API that handles it, or flag as "gap → need new API". Extend
  the inventory doc.

### Phase 2 — Bridge

- **S4**: Add `__attribute__((deprecated("use sql_builder")))` to
  `append_sql`/`append_identifier`/`append_string_literal` so new uses warn
  at compile time. Add gap-filling APIs to `sql_builder` if S3 identified
  any.

### Phase 3 — Migrate by clause type

Each task migrates one or two clause/function files. Commit per task.
Verification: unit + functional clean, TCK delta = 0.

- **S5**: Migrate `transform_set.c`, `transform_delete.c`,
  `transform_remove.c` (smallest call-site counts).
- **S6**: Migrate `transform_create.c`, `transform_foreach.c`,
  `transform_unwind.c`.
- **S7**: Migrate `transform_match.c` (high call count, MATCH is hot path).
- **S8**: Migrate `transform_with.c`.
- **S9**: Migrate `transform_return.c`.
- **S10**: Migrate `transform_expr_ops.c`, `transform_expr_predicate.c`.
- **S11**: Migrate `transform_func_string.c`, `transform_func_math.c`,
  `transform_func_path.c`, `transform_func_entity.c`,
  `transform_func_aggregate.c`, `transform_func_graph.c`,
  `transform_func_dispatch.c`.
- **S12**: Migrate `transform_func_list.c` (largest single file).

### Phase 4 — Fold pending_prop_joins

- **S13**: Add typed "pending CTE / prepend" section to `sql_builder` API.
- **S14**: Migrate `add_pending_prop_join` callers to use it.
- **S15**: Delete `pending_prop_joins` fields from context. Delete
  `prepend_cte_to_sql`, `add_pending_prop_join`, `get_pending_prop_joins`,
  `reset_pending_prop_joins`.

### Phase 5 — Context output sub-struct

- **S16**: Define `transform_output` struct. Move `unified_builder` into it.
  Update all callers (most go through `ctx->output->builder` instead of
  `ctx->unified_builder`).
- **S17**: Wire lifecycle — `output` allocated in
  `cypher_transform_create_context`, freed in `cypher_transform_free_context`.

### Phase 6 — Delete legacy

- **S18**: Delete `append_sql`, `append_identifier`, `append_string_literal`
  function definitions from `cypher_transform.c`. Delete declarations from
  `cypher_transform.h`. Delete `sql_buffer`/`sql_size`/`sql_capacity` from
  context.
- **S19**: Full TCK regression run. Compare `baseline.json` before/after —
  pass set should be byte-identical.

## Exit Criteria

- [ ] `grep -r 'append_sql\b\|append_identifier\b\|append_string_literal\b' src/backend/`
      returns no matches.
- [ ] `grep -r 'pending_prop_joins\|prepend_cte_to_sql' src/backend/` returns
      no matches.
- [ ] `cypher_transform_context` no longer has `sql_buffer`,
      `pending_prop_joins`, or `unified_builder` as top-level fields
      (replaced by `transform_output *output`).
- [ ] `angreal test tck` shows zero scenario delta vs. baseline at initiative
      start.
- [ ] `angreal test unit && angreal test functional` clean.
- [ ] All 19 child tasks (S1–S19) closed.

## Operational Notes

- Each migration task is small and reviewable. The Ralph loop can knock most
  of Phase 3 down autonomously once S1–S4 land.
- Keep migration commits surgical: one file at a time, no opportunistic
  refactors. Save those for a follow-up.
- The TCK pass set is the ground truth. Re-run after every migration commit
  — a single byte-level SQL change can shift scenarios.
- Related initiatives: GQLITE-I-0040 (god-file splits) should ideally land
  its Phase 3 (`transform_func_list.c` split) before S12 of this initiative,
  so the migration target is four ~500-LOC files instead of one 1,979-LOC
  file.
