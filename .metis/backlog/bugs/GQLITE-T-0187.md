---
id: on-create-set-with-param-on
level: task
title: "ON CREATE SET with $param on relationship variable does not persist (bug #61.3)"
short_code: "GQLITE-T-0187"
created_at: 2026-04-18T16:54:21.519693+00:00
updated_at: 2026-04-18T20:19:07.911196+00:00
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

# ON CREATE SET with $param on relationship variable does not persist (bug #61.3)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 3)

## Objective

Make `ON CREATE SET r.prop = $param` / `ON MATCH SET r.prop = $param` persist `$param` values on relationship variables, matching the already-working node behavior.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P0 - Critical (data loss: silently-NULL properties on MERGE)

### Impact Assessment
- **Affected Users**: Users relying on `MERGE ... ON CREATE SET` to idempotently attach relationship metadata (file/line, timestamps, weights, edge IDs).
- **Reproduction**:
  ```sql
  SELECT cypher('MATCH (a:N {node_id:"a"}) MATCH (b:N {node_id:"b"})
                 MERGE (a)-[r:CALLS {edge_id: $eid}]->(b)
                 ON CREATE SET r.file = $file, r.line = $line',
                '{"eid":"e1","file":"test.py","line":42}');
  SELECT cypher('MATCH ()-[r:CALLS]->() RETURN r.edge_id, r.file, r.line');
  -- Actual: [{"r.edge_id":null,"r.file":null,"r.line":null}]
  ```
  Node equivalent (`MERGE (n:Test {id:$id}) ON CREATE SET n.name = $name`) correctly persists.
- **Expected vs Actual**: all three properties should equal their bound parameter values; all are NULL.

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**. Relationship row created (1 edge); zero rows in edge_props_*.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `ON CREATE SET r.prop = $param` writes the parameter value into edge_props.
- [ ] `ON MATCH SET r.prop = $param` updates the parameter value on edge_props.
- [ ] Inline MERGE rel properties `MERGE (a)-[r:T {k: $v}]->(b)` also persist (shared with #61.2 / GQLITE-T-0186).
- [ ] Node `ON CREATE SET` with `$param` continues to work (regression).
- [ ] New regression tests.

## Suggested Remediation

Likely shares the root cause with GQLITE-T-0186 (bug #61.2): the relationship SET/UPDATE code path doesn't detect `AST_NODE_PARAMETER` as a value source, or detects it but doesn't bind. Fix together.

Investigation path:
- `src/backend/executor/query_dispatch.c` → MERGE dispatch → ON CREATE / ON MATCH branch → SET items.
- `src/backend/transform/transform_set.c` (if present) — SET-item transform for relationship variables.
- Compare the node SET path which correctly resolves `$param` → `sqlite3_bind_*` on the prepared edge_props UPSERT.

## Status Updates

- 2026-04-18: Validated on current `main`. Edge row written with no properties.
- 2026-04-18: **Fixed.** Two defects in `src/backend/executor/executor_merge.c`:
  1. MERGE rel inline props only handled `AST_NODE_LITERAL` (mirrored T-0186 defect in executor_create.c). Added `AST_NODE_PARAMETER` branch using `get_param_value()` — patched in both MERGE entry points (execute_merge_clause_with_vars ~line 724 and execute_merge_with_variables ~line 1360).
  2. ON CREATE/ON MATCH for edges was explicitly stubbed with `CYPHER_DEBUG("not yet implemented for relationship variables")` — replaced with `execute_set_items(executor, merge->on_create, var_map, result)`. The edge variable was already being registered via `set_variable_edge_id()`, and `execute_set_items` already supports edge variables via `is_variable_edge()` branching — so the stub was gratuitous. Also broadened the trigger: ON CREATE now fires when the MERGE as a whole created a new edge OR a new target endpoint.
- 2026-04-18: Regression test added to `tests/functional/39_issue_regression_tests.sql` (Issue #61.3 section). Verified: `[{"r.edge_id":"e1","r.file":"test.py","r.line":42}]`. Full functional suite re-run: no hard failures.