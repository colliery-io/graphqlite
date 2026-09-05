---
id: 113-python-remove-dead-namespace
level: task
title: "#113: python remove dead namespace parameter from Graph and graph()"
short_code: "GQLITE-T-0352"
created_at: 2026-09-05T13:18:44.452269+00:00
updated_at: 2026-09-05T13:18:44.452269+00:00
parent: GQLITE-I-0050
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0050
---

# #113: python remove dead namespace parameter from Graph and graph()

## Parent Initiative

[[GQLITE-I-0050]]

## Objective **[REQUIRED]**

Close GitHub #113: remove the dead `namespace` parameter from `Graph.__init__`, `graph()`, the `_base.py` annotation, and the reference doc. It promises isolation it does not provide.

## Validation

Validated 2026-09-05 against a fresh `angreal build extension` build via the Python binding; behaviour reproduces exactly as filed. `graph(":memory:", namespace="x").namespace == "x"` and nothing else reads it (grep confirms only the three sites in the issue). Only the test suite references it (`test_graph_namespace`).

## Acceptance Criteria **[REQUIRED]**

- [ ] Signature becomes `Graph(db_path=":memory:", extension_path=None)`; same for `graph()`.
- [ ] `test_graph_rejects_namespace_kwarg`: `graph(":memory:", namespace="x")` raises `TypeError`; old `test_graph_namespace` removed.
- [ ] `python-api.md` constructor block and parameter table updated; changelog notes the positional signature change.

## Status Updates **[REQUIRED]**

- 2026-09-05: created, validated.
- 2026-09-05: namespace removed from `Graph.__init__`, `graph()`, `_base.py`, python-api.md. `test_graph_namespace` replaced by `test_graph_rejects_namespace_kwarg`. Green.
