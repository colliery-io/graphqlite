---
id: set-on-relationship-variable-after
level: task
title: "SET on relationship variable after MERGE raises Unbound variable (bug #61.5)"
short_code: "GQLITE-T-0189"
created_at: 2026-04-18T16:54:24.149371+00:00
updated_at: 2026-04-18T22:43:14.997274+00:00
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

# SET on relationship variable after MERGE raises "Unbound variable" (bug #61.5)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 5)

## Objective

Fix scope resolution so that a relationship variable bound by `MERGE (a)-[r:T ...]->(b)` remains visible to a subsequent `SET r.prop = ...` in the same query, instead of raising `Unbound variable in SET: r`.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P0 - Critical (hard error blocks the canonical MERGE-then-SET pattern)

### Impact Assessment
- **Affected Users**: Anyone using the standard Neo4j `MERGE ... SET` pattern without `ON CREATE` / `ON MATCH` clauses.
- **Reproduction**:
  ```sql
  SELECT cypher('MATCH (a:N {node_id:"a"}) MATCH (b:N {node_id:"b"})
                 MERGE (a)-[r:CALLS {edge_id:"e1"}]->(b)
                 SET r.file = "test.py", r.line = 42');
  -- Runtime error: {"error":"Unbound variable in SET: r","code":"EXECUTION_ERROR"}
  ```
  Node equivalent `MERGE (n) SET n.k = v` works.
- **Expected vs Actual**: SET should apply `file`/`line` to the merged edge; instead the executor raises "Unbound variable".

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**. Exact error message: `Unbound variable in SET: r`.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MERGE (a)-[r:T {...}]->(b) SET r.k = v` applies the SET to the merged relationship.
- [ ] Works whether the MERGE creates or matches the edge.
- [ ] Node MERGE+SET continues to work (regression).
- [ ] Multi-statement SET (`SET r.a = 1, r.b = 2`) works in one query.
- [ ] Regression tests for relationship MERGE+SET (with and without `ON CREATE`).

## Suggested Remediation

The MERGE executor binds `r` into the variable context for subsequent clauses in the node case but not the relationship case. Investigation:

- `src/backend/executor/query_dispatch.c` — MERGE relationship dispatch. After the INSERT/SELECT resolves the edge id, register the edge variable name in `var_ctx` (mirroring the node MERGE path) so SET can resolve `r` to the edge row.
- Variable lookup in `transform_set.c` (or wherever SET resolves its target variable) — ensure it consults both the node AND relationship scopes of `var_ctx`.

This is almost certainly a one-line "forgot to register the variable" omission in the MERGE rel handler.

## Status Updates

- 2026-04-18: Validated on current `main`. Reproduces the exact "Unbound variable in SET: r" error.
- 2026-04-18: Earlier suspicion ("one-line forgot to register the variable") was **wrong**. `set_variable_edge_id(var_map, rel_pattern->variable, edge_id)` is already called in both MERGE entry points (executor_merge.c lines 765 and 1403). Edge variable IS registered — but only within the `var_map` local to the MERGE handler, which is freed before the outer SET runs.
- 2026-04-18: Real root cause is the **same dispatcher gap as GQLITE-T-0188**: the pattern dispatcher for `MATCH+MERGE+SET` invokes the MERGE handler without threading MERGE's var_map into a subsequent `execute_set_operations` call. The "Unbound variable" message surfaces because SET eventually runs via a different path that builds a fresh, empty-ish var_map without `r`.
- 2026-04-18: Related fix confirmed indirectly: my earlier T-0187 patch now allows `ON CREATE SET r.x = $p` (inside `execute_merge_clause`) to work. Only **bare** SET-after-MERGE is still broken.
- 2026-04-18: **Blocked** pending dispatcher-level fix, shared with GQLITE-T-0188. Remediation: add `MERGE+SET` / `MATCH+MERGE+SET` dispatch entries that call an `execute_merge_clause_with_varmap` variant, then `execute_set_operations(executor, set, varmap, result)`. Model after the existing UNWIND+CREATE+SET path (`query_dispatch.c:1471`).
- 2026-04-18: **Recommendation:** bundle T-0188, T-0189, and T-0185 with a narrow "Write-clause → SET var_map threading" initiative (~1 week); all three are manifestations of the same dispatcher gap.