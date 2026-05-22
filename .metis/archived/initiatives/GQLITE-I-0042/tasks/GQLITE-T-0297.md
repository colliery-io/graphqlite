---
id: c12-match-where-set-return-pre-set
level: task
title: "C12: MATCH+WHERE+SET+RETURN — pre-SET var_map capture (unblocks ~10 TCK; depends on C11)"
short_code: "GQLITE-T-0297"
created_at: 2026-05-19T14:49:52.790081+00:00
updated_at: 2026-05-19T14:49:52.790081+00:00
parent: GQLITE-I-0042
blocked_by: []
archived: true

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0042
---

# C12: MATCH+WHERE+SET+RETURN — pre-SET var_map capture (unblocks ~10 TCK; depends on C11)

## Parent Initiative

[[GQLITE-I-0042]]

## Objective

TCK scenarios Set1 [1]/[2], Set2 [2], Set6 [5]/[6]/[7]/[19]/[20]/[21] (and similar Remove6) currently return zero rows because we re-run MATCH+RETURN after SET — the post-SET re-match re-applies the WHERE against the just-updated property and drops the row. Fix: in `handle_match_set`, capture the var_map at the MATCH+WHERE stage (pre-SET), apply SET, then project RETURN from the captured var_map via `project_return_row_from_var_map` (which I-0040 M3 splits out). Depends on C11 because `bind_match_clause_into_varmap` must support every MATCH shape — currently fails for relationship-only patterns, which was why an earlier attempt regressed -23 scenarios.

## Acceptance Criteria

## Acceptance Criteria

- [ ] C11 (canonical execute_match_*) completed.
- [ ] `handle_match_set` switched to the var_map projection path when a RETURN follows.
- [ ] Set1 [1]/[2], Set2 [2], Set6 [5]/[7]/[19]/[21] now pass.
- [ ] No regressions on relationship-only MATCH+SET (currently fragile).

## Status Updates

### 2026-05-19 — Two attempts, both reverted; deferred pending redesign

**Attempt 1 (naive single-row):** Modified `bind_match_clause_into_varmap`
(executor_match.c:772) to also bind VAR_KIND_EDGE — selecting `<alias>.id
AS "<name>_id"` for edges and calling `set_variable_edge_id`. Then in
`handle_match_set` (query_dispatch.c:696), when a trailing RETURN is
present, capture the var_map pre-SET, run `execute_set_operations`, and
project RETURN via `project_return_row_from_var_map` (the I-0040 M3
extraction).

**Result:** TCK regressed -10 (3348 → 3338 band). Root cause: the
single-row capture in `bind_match_clause_into_varmap` only takes the
first matched row. Multi-row MATCH+SET+RETURN scenarios (e.g. `MATCH (n)
SET n.x = n.x * 2 RETURN n`) produced 1 row instead of N.

**Attempt 2 (gated per-row iteration):** Built a new function
`execute_match_set_return_rows` in executor_match.c that:

1. Transforms MATCH to SQL, rewrites SELECT * to explicit id selection
   per node/edge variable.
2. Prepares + executes, buffers all rows' (id, ...) tuples into a
   dynamically-grown array.
3. For each buffered row: builds a fresh var_map, applies SET via
   `execute_set_operations`, projects RETURN via
   `project_return_row_from_var_map`.

Gated entry with `return_safe_for_var_map_projection`: only routes when
every RETURN item is a bare identifier or a simple property access
(`n`, `n.x`) — anything more complex falls back to the legacy re-MATCH
path.

**Result:** TCK regressed -8 to -15 (3356-3360 → 3345-3348 band). The
gating filter isn't tight enough — bare-identifier / property RETURN
shapes still break for cases involving OPTIONAL MATCH, complex
patterns, or relationship variables. The `project_return_row_from_var_map`
projector handles `AST_NODE_IDENTIFIER` and `AST_NODE_PROPERTY` but its
internal SQL for node JSON-projection (executor_result_project.c:398+)
doesn't perfectly mirror what the full transform pipeline produces.

**Working code archived** (in case useful later): both attempts left
the `bind_match_clause_into_varmap` edge-binding patch and the new
`execute_match_set_return_rows` function in good shape. The decl
also landed briefly in `src/include/executor/executor_internal.h`:

```c
int execute_match_set_return_rows(cypher_executor *executor, cypher_match *match,
                                  cypher_set *set, cypher_return *ret,
                                  cypher_result *result);
bool match_set_return_can_use_var_map_projection(cypher_return *ret);
```

Both were reverted to keep main green.

### Recommended path for next attempt

Don't write a custom expression evaluator inside
`project_return_row_from_var_map`. Instead, **route through the existing
SQL pipeline with the var_map pinned as a WHERE id-filter**:

1. Capture var_map per matched row (as attempt 2 does).
2. Apply SET against that var_map.
3. For each row, run `execute_match_return_query` with a synthetic
   MATCH whose pattern is rewritten to anchor every variable on its
   captured id — e.g. transform `(n:A)-[r]->(m)` to `(n:A WHERE id=$n_id)
   -[r WHERE id=$r_id]->(m WHERE id=$m_id)`. Or, equivalently, push
   the id constraints into a separate WHERE clause appended to the
   MATCH.

That way RETURN's SQL generation handles ALL expression types
(function calls, comprehensions, aggregates, etc.) and our only
contribution is the pin-to-id filter. The pin can be generated as a
simple SQL `WHERE n.id = X AND r.id = Y AND m.id = Z` injected into
the MATCH transform's WHERE list before `transform_match_clause` runs.

Alternatively: instead of per-row re-execution, capture the SQL
generated for MATCH+RETURN ONCE, then prepend a CTE that materializes
the pre-SET id snapshot, and rewrite the RETURN to JOIN against that
snapshot CTE (so the row identity comes from the pre-SET state). This
is one SQL execution per query, not per row — likely much faster and
correct under arbitrary RETURN shapes.

### Acceptance Criteria (unchanged)

- [ ] C11 (canonical execute_match_*) completed.
- [ ] `handle_match_set` switched to the var_map projection path when a RETURN follows.
- [ ] Set1 [1]/[2], Set2 [2], Set6 [5]/[7]/[19]/[21] now pass.
- [ ] No regressions on relationship-only MATCH+SET (currently fragile).