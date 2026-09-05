---
id: 109-bindings-upsert-node-lets-node
level: task
title: "#109: bindings upsert_node lets node_data["id"] hijack node identity"
short_code: "GQLITE-T-0348"
created_at: 2026-09-05T13:18:39.198578+00:00
updated_at: 2026-09-05T13:34:56.334891+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #109: bindings upsert_node lets node_data["id"] hijack node identity

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #109: `upsert_node(node_id, node_data, label)` lets `node_data["id"]` override `node_id` on create (dict spread) and rename the node on update (`SET n.id = ...`).

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed.
- Create: `upsert_node("alice", {"id": "bob", ...})` → `has_node("alice") False`, `has_node("bob") True`.
- Update: `upsert_node("carol", {"id": "dave"})` after creating carol → carol gone, dave exists.
- `upsert_edge` checked: `id`/`startNode`/`endNode` in edge props land in `properties` and do **not** affect identity, so no edge change is needed.

## Acceptance Criteria

## Acceptance Criteria **[REQUIRED]**

- [ ] Python and Rust strip `id` from the property set on both create and update paths.
- [ ] Python `test_upsert_node_id_symmetry` (create + update); Rust same.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: `id` stripped from props on create+update in `nodes.py` and `nodes.rs`. Edge check confirmed no hijack (id/startNode/endNode land in `properties`), so upsert_edge unchanged. `test_upsert_node_id_symmetry` in both bindings, green.