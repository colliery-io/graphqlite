---
id: 114-python-get-node-edges-returns
level: task
title: "#114: python get_node_edges returns tuples; standardize on dicts"
short_code: "GQLITE-T-0353"
created_at: 2026-09-05T13:18:45.961357+00:00
updated_at: 2026-09-05T13:18:45.961357+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #114: python get_node_edges returns tuples; standardize on dicts

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #114: `get_node_edges` returns `(source, target, r)` tuples while every sibling returns `{source, target, r}` dicts from the same query shape.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `get_node_edges("a")[0]` → tuple; `get_edges_from("a")[0]` → dict with `source`/`target`/`r`. `python-api.md` already documents `list[dict]`.

## Acceptance Criteria **[REQUIRED]**

- [ ] `get_node_edges` returns `result.to_list()`; annotation and docstring updated.
- [ ] `test_get_node_edges_shape`: rows are dicts with exactly `source`, `target`, `r`, and the set equals `get_edges_from + get_edges_to` for the node.
- [ ] Changelog notes the breaking change.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: `get_node_edges` returns `result.to_list()`; docstring/annotation/python-api.md updated. `test_get_node_edges_shape` green.
