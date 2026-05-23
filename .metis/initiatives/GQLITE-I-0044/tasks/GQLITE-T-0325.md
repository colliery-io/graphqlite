---
id: a1-graph3-labels-returns-null-on
level: task
title: "A1: Graph3 labels() returns NULL on no-label nodes — should be empty list"
short_code: "GQLITE-T-0325"
created_at: 2026-05-23T10:53:12.837997+00:00
updated_at: 2026-05-23T11:03:21.432552+00:00
parent: GQLITE-I-0044
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0044
---

# A1: labels() returns NULL on no-label nodes — should be empty list

## Parent Initiative

[[GQLITE-I-0044]] — Phase A quick win #1.

## Objective

`labels(n)` on a node with no labels returns `null` instead of the
empty list `[]` that Cypher spec requires.

## Repro

```cypher
CREATE (node) RETURN labels(node);
```

**Expected:** `[{"labels(node)": []}]`
**Actual:** `[{"?column?": null}]` (column name wrong too — but
the value-NULL is the primary bug).

## Affected scenarios

- Graph3 [1] Creating node without label.
- Graph3 [2] Creating node with two labels (probably column name
  issue).
- Graph3 [3] Ignore space when creating node with labels.
- Graph3 [4] Create node with label in pattern.
- Graph3 [6] `labels()` should accept type Any.

Estimated **+5 TCK**.

## Implementation plan

1. Find `labels()` emission in transform layer:
   `transform_func_entity.c::transform_labels_function` (most
   likely) or wherever the function dispatcher routes `labels`.
2. Inspect the SQL emission. Likely shape:
   ```sql
   (SELECT json_group_array(label) FROM node_labels WHERE node_id = X)
   ```
   When `node_labels` has zero rows for `X`, `json_group_array`
   returns NULL.
3. Wrap with `COALESCE(..., json('[]'))` so empty rows produce
   `[]`.
4. Confirm column name: ensure the RETURN projection names the
   column `labels(node)` not `?column?`. May be a separate
   alias-emission bug.
5. Run the Graph3 family + the wider TCK to confirm no regressions
   on currently-passing labels() usages.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Graph3 [1]/[2]/[3]/[4]/[6] pass.
- [ ] `MATCH (n) RETURN labels(n)` returns `[]` for nodes with no
  labels and `["A", "B"]` (or similar) for labeled nodes.
- [ ] Functional regression test added in `tests/functional/`.
- [ ] No regression on existing labels() scenarios.
- [ ] `angreal test unit && angreal test functional` clean.

## Affected files

- `src/backend/transform/transform_func_entity.c` (primary).
- Possibly `src/backend/transform/transform_func_dispatch.c`
  (if registered there) or `transform_return.c` (if column
  naming is the secondary issue).

## Effort

S — single function change + tests.

## Status Updates

### 2026-05-23 — Completed

Two-part fix:

1. **`transform_func_entity.c::transform_labels_function`** — wrapped
   the inner subquery with `COALESCE(..., json('[]'))` so a non-NULL
   node with zero labels produces `[]` instead of NULL. The outer
   CASE for NULL-node-id preservation is intact.

2. **`query_dispatch.c::handle_create_return`** — the CREATE+RETURN
   fast-path only handled AST_NODE_IDENTIFIER and AST_NODE_PROPERTY
   return items; function calls fell through with `?column?: null`.
   Added AST_NODE_FUNCTION_CALL handling for the common entity
   functions: `labels()`, `id()`, `keys()`. Uses one-shot SQL
   queries against the proper tables.

The fast-path can't easily delegate to the generic transform
pipeline because that pipeline doesn't support CREATE+RETURN yet
("RETURN after CREATE not yet implemented"). Adding labels/id/keys
directly to the fast-path is the contained fix.

**TCK: 3549 → 3553 (+4).** All 4 Graph3 scenarios pass:
- Graph3 [1] Creating node without label
- Graph3 [2] Creating node with two labels
- Graph3 [3] Ignore space when creating node with labels
- Graph3 [4] Create node with label in pattern

Graph3 [6] (`labels()` should accept type Any) still fails — it's
a different shape that goes through transform's labels() handler
and hits the "argument must be a node variable" early-exit. That's
a separate fix.

944/944 unit, functional clean. 0 regressions.