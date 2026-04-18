---
id: target-node-properties
level: task
title: "Target node properties inaccessible in pattern match traversals (bug #61.6)"
short_code: "GQLITE-T-0190"
created_at: 2026-04-18T16:54:25.738054+00:00
updated_at: 2026-04-18T22:43:16.143218+00:00
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

# Target node properties inaccessible in pattern match traversals (bug #61.6)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 6)

## Objective

Return correct target-node property values when traversing a relationship pattern `(src)-[r]->(tgt)` — currently `tgt.*` returns NULL for every property even when the target node has valid properties.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P0 - Critical (breaks the fundamental graph traversal read pattern)

### Impact Assessment
- **Affected Users**: Every user running traversal queries. This is the most common read pattern in any graph application.
- **Reproduction**:
  ```sql
  SELECT cypher('CREATE (a:N {node_id:"a", name:"alice"})');
  SELECT cypher('CREATE (b:N {node_id:"b", name:"bob"})');
  SELECT cypher('MATCH (a:N {node_id:"a"}) MATCH (b:N {node_id:"b"})
                 CREATE (a)-[:CALLS {edge_id:"e1"}]->(b)');
  SELECT cypher('MATCH (src:N)-[r:CALLS]->(tgt)
                 RETURN src.name AS from_name, tgt.name AS to_name, tgt.node_id AS to_id');
  -- Actual:   [{"from_name":"alice","to_name":null,"to_id":null}]
  -- Expected: [{"from_name":"alice","to_name":"bob","to_id":"b"}]
  ```
- **Expected vs Actual**: properties on `tgt` should resolve via the same node_props_* joins as `src`; they currently return NULL.

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**. Likely cascades to GQLITE-T-0191 (bug #61.7: empty-result traversal) as a sub-symptom.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (s)-[r]->(t) RETURN s.k, t.k` returns both `s.k` and `t.k` correctly.
- [ ] Works for all scalar property types (text/int/real/bool).
- [ ] Works for unbound source and/or unbound target (e.g., `MATCH ()-[r]->(t) RETURN t.*`).
- [ ] Works for multiple-hop patterns `(a)-[r1]->(b)-[r2]->(c)` — every variable's properties are accessible.
- [ ] Regression tests; add coverage of the specific `src.x, tgt.x` read pattern across the functional test suite.

## Suggested Remediation

The transform almost certainly emits the `node_props_*` LEFT JOIN for the source node but not for the target — or it emits the join but the SELECT expression uses the wrong alias. Investigation path:

- `src/backend/transform/transform_match.c` / `transform_return.c` — check whether both endpoints of a REL_PATTERN register an alias in `var_ctx` and whether `transform_property_access()` generates a join for both.
- `transform_property_access()` in `transform_expr_ops.c` — confirm it's invoked for `tgt.name` and returns a reference to a joined `_prop_<alias>` table, not to a column on the edges table.
- The alias generator (`_gql_default_alias_N`) may be colliding between endpoints — compare with GQLITE-T-0181 (startNode/endNode alias collision) and GQLITE-T-0184 (alias sanitation).

This bug likely explains why GQLITE-T-0191 (bug #61.7) returns `[]`: `pkg.name` resolves to NULL, and the `ORDER BY pkg.name` + DISTINCT path then filters everything out.

## Status Updates

- 2026-04-18: Validated on current `main`. `src.name` resolves correctly; `tgt.*` always NULL.
- 2026-04-18: **Root cause is not in the traversal code.** The traversal SQL is correct — `EXPLAIN MATCH (src)-[r]->(tgt) RETURN tgt.name` emits a valid SELECT with `_gql_default_alias_1.id` joined back to `nodes` and `node_props_text`. When I executed the generated SQL directly against well-formed data, it correctly returned `bob`. Running the same Cypher read after a simpler write path (`CREATE (a)-[:CALLS]->(b)` as a single path) returns `[{"src.name":"alice","tgt.name":"bob"}]` — the read works.
- 2026-04-18: **The real bug is upstream, in `MATCH ... MATCH ... CREATE (a)-[:REL]->(b)`.** Direct inspection of the `nodes`/`edges` tables after the reporter's exact write sequence shows:
  - `MATCH (a) MATCH (b) CREATE (a)-[:CALLS]->(b)` produces `edges: 1|1|3|CALLS` and a phantom node id=3 with no properties — i.e., the CREATE created a new, empty node for `b` instead of reusing the one bound in the second MATCH.
  - `execute_match_create_query` (executor_match.c:728) calls `find_match_clause(query)` which returns only the **first** MATCH. The second MATCH clause's variable bindings are silently dropped, so when CREATE references `b`, the name resolves to an unknown variable and a new anonymous node is created.
- 2026-04-18: **Workaround:** use a single comma-separated MATCH — `MATCH (a:N {node_id:"a"}), (b:N {node_id:"b"}) CREATE (a)-[:CALLS]->(b)` works correctly. The reporter should adopt this pattern until the dispatcher is fixed.
- 2026-04-18: Ticket title ("Target node properties inaccessible") is **misleading** — will rename / reframe at decomposition time. The real defect is "multi-MATCH variable bindings lost in MATCH+CREATE".
- 2026-04-18: **Blocked** pending multi-MATCH design work. Required fix: `execute_match_create_query` (and the MATCH+MATCH+SET and MATCH+MATCH+MERGE equivalents) must iterate every MATCH clause in `query->clauses` and union their bindings into a single var_map before the write clause runs. Related blocker to the T-0188/T-0189 dispatcher gap — all three are symptoms of variable-scope flowing cleanly between clauses at dispatch time.