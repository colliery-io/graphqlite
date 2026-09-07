---
id: 001-table-valued-result-interface
level: adr
title: "Table-valued result interface (cypher_rows) to remove result amplification"
number: 1
short_code: "GQLITE-A-0006"
created_at: 2026-09-07T11:19:44.035854+00:00
updated_at: 2026-09-07T11:19:44.035854+00:00
decision_date: 
decision_maker: 
parent: 
archived: false

tags:
  - "#adr"
  - "#phase/draft"


exit_criteria_met: false
initiative_id: NULL
---

# ADR-1: Table-valued result interface (cypher_rows) to remove result amplification
## Context **[REQUIRED]**

`cypher()` is a scalar SQL function: every result set leaves the engine as one JSON string. Measured in the performance review (Metis GQLITE-I-0051, finding 10): a 6.6 MB `MATCH (n) RETURN n` result over 50K nodes peaks at +44 MB, a 250K-row three-column projection at +83 MB. Each value is copied 6-8 times: SQLite materialises the row text, `build_query_results` strdup()s every cell, the extension assembles one output buffer, `sqlite3_result_text(..., SQLITE_TRANSIENT)` copies it again, the host language creates a string, and the binding parses JSON. Phase 2 (F5) removed the agtype tree and the double serialisation; the remaining copies are inherent to the scalar interface. Callers also cannot page without re-running the query (`LIMIT` inside Cypher re-executes everything), and column types are lost (everything is JSON text on the wire).

## Decision **[REQUIRED]**

Add an eponymous virtual table `cypher_rows` so a Cypher query can be consumed as SQL rows:

```sql
SELECT * FROM cypher_rows('MATCH (n:Person) WHERE n.age > 30 RETURN n.name, n.age');
SELECT name, age FROM cypher_rows('MATCH (n) RETURN n.name AS name, n.age AS age', '{"limit": 10}') LIMIT 100;
```

- Arguments: the Cypher text and an optional params JSON, exactly as `cypher()` takes them (hidden columns `query` and `params` on the virtual table).
- Columns: one SQLite column per RETURN item, named by the item's alias or expression text, declared dynamically via `sqlite3_declare_vtab` at `xConnect`/`xFilter` time from the transform's column metadata. Scalars keep native types (INTEGER/REAL/TEXT/NULL, booleans as INTEGER with the existing subtype tag); nodes, relationships, paths, lists and maps are emitted as JSON text (the same shape `cypher()` emits inside its arrays).
- Streaming: `xFilter` parses/transforms/prepares (reusing the F8 statement cache by text), `xNext` steps the underlying SQLite statement one row at a time, `xColumn` hands the current row's values straight from the inner `sqlite3_value`s via `sqlite3_result_value` (zero-copy) or the pass-through JSON text. Peak memory is one row.
- Writes: `cypher_rows` is read-only. A write query (no RETURN) yields one row with the five statistics columns, so scripts can use one entry point; write queries with RETURN behave like reads.
- `cypher()` remains, implemented as it is today, as the compatibility surface; the bindings gain `iter_rows()` / `Connection.rows()` APIs over `cypher_rows` and keep `cypher()` for existing callers.

## Alternatives Analysis **[CONDITIONAL: Complex Decision]**

| Option | Pros | Cons | Risk Level | Implementation Cost |
|--------|------|------|------------|-------------------|
| Table-valued function `cypher_rows` (chosen) | One row in memory at a time; native types; SQL composability (`LIMIT`, joins, `json_each`); no change to `cypher()` | Column set is only known after transform, so `xBestIndex` must accept the query text as a hidden-column constraint and columns are declared per instance; virtual-table cursor lifetime rules | Medium | Medium-large (new module ~1-1.5K lines, binding wrappers, docs) |
| Streaming JSON via `sqlite3_result_text` with a callback / chunked output | No new SQL surface | SQLite scalar functions cannot stream; would still buffer the whole string | High | n/a (not feasible) |
| Shrink the scalar path further (flat allocations, size once) | Small, local | Bounded at ~3x amplification by the scalar contract; no paging, no types | Low | Small (do anyway as a follow-up) |
| Native binding hooks (Python/Rust call the executor directly and build objects) | Best possible per-language performance | Duplicates the engine's result layer per binding; bypasses SQL entirely; large surface to keep in parity | High | Large |

## Rationale **[REQUIRED]**

The scalar function's contract ("one string") is the amplification. A virtual table is the SQLite-native way to return rows from an extension, composes with the rest of SQL, keeps peak memory at one row, and lets callers page without re-running the Cypher. The F8 statement cache already keeps the parse/transform/prepare cost off the hot path, so `xFilter` can be cheap. Keeping `cypher()` untouched means no compatibility break: the new interface is additive and the bindings can adopt it incrementally.

## Consequences **[REQUIRED]**

### Positive
- Peak memory for large results drops from O(result) to O(row); the 44 MB / 83 MB peaks in the review become a few hundred KB.
- Native column types on the wire; no JSON encode/decode for scalar projections.
- SQL-level paging, joining and filtering over Cypher results.

### Negative
- Two ways to run a query; documentation must present `cypher_rows` as the recommended path for large results and `cypher()` for compatibility and single values.
- Dynamic column declaration is more intricate than a fixed schema; every RETURN item must map to a valid SQLite column name (aliases are sanitised the same way the transform sanitises SQL aliases).
- Entity values remain JSON text inside a column; a typed entity representation is out of scope.

### Neutral
- The scalar path's remaining copies (`result->data` per-row arrays, `SQLITE_TRANSIENT`) can still be trimmed independently; tracked as follow-ups under GQLITE-I-0051.

## Implementation notes (for the follow-up task)

1. `src/backend/runtime/cypher_rows_vtab.c`: `sqlite3_module` with `xConnect` (eponymous), `xBestIndex` (require `query` hidden column `=` constraint, optional `params`), `xFilter` (executor lookup via the connection cache, F8 statement cache), `xNext`/`xEof`/`xColumn`/`xRowid`, `xClose`.
2. Reuse `build_query_results`' per-cell logic as a per-row function (`render_cell`) so `cypher()` and `cypher_rows` share one value-rendering path.
3. Register in `sqlite3_graphqlite_init` next to `cypher()`.
4. Bindings: Python `Connection.iter_rows(query, params)` yielding dicts; Rust `Connection::rows(query)` returning an iterator of `Row`.
5. Tests: functional SQL (`SELECT ... FROM cypher_rows(...)` with LIMIT/joins), CUnit for the module, Python/Rust wrappers, and a harness measurement replacing the review's finding-10 table.
