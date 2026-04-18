---
id: unwind-variable-in-match-property
level: task
title: "UNWIND variable in MATCH property pattern doesn't filter (bug #61.1)"
short_code: "GQLITE-T-0185"
created_at: 2026-04-18T16:54:18.439013+00:00
updated_at: 2026-04-18T22:56:22.640844+00:00
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

# UNWIND variable in MATCH property pattern doesn't filter (bug #61.1)

**GitHub Issue**: [#61](https://github.com/colliery-io/graphqlite/issues/61) (sub-bug 1)

## Objective

Ensure an `UNWIND` variable referenced inside a subsequent `MATCH` property pattern correctly filters nodes to the UNWIND-bound value, rather than returning all nodes of the matched label.

## Backlog Item Details

### Type
- [x] Bug

### Priority
- [x] P1 - High

### Impact Assessment
- **Affected Users**: Anyone doing batch lookups via `UNWIND [...] AS item MATCH (n {prop: item.id})` — a common ingestion/enrichment pattern.
- **Reproduction**:
  ```sql
  .load build/graphqlite.dylib
  SELECT cypher('CREATE (a:Test {node_id: "a", name: "alice"})');
  SELECT cypher('CREATE (b:Test {node_id: "b", name: "bob"})');
  SELECT cypher('UNWIND [{id: "b"}] AS item MATCH (n:Test {node_id: item.id}) RETURN n.node_id, n.name');
  ```
- **Expected vs Actual**:
  - Expected: `[{"n.node_id":"b","n.name":"bob"}]`
  - Actual:   `[{"n.node_id":"a","n.name":"alice"},{"n.node_id":"b","n.name":"bob"}]`

### Validity
- [x] **Confirmed reproducible on graphqlite built from `main` (v0.4.3, 2026-04-18)**

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `UNWIND [{id:"b"}] AS item MATCH (n:L {k: item.id}) RETURN n` returns only the matching row.
- [ ] Works equivalently for `UNWIND $param AS item MATCH (n:L {k: item.id})`.
- [ ] Direct `$param` in MATCH property patterns continues to work (already passing).
- [ ] Regression tests added under `tests/functional/`.

## Suggested Remediation

The `transform_match` path generates the inline-property filter from the AST but evidently treats `item.id` (a property access on an UNWIND-bound variable) as an unresolved reference and elides it, producing a MATCH with no WHERE clause on `node_id`. Likely locations:

- `src/backend/transform/transform_match.c` — inline property pattern filter generation.
- Scoping: UNWIND-bound variables must be registered in `var_ctx` so that subsequent MATCH pattern property expressions resolve `item.id` to the UNWIND projection alias (same mechanism used by direct `$param` references, which DO work per the reporter).
- Cross-reference the working `$param` path (likely `transform_expr_ops.c` parameter resolution) and apply the same binding lookup for UNWIND variables.

Overlaps with GQLITE-T-0183 (UNWIND+write) but is distinct: this bug is the **read/MATCH** path.

## Status Updates

- 2026-04-18: Validated against current `main`. Bug reproduces exactly as reported.
- 2026-04-18: Root cause identified — **two-part defect**:
  1. `src/backend/transform/transform_unwind.c` lines 91-132: `AST_NODE_LIST` handler only has a literal-value branch (INTEGER/DECIMAL/STRING/BOOLEAN/NULL). Any non-literal item (e.g. `AST_NODE_MAP` `{id:"b"}`) silently becomes `NULL`. Comment at line 128: `/* For other expression types, we'd need to transform them */`.
  2. `src/backend/transform/transform_match.c` lines 159-237: node inline-property-pattern filter only handles `AST_NODE_LITERAL` and `AST_NODE_PARAMETER`. An `AST_NODE_PROPERTY` RHS (like `item.id`) falls through with no WHERE clause emitted — hence "no filter, returns all rows".
  Also relevant: `transform_unwind.c` line 266 `transform_var_ctx_reset(ctx->var_ctx)` — comment says "UNWIND creates a new scope"; per openCypher this is only partially true. Orthogonal to this bug but a separate correctness concern.
- 2026-04-18: **Blocked** pending design decision — fix touches three orthogonal pieces that interact: (a) serialize UNWIND-of-list-of-maps as JSON and store in CTE's value column; (b) teach match filter generator to emit `json_extract` when base is UNWIND-projected; (c) extend inline-property-pattern filter to accept generic expression via `transform_expression` instead of literal/param switch. Recommend bundling with GQLITE-T-0183 (UNWIND+write paths) under a single "UNWIND parity" initiative.