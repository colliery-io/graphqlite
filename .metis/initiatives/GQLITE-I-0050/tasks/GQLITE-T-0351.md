---
id: 112-graphmanager-query-claims-auto
level: task
title: "#112: GraphManager.query claims auto-detection; make graphs required"
short_code: "GQLITE-T-0351"
created_at: 2026-09-05T13:18:42.956516+00:00
updated_at: 2026-09-05T13:18:42.956516+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #112: GraphManager.query claims auto-detection; make graphs required

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #112: docstrings and `python-api.md` claim graphs are auto-attached when `graphs` is omitted; no such code exists and the core fails with "no such table". Make `graphs` required, reject empty lists in the binding, and delete the auto-detection claims.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `gm.query("MATCH (n) FROM social RETURN n.name", graphs=None)` → `sqlite3.OperationalError: SQL prepare failed: no such table: social.nodes`; with `graphs=["social"]` → `[{'n.name': 'S1'}]`.

## Acceptance Criteria **[REQUIRED]**

- [ ] Python `GraphManager.query(cypher, graphs, params=None)`; empty/None → `ValueError("graphs is required")`.
- [ ] Rust `query` / `query_sql` return `Error::InvalidArgument` on an empty slice.
- [ ] Docstrings (both), `docs/src/reference/python-api.md`, `docs/src/reference/rust-api.md` updated.
- [ ] Python `test_manager_query_requires_graphs`; Rust same.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: Python `query(cypher, graphs, params=None)` required + ValueError on empty (also `query_sql`); shared `_attach` helper. Rust `Error::InvalidArgument` on empty slice for query/query_sql. Docstrings + python-api.md + rust-api.md fixed (rust-api.md signatures were also stale). Tests green.
