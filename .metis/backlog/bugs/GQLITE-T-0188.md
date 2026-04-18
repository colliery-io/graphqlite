---
id: set-node-map-doesn-t-apply-map
level: task
title: "SET node += map doesn't apply map properties (bug #61.4)"
short_code: "GQLITE-T-0188"
created_at: 2026-04-18T16:54:23.029615+00:00
updated_at: 2026-04-18T22:43:14.237659+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# SET node += map doesn't apply map properties (bug #61.4)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 4)

## Objective

Implement Cypher map-merge semantics for `SET node += {k: v, ...}` so that the map's entries are applied as property updates to the node.

## Backlog Item Details

### Type
- [x] Bug (missing spec feature surfacing as silent data loss)

### Priority
- [x] P1 - High

### Impact Assessment
- **Affected Users**: Users migrating from Neo4j that rely on `+=` to merge property maps (also common for partial updates in parameterized ingestion: `SET n += $props`).
- **Reproduction**:
  ```sql
  SELECT cypher('MERGE (n:Test {node_id: "a"}) SET n += {name: "alice", type: "person"}');
  SELECT cypher('MATCH (n:Test) RETURN n.node_id, n.name, n.type');
  -- Actual:   [{"n.node_id":"a","n.name":null,"n.type":null}]
  -- Expected: [{"n.node_id":"a","n.name":"alice","n.type":"person"}]
  ```
- **Expected vs Actual**: `+=` should iterate the map's entries and upsert each as a property; currently the map operator is either parsed but treated as no-op, or the map-merge SET item is not dispatched.

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**. No parse/runtime error — properties silently NULL.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `SET n += {a: 1, b: 2}` adds/overwrites properties `a` and `b`, leaves others intact (merge semantics).
- [ ] `SET n = {a: 1, b: 2}` (if supported) replaces all properties (strict semantics — verify separately).
- [ ] `SET n += $map` works with parameterized map values.
- [ ] Applies equivalently to relationship variables: `SET r += {...}`.
- [ ] Spec coverage matrix updated (§ SET map-merge).
- [ ] Regression tests for literal maps, parameterized maps, and relationship variables.

## Suggested Remediation

Verify the grammar accepts `+=` in SET (likely yes — no parse error observed), then extend the transform:

- Grammar / AST: confirm there's a distinct `SET_ITEM` variant for map-merge (e.g. `AST_SET_OP_MERGE` vs `AST_SET_OP_ASSIGN`). If missing, add it.
- Transform: in the SET-item transform, when the operator is `+=` and the RHS is an `AST_NODE_MAP` (or a parameter resolving to a map), iterate the entries and emit the same per-property UPSERT statements that `SET n.k = v` uses today.
- Reuse: the literal-map handling in node CREATE already iterates map entries to produce property inserts — refactor that into a shared helper callable from both CREATE and SET+=.

Likely files:
- `src/backend/parser/cypher_gram.y` — confirm `+=` production.
- `src/backend/transform/transform_set.c` — add map-merge dispatch.
- `src/backend/ast/cypher_ast.c` — add/verify SET_ITEM op enum.

## Status Updates

- 2026-04-18: Validated on current `main`. Query succeeds; properties NULL.
- 2026-04-18: Root cause identified — **not** a missing feature in `SET +=` handling. The bulk-SET block in `src/backend/executor/executor_set.c:573-708` already correctly iterates map pairs and calls `cypher_schema_set_node_property` for each. The code works when reached. The real defect is the **dispatcher never reaches it** for `CREATE ... SET ...` or `MERGE ... SET ...` (standalone SET, no ON CREATE). `query_dispatch.c::handle_create` and `handle_merge` invoke `execute_create_clause`/`execute_merge_clause` only — SET is silently dropped. `execute_set_clause` (standalone SET) errors out with "requires MATCH to bind variables".
- 2026-04-18: Also confirmed the non-`+=` variant is broken: `CREATE (n:Test {node_id:"a"}) SET n.name = "alice"` also returns `n.name=null`. Single code path.
- 2026-04-18: **Blocked** pending dispatcher-level fix. Required changes:
  - Add `CREATE+SET` and `MERGE+SET` entries to the pattern dispatch table in `query_dispatch.c`, OR extend `handle_create`/`handle_merge` to call `execute_set_operations(executor, set, varmap_from_write, result)` when a SET clause is present.
  - `execute_create_clause_with_varmap` already exists (used in UNWIND+CREATE+SET at line 1471). An equivalent `execute_merge_clause_with_varmap` would need to expose the write-clause's `variable_map`. The UNWIND+CREATE+SET path is a usable model.
  - Once plumbing lands, this bug and GQLITE-T-0189 are both resolved without any changes to the SET code itself.