---
id: 110-bindings-validate-identifiers
level: task
title: "#110: bindings validate identifiers (labels, keys, coordinate props) before interpolation"
short_code: "GQLITE-T-0349"
created_at: 2026-09-05T13:18:40.704706+00:00
updated_at: 2026-09-05T13:34:41.201261+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #110: bindings validate identifiers (labels, keys, coordinate props) before interpolation

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #110: labels, property keys, and astar coordinate property names are interpolated verbatim into Cypher. Add one `assert_identifier()` per binding enforcing `^[A-Za-z_][A-Za-z0-9_]*$` and apply it at every interpolation site before any query is built.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed.
- `upsert_node("k1", {"name: 'x', is_admin": True})` → properties `{'id':'k1','name':'x','is_admin':True}` (one key became two).
- `upsert_node("l1", {...}, label="Person:Admin")` → `labels(n) == ['Admin','Person']`.

Decision: relationship types are **not** switched to raising. They already pass through the public, tested `sanitize_rel_type` in every path (`edges.py`, `queries.py`, Rust `rel_type_pattern`), and #115 relies on that contract. Only raw-interpolated identifiers get the assertion.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Python `utils.assert_identifier` applied in `nodes.py` (label on create + `get_all_nodes`, keys on create + update), `edges.py` (keys in SET), `paths.py` (lat/lon props).
- [ ] Rust `utils::assert_identifier` returning `Error::InvalidIdentifier`, applied in `graph/nodes.rs`, `graph/edges.rs`, `algorithms/paths.rs`.
- [ ] Tests (both bindings): key with `:`/space/`-`, label with `:`, astar prop with `'` raise and leave the graph unchanged; identifiers with underscores and digits pass.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: `assert_identifier` added to `utils.py` (exported) and `utils.rs` (+`is_identifier`, exported; new `Error::InvalidIdentifier`). Applied: nodes (label, keys, get_all_nodes label), edges (keys), astar (lat/lon). Rel types deliberately keep `sanitize_rel_type`. Tests in both bindings, green.