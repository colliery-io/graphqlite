---
id: param-values-in-create
level: task
title: "$param values in CREATE relationship inline properties silently become NULL (bug #61.2)"
short_code: "GQLITE-T-0186"
created_at: 2026-04-18T16:54:20.342159+00:00
updated_at: 2026-04-18T19:57:57.044313+00:00
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

# $param values in CREATE relationship inline properties silently become NULL (bug #61.2)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 2)

## Objective

Persist `$param` values supplied to relationship inline properties in `CREATE (a)-[:REL {prop: $p}]->(b)` instead of silently storing NULL.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P0 - Critical (data loss: parameterized relationship properties silently NULL)

### Impact Assessment
- **Affected Users**: Every user using parameterized Cypher to write relationships — the standard Neo4j driver pattern.
- **Reproduction**:
  ```sql
  SELECT cypher('CREATE (a:N {node_id: "a"})');
  SELECT cypher('CREATE (b:N {node_id: "b"})');
  SELECT cypher('MATCH (a:N {node_id: "a"}) MATCH (b:N {node_id: "b"}) CREATE (a)-[:REL2 {prop1: $p1, prop2: $p2}]->(b)', '{"p1":"world","p2":99}');
  SELECT cypher('MATCH ()-[r:REL2]->() RETURN r.prop1, r.prop2');
  -- Actual:   [{"r.prop1":null,"r.prop2":null}]
  -- Expected: [{"r.prop1":"world","r.prop2":99}]
  ```
  Literal-value equivalent (`{prop1: "hello", prop2: 42}`) correctly persists — so the bug is specific to parameter substitution in REL property maps.
- **Expected vs Actual**: see above.

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**. Relationship is created but edge_props_text / edge_props_int rows are not written.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `CREATE (a)-[:REL {prop: $p}]->(b)` stores the bound `$p` value into the appropriate edge_props table.
- [ ] Works for all supported scalar types (text, int, real, bool).
- [ ] Literal-value relationship properties continue to work (regression).
- [ ] Parameter-bound **node** properties continue to work (regression — already passing).
- [ ] New regression tests cover text/int/real/bool parameters on relationship inline props.

## Suggested Remediation

The relationship-create path does not propagate parameter placeholders into the edge property INSERT. Likely sites:

- `src/backend/executor/query_dispatch.c` — CREATE relationship handler / `create_edge_properties` helper.
- `src/backend/transform/transform_create.c` (or the equivalent emitting REL property inserts).

Compare the working node-create path: node inline properties with `$param` are bound via `sqlite3_bind_*` on the prepared property-insert statement. The relationship path appears to either (a) not detect `AST_NODE_PARAMETER` and substitute NULL, or (b) build the INSERT without a binding call. The fix is to mirror the node logic: for each REL property whose value is an `AST_NODE_PARAMETER`, emit a placeholder and bind the parameter value at execution time.

Related tickets: GQLITE-T-0187 (ON CREATE SET $param on rel — likely same root cause).

## Status Updates

- 2026-04-18: Validated against current `main`. Relationship created; both properties NULL.
- 2026-04-18: **Fixed.** Root cause: `src/backend/executor/executor_create.c` relationship property loop (lines 417-468) handled only `AST_NODE_LITERAL` and `AST_NODE_MAP/LIST`; missed `AST_NODE_PARAMETER`. Node loop already had the handler. Patch mirrors the node param handler: calls `get_param_value()` into a `property_value`, maps to `prop_type/prop_value`, then calls `cypher_schema_set_edge_property()`. Refactored to put the `set_edge_property` call after the if/else chain (was inside the LITERAL branch) so LITERAL and PARAMETER share it.
- 2026-04-18: Regression test added to `tests/functional/39_issue_regression_tests.sql` (Issue #61.2 section): covers text/int/real/bool params. Verified: `[{"r.prop1":"world","r.prop2":99}]` and `[{"r.weight":3.14,"r.active":true}]`. Existing `26_parameterized_queries.sql` Test 7.2 now returns real value `r.since:2023` where before the suite would have accepted NULL — a symptom of the test-harness coverage gap tracked in GQLITE-I-0035.