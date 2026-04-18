---
id: return-distinct-with-relationship
level: task
title: "RETURN DISTINCT with relationship traversal returns empty result (bug #61.7)"
short_code: "GQLITE-T-0191"
created_at: 2026-04-18T16:54:27.225857+00:00
updated_at: 2026-04-18T22:58:38.352171+00:00
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

# RETURN DISTINCT with relationship traversal returns empty result (bug #61.7)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 7)

## Objective

Make `MATCH (src {props})-[r:TYPE]->(pkg) RETURN DISTINCT pkg.name ORDER BY pkg.name LIMIT N` return the target-node names, rather than the empty set.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P1 - High (common production query shape)

### Impact Assessment
- **Affected Users**: Any query combining filtered-source traversal with `RETURN DISTINCT` on target properties.
- **Reproduction**:
  ```sql
  SELECT cypher('CREATE (a:N {node_id:"a", node_type:"file", tenant_id:"t", repo_id:"r"})');
  SELECT cypher('CREATE (b:N {node_id:"b", name:"numpy"})');
  SELECT cypher('MATCH (a:N {node_id:"a"}) MATCH (b:N {node_id:"b"}) CREATE (a)-[:DEPENDS_ON]->(b)');
  SELECT cypher('MATCH (src:N {tenant_id:"t", repo_id:"r", node_type:"file"})
                 -[r:DEPENDS_ON]->(pkg:N)
                 RETURN DISTINCT pkg.name AS package ORDER BY pkg.name LIMIT 10');
  -- Actual:   []
  -- Expected: [{"package":"numpy"}]
  ```
- **Expected vs Actual**: one row with `package = "numpy"`; returns empty.

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**, but with a caveat: the reporter saw a SQL error (`no such column: _prop__gql_default_alias_0.value`). Current build does **not** error; it silently returns `[]`. Either the SQL-error path has since been patched, or the repro path differs between Python bindings and direct extension. The empty-result symptom is still a bug.

### Likely relation to other bugs
Almost certainly a downstream manifestation of **GQLITE-T-0190** (bug #61.6 — target node properties inaccessible). If `pkg.name` resolves to NULL, `ORDER BY pkg.name` + `DISTINCT` under certain plan shapes can elide the row, producing `[]`.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Above query returns `[{"package":"numpy"}]`.
- [ ] Multi-target variations (multiple packages, duplicates) return correct DISTINCT set.
- [ ] `ORDER BY` on target-node property preserves results.
- [ ] Regression test added that combines filtered-source + target `RETURN DISTINCT ... ORDER BY`.
- [ ] Re-validate after GQLITE-T-0190 is fixed — if the bug disappears, close as dup.

## Suggested Remediation

1. **First**, fix GQLITE-T-0190 (target node property access). Then re-run this repro — the empty result may vanish.
2. If the empty result persists after #190 is fixed, investigate:
   - `transform_return.c` — interaction between `DISTINCT`, `ORDER BY`, and a filtered-source MATCH.
   - Alias/binding scope: multi-property MATCH filters on `src` may be generating a subquery that doesn't expose `pkg` to the outer SELECT.
3. Also check the reporter's original SQL error path (`_prop__gql_default_alias_0.value`) — that column name shape suggests a malformed property-table join when combining DISTINCT + multi-property source filters. Search for `_prop_` concatenation in alias generation.

## Status Updates

- 2026-04-18: Validated on current `main`. Empty-result form confirmed; reporter's SQL-error form not reproduced (possibly already patched).
- 2026-04-18: **Root cause identified — this is NOT a consequence of T-0190.** Reproduced the bug using a well-formed single-path CREATE (same node/edge shape, no phantom nodes). Still returns `[]`. Ran `EXPLAIN` on the query — the generated SQL contains the smoking gun:
  ```sql
  FROM node_props_text AS _prop__gql_default_alias_0
  JOIN property_keys AS _pk__gql_default_alias_0
    ON _pk__gql_default_alias_0.id = _prop__gql_default_alias_0.key_id
   AND _pk__gql_default_alias_0.key = 'tenant_id'
   AND _prop__gql_default_alias_0.value = 't'
  JOIN nodes AS _gql_default_alias_0 ON ...
  ...
  WHERE _prop__gql_default_alias_0.value = 'r'
    AND _prop__gql_default_alias_0.value = 'file'
    AND ...
  ```
  The MATCH inline property filter `{tenant_id:"t", repo_id:"r", node_type:"file"}` generates **one** `_prop_*` join (for tenant_id) and then attempts to filter the remaining two properties (repo_id, node_type) against the SAME `_prop__gql_default_alias_0.value` column in the WHERE clause. That's a contradiction — a single column can't equal `'t'` AND `'r'` AND `'file'` simultaneously. Hence empty result.
- 2026-04-18: Bug is in `src/backend/transform/transform_match.c`, node inline-property-pattern handler. The first property emits a dedicated `_prop_<alias>` + `_pk_<alias>` join; the remaining properties reuse the same alias instead of generating new per-property joins keyed by their own `pk.key`. Reporter's original SQL error (`no such column: _prop__gql_default_alias_0.value`) is a close cousin from a slightly different plan — they likely hit the same alias-collision defect.
- 2026-04-18: **Blocked** pending transform fix. Scope: extend the map-iteration at transform_match.c lines 159-237 so each `cypher_map_pair` with a LITERAL/PARAMETER value emits its own distinct `_prop_<alias>_<k>` join (or switches to an EXISTS subquery per property) rather than reusing the first pair's alias. Medium-sized change; single file. Independent of T-0188/T-0189/T-0190 dispatcher work.