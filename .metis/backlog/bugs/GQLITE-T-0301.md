---
id: rust-binding-engine-regressions
level: task
title: "Engine regressions surfaced by Rust binding tests (7 #[ignore]'d cases)"
short_code: "GQLITE-T-0301"
created_at: 2026-05-19T18:30:00.000000+00:00
updated_at: 2026-05-19T18:30:00.000000+00:00
parent:
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#bug"


exit_criteria_met: false
initiative_id: NULL
---

# Engine regressions surfaced by Rust binding tests (7 #[ignore]'d cases)

## Objective

During the 187-commit openCypher conformance push, seven Rust binding
integration tests started failing. They were marked `#[ignore = "engine
regression: <why> (T-0301)"]` to unblock the merge that landed the TCK
conformance gate (PR #67); this ticket tracks unblocking each one with
a targeted engine fix.

## Backlog Item Details

### Type
- [x] Bug — Production issue that needs fixing

### Priority
- [x] P1 — High (user-facing binding contract slipped)

### Impact Assessment
- **Affected users**: Rust binding consumers using CALL subqueries,
  WITH-through edge vars, function calls in CREATE property maps,
  COUNT-over-OPTIONAL-MATCH, or datetime accessors.
- **Reproduction**: re-run `cargo test --manifest-path bindings/rust/Cargo.toml --test integration -- --ignored`.

## Failing Tests

| Test | Symptom | Likely root cause |
|---|---|---|
| `test_count_skips_nulls_from_optional_match` | `COUNT(r)` over OPTIONAL MATCH NULL returns 1 instead of 0. | Aggregate not skipping NULL bindings. |
| `test_edge_variable_through_with` | `MATCH (a)-[r]->(b) WITH a, b, r RETURN r.weight` errors: `no such column: _with_0.r.id`. | WITH-projection alias bookkeeping drops the edge's id column. |
| `test_function_call_in_create_property_map` | `CREATE (n {upper: toUpper('hello')})` stores empty string. | Function-call evaluation in CREATE property-map writes happen pre-evaluation. |
| `test_call_subquery_exports_inner_return` | `CALL { WITH a RETURN a.id AS inner_id } RETURN a.id AS outer_id, inner_id` → `Unknown variable: inner_id`. | CALL subquery doesn't export inner RETURN aliases to outer scope. |
| `test_call_subquery_processes_all_inner_match_rows` | `CALL { WITH c MATCH (d) MERGE (c)-[:R]->(d) }` iterates only 1 inner row. | CALL invocation per outer row not iterating inner row-set. |
| `test_datetime_from_epoch` | macOS returns `"1970-01-01 00:00:00"`, Linux returns `"1970-01-01T00:00:00Z"`. | Platform-dependent `strftime` format string. |
| `test_datetime_map_construction` | macOS returns `"2024-06-15T10:30:00"`, Linux returns `"2024-06-15T10:30Z"`. | Same root cause. |

## Acceptance Criteria

- [ ] Each test passes with `#[ignore]` removed.
- [ ] No regressions in TCK baseline (`scripts/check-tck-baseline.sh`).
- [ ] Datetime formatting is byte-identical across macOS and Linux.

## Implementation Notes

The datetime pair (last two) likely shares a single fix in
`src/extension.c` temporal UDFs — they're already targeted by I-0040 M14
(temporal ISO-week math) and may resolve together. The other five are
independent engine bugs touching aggregation, WITH bookkeeping, CREATE
property-map evaluation, and CALL subquery row semantics.

## Status Updates

*To be added during implementation*
