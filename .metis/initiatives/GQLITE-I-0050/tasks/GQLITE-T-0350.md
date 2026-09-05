---
id: 111-graphmanager-accepts-path
level: task
title: "#111: GraphManager accepts path traversal in graph names"
short_code: "GQLITE-T-0350"
created_at: 2026-09-05T13:18:42.200672+00:00
updated_at: 2026-09-05T13:34:41.906697+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #111: GraphManager accepts path traversal in graph names

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #111: `GraphManager._graph_path(name)` is `base_path / f"{name}.db"` with no validation, so `../x` escapes the directory for create/open/drop/query, and the name is also interpolated into `ATTACH DATABASE ... AS {name}`.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `graphs(tmp).create("../escaped")` created `<parent>/escaped.db` (`os.path.exists` → True). Rust `graph_path` has the identical construction.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Python: `_graph_path` validates with the identifier rule and asserts the resolved path is under `base_path`; raises `ValueError` before touching the filesystem. Covers create/open/drop/exists/query/query_sql.
- [ ] Rust: `graph_path` returns `Result<PathBuf>` with `Error::InvalidGraphName`; `exists()` returns false for invalid names.
- [ ] Python `test_manager_rejects_traversal` (`../x`, `a/b`, `""` raise; no file created) and `test_manager_drop_rejects_traversal`; Rust equivalents.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: `_graph_path`/`graph_path` validate identifier + parent==base; Python raises ValueError, Rust `Error::InvalidGraphName`; `exists()` returns False for invalid names. Traversal + drop tests in both bindings, green.