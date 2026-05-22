---
id: i-0042-architecture-split-dml
level: task
title: "I-0042 architecture: split DML/SELECT at builder boundary for MATCH+SET+WITH+RETURN"
short_code: "GQLITE-T-0310"
created_at: 2026-05-21T20:30:00.000000+00:00
updated_at: 2026-05-21T20:30:00.000000+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#tech-debt"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# Split DML / SELECT at the sql_builder boundary

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
