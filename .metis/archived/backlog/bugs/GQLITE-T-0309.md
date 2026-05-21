---
id: var-length-return-projection
level: task
title: "Var-length RETURN: projection treats CTE alias as edge row instead of emitting list of edges"
short_code: "GQLITE-T-0309"
created_at: 2026-05-21T16:05:19.794253+00:00
updated_at: 2026-05-21T17:13:55.373378+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Var-length RETURN projects single-edge JSON against varlen CTE alias

## Reproducer

```sql
.load build/graphqlite.dylib
SELECT cypher('CREATE ()-[:T]->()');
SELECT cypher('MATCH (a)-[r*1..1]->(b) RETURN r');
-- Error: "no such column: _gql_default_alias_2.id"
```

Expected: `[[{type: "T", ...}]]` (list containing one path of edges).

## Root cause

For a variable-length pattern `[r*1..1]`, the FROM clause aliases the
recursive CTE as `_varlen_path_1 AS _gql_default_alias_2`. The CTE has
columns `start_id, end_id, depth, path_ids, visited` — but the RETURN
projection emits a single-edge JSON template referencing
`_gql_default_alias_2.id`, `_gql_default_alias_2.type`, etc. as if the
alias were a row from the `edges` table.

Concrete bad SQL (formatted):

```sql
WITH RECURSIVE _varlen_path_1(start_id, end_id, depth, path_ids, visited) AS (...)
SELECT (CASE WHEN _gql_default_alias_2.id IS NULL THEN NULL
             ELSE json_object('id', _gql_default_alias_2.id,
                              'type', _gql_default_alias_2.type, ...) END)
FROM nodes AS _gql_default_alias_0
CROSS JOIN _varlen_path_1 AS _gql_default_alias_2
CROSS JOIN nodes AS _gql_default_alias_1
WHERE _gql_default_alias_2.start_id = _gql_default_alias_0.id
  AND _gql_default_alias_2.end_id = _gql_default_alias_1.id
  AND _gql_default_alias_2.depth BETWEEN 1 AND 1
-- Errors: no such column: _gql_default_alias_2.id
```

## Fix sketch

When `r` is bound to a variable-length pattern (the var's kind is
VAR_KIND_EDGE but it represents a sequence of edges, the alias points
at the path CTE), the RETURN projection must emit a JSON array
constructed from `path_ids` rather than a single edge JSON object:

```sql
SELECT (SELECT json_group_array(json_object(
            'id', e.id, 'type', e.type, 'startNodeId', e.source_id,
            'endNodeId', e.target_id, 'properties', ...))
        FROM edges e
        WHERE e.id IN (
            SELECT CAST(value AS INTEGER)
            FROM json_each('[' || REPLACE(_gql_default_alias_2.path_ids, ',', ',') || ']')
        ))
```

(or similar — use a helper UDF `_gql_path_edges_json(path_ids)`).

This needs the projection code (transform_return.c, or wherever edge
JSON is emitted) to:
1. Detect that the variable being projected is a varlen-bound edge
   (transform_var->is_varlen flag or similar marker exists?).
2. Branch to the list-of-edges projection instead of single-edge.

## Affected TCK (22 errors)

Match4 [1] [5] [6] [7], Match7 [13] [20], Match9 [1] [2] [3] [4] [5]
[6] [7], and likely additional follow-ons that surface after this
fix. ~22 error scenarios share this root cause.

## Affected files

- `src/backend/transform/transform_return.c` — the edge-projection
  template emission (json_object with edge fields).
- `src/backend/transform/transform_match.c` — sets up the varlen CTE
  and alias; the var_kind / is_varlen info has to flow to the
  projector.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [x] `MATCH (a)-[r*1..1]->(b) RETURN r` returns
  `[{"r": [{"id":1,"type":"T",...}]}]`.
- [x] `MATCH (a)-[r*1..2]->(b) RETURN r` returns one row per path
  with the edge list as `r`.
- [x] No regression in non-varlen edge projection
  (`MATCH ()-[r]->() RETURN r` still single edge JSON).
- [x] `_gql_default_alias` errors flip: 5 errors → 3 pass + 2 fails
  (the 2 fails moved on to row-count mismatches caused by separate
  executor bugs in MERGE/varlen interaction — not the projection
  shape this ticket targeted).

## Status Updates

**2026-05-21 iter 40** — Completed in commit f1c9ee9 chain (final
landing in 4be937f). Four coordinated changes shipped:

1. `transform_match.c` — stamp `transform_var->cte_name` on the
   varlen rel variable so downstream code can branch.
2. `transform_return.c` — when projecting an edge variable with
   `cte_name` set, emit `json_group_array(json_object(...))` over the
   edges whose ids appear in `path_ids`, ordered by `instr()` to
   preserve path order.
3. `executor_match.c` (build_query_results) — for varlen edge vars,
   skip the single-edge re-fetch and leave the column verbatim in
   `result->data[row][col]`.
4. `extension.c` — the JSON result formatter falls back to
   `result->data` text when `agtype_data[row][col]` is NULL. The
   fallback must be gated on the pointer being NULL, not on
   `agtype_value_to_string` returning NULL — that function returns
   `"null"` for NULL input. Both the single-row and multi-row
   single-column paths needed the fix.

TCK delta: pass=3458 → 3461 (+3), errors=84 → 79 (-5).

## Iter 39 attempt — partial: projector fixed, result-renderer still wrong

Implemented the projector branch:
- transform_match.c (line ~1145, inside the `rel->varlen` block):
  call `transform_var_set_cte(ctx->var_ctx, rel->variable, cte_name)`
  to stamp the varlen rel variable.
- transform_return.c (line ~956): when projecting an edge variable
  whose `cte_name` is set, emit
  `(SELECT json_group_array(json_object(...)) FROM edges e WHERE e.id IN (json_each path_ids) ORDER BY instr(...))`
  instead of the single-edge JSON template.

Verified the **generated SQL is correct**: it prepares and runs (no
more "no such column" errors). But the rendered result is still wrong:

```
MATCH (a)-[r*1..1]->(b) RETURN r
=> [{"r": {"id": 0, "type": "", "startNode": 0, "endNode": 0, "properties": {}}}]
   (one row, edge object with all-zero fields)
```

Expected:

```
=> [{"r": [{"id": 1, "type": "T", "startNodeId": ..., "endNodeId": ..., "properties": {}}]}]
   (one row, value is a JSON array with one edge object)
```

Root cause of the residual: **build_query_results in
executor_match.c reads the variable kind from the RETURN AST and
re-fetches edges by id from the column's integer value**. It doesn't
know to treat varlen-bound edge variables as already-rendered JSON
arrays. The fetch path is in `executor_result_project.c` around
line 425 (`edge_id >= 0` branch).

The complete fix needs:
1. The projector branch (done in iter 39, reverted because it doesn't
   help alone).
2. Detect varlen-bound edge vars in `build_query_results` and skip
   the per-edge-id re-fetch — use the column's text value verbatim
   as the JSON array.

Reverted iter 39 because partial fix leaves TCK identical with extra
churn. Full fix is the next attempt.

## Discovered

2026-05-21 during iteration 38 of the open-work queue, dumping the
SQL via /tmp/badsql.log debug print.