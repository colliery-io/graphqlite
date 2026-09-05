---
id: 116-core-return-structured-json
level: task
title: "#116: core return structured JSON stats for modification queries"
short_code: "GQLITE-T-0355"
created_at: 2026-09-05T13:18:48.976184+00:00
updated_at: 2026-09-05T13:34:47.172395+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #116: core return structured JSON stats for modification queries

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #116: replace the `Query executed successfully - nodes created: N, relationships created: M` string returned by `cypher()` for RETURN-less writes with a JSON object so bindings can branch on the first byte and deletes/property-sets are exposed.

Decision (issue asked for one): **option 1** — change the output in the 0.7.0 minor release with a changelog entry; no second function.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `CREATE (a:X)-[:R]->(b:X)` → `[{'result': 'Query executed successfully - nodes created: 2, relationships created: 1'}]`; DETACH DELETE and SET both report zeros. The executor already tracks `nodes_deleted`, `relationships_deleted`, `properties_set` (`cypher_executor.h:32-36`, used by the CLI in `main.c`).

Consumers found by grep: `src/extension.c:397`, `tests/test_output_format.c` (builds and asserts the string), `tests/tck/_extension_worker.py::_decode_payload` (a JSON dict without `error` is currently treated as a **data row** — must become status or TCK write scenarios will gain a phantom row), `docs/src/reference/sql-interface.md:74`. The CLI (`main.c`) prints its own text and is untouched. Neither binding scrapes the string today; both already turn a JSON object into a single row.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] `extension.c` emits `{"nodes_created":N,"relationships_created":N,"nodes_deleted":N,"relationships_deleted":N,"properties_set":N}`.
- [ ] `test_output_format.c` asserts the JSON shape (and its local helper mirrors it).
- [ ] TCK worker treats the stats object as `status`, not a row; TCK pass count unchanged.
- [ ] Functional SQL test asserting exact JSON for CREATE, MERGE, DELETE, DETACH DELETE, SET-only.
- [ ] Python `test_write_query_stats` and Rust `test_write_query_stats` assert parsed counts.
- [ ] `sql-interface.md` updated.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: implemented: `extension.c` emits the 5-key object; `test_output_format.c` helper mirrors it (plus the missing 'columns but zero rows → []' branch); TCK worker treats a dict with `nodes_created` as status. Also `delete_node_by_id` gained `int *detached_edges` so DETACH DELETE reports `relationships_deleted` (was always 0). CUnit 945/945; functional `37_write_stats.sql`; Python/Rust `test_write_query_stats` green. Runner note: this machine needs `SQLITE=/opt/local/bin/sqlite3` for functional tests because Android platform-tools' sqlite3 shadows PATH.