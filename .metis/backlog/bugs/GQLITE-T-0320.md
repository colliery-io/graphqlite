---
id: optional-match-sql-emission
level: task
title: "OPTIONAL MATCH SQL emission: reorder target-node JOIN before LEFT JOIN varlen + source-via-edge restructure"
short_code: "GQLITE-T-0320"
created_at: 2026-05-22T14:30:00+00:00
updated_at: 2026-05-23T02:19:26.292674+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/active"


exit_criteria_met: false
initiative_id: NULL
---

# OPTIONAL MATCH SQL emission needs JOIN-order restructure

## Discovered

2026-05-22 during T-0261 partial work (PR #70). The targeted alias
collision and OPTIONAL+varlen LEFT-JOIN fixes that landed in
PR #70 (+7 TCK) hit a hard structural limit for the remaining
OPTIONAL MATCH bugs.

## Affected scenarios (~8-10 TCK + side-effects family)

- **Match7 [3]**: `MATCH (a:A), (b:C) OPTIONAL MATCH (x)-->(b) RETURN x`
  — unbound source `x`. Currently emits `LEFT JOIN nodes AS x ON 1=1`
  returning ALL nodes; expected only nodes connected to b.
- **Match7 [4]**: `MATCH (a1)-[r]->() WITH r, a1 LIMIT 1 OPTIONAL MATCH (a1)<-[r]-(b2)`
  — bound-rel reuse in OPTIONAL.
- **Match7 [9]**: `Longer pattern with bound nodes` — expected 1 got 2,
  same source-cartesian.
- **Match7 [12]/[14]/[19]**: OPTIONAL+varlen with unbound target.
  Currently `near 'AND': syntax error` because constraints land on
  wrong join.
- **Match4 [7]**: `MATCH ()-[r:EDGE]-() MATCH p = (n)-[*0..1]-()-[r]-()-[*0..1]-(m)`
  — bound-rel multi-MATCH collision.
- **Match9 [9]**: `Optionally matching named paths with variable length`
  — varlen + ambiguous bound-rel.
- **MatchWhere6 [1]/[2]/[3]/[5]**: OPTIONAL MATCH with label/property
  predicates on unbound vars — same source-cartesian root cause.

Estimate: **8-12 TCK** unlock once the structural change lands.

## Root cause

`transform_match.c`'s emission order for OPTIONAL MATCH paths is:

  1. `i=0`: source node JOIN (or skipped if bound).
  2. `i=1`: rel handler emits `LEFT JOIN varlen ON ...` + target
     node JOIN.
  3. `i=2`: target node JOIN (skipped if rel handler emitted it).

For OPTIONAL semantics to work in SQL:

- **Source-via-edge**: when source is unbound, we want
  `LEFT JOIN edges ON edges.target = bound_target.id`
  `LEFT JOIN nodes AS source ON source.id = edges.source_id`
  Not `LEFT JOIN nodes AS source ON 1=1` (which returns all nodes).

- **Target-via-edge for varlen**: when target is unbound, we want
  `LEFT JOIN varlen ON varlen.start = source.id AND depth >= 1`
  `LEFT JOIN nodes AS target ON target.id = varlen.end_id`
  Not target JOIN'd via property-table or `1=1`, with constraints
  emitted later.

SQLite enforces that ON clauses can't reference tables joined to
their right (positional restriction). So the target node JOIN MUST
come BEFORE the LEFT JOIN edge if the edge's ON references target;
OR the edge's ON must reference only earlier-joined tables.

The two-LEFT-JOIN approach (edge first, then node tied to edge.id)
is structurally compatible:

  - Edge LEFT JOIN ON edge.endpoint = <bound endpoint>
  - Node LEFT JOIN ON node.id = edge.<other endpoint>

This preserves outer-row semantics (both nullable when no match)
and respects SQLite's positional restriction.

## Proposed implementation

1. Detect OPTIONAL MATCH patterns where source OR target is unbound.
2. In the rel handler, emit the LEFT JOIN edge with ON referencing
   only the bound endpoint(s) (no unbound-node reference).
3. After edge JOIN, emit a LEFT JOIN nodes AS unbound_endpoint ON
   `unbound_endpoint.id = edge.<source|target>_id`.
4. Skip the path-loop's standard generate_node_match for the
   unbound endpoint (it'd emit `LEFT JOIN ON 1=1` which is the bug).
5. Apply label/property filters via the edge JOIN's ON or a
   `(node.id IS NULL OR <filter>)` WHERE clause that preserves
   no-match rows.

This is a substantial restructure of `transform_match.c`'s rel
handler. Touches:

- The path-element loop's skip logic (need to know which endpoints
  are bound).
- The rel handler's CTE/edge JOIN emission (constraints split into
  ON clauses by table-positional order).
- generate_node_match's `1=1` short-circuit for unbound nodes that
  should be tied via edges.

## Why deferred

- Each attempt during the PR #70 work either broke other tests
  (the over-eager skip for ALL OPTIONAL anon sources lost 5 TCK)
  or hit SQLite's positional restriction.
- The right fix needs careful per-shape handling (bound/unbound
  source × bound/unbound target × varlen/non-varlen × directed/
  undirected).
- T-0261's parent initiative I-0043 (transform_expression rewrite)
  may also touch this code; coordinating with that migration
  might be more productive than a standalone fix.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] All Match7 [3]/[4]/[9]/[12]/[14]/[19], Match4 [7], Match9 [9],
  MatchWhere6 [1]/[2]/[3]/[5] pass.
- [ ] No regression on currently-passing OPTIONAL MATCH scenarios.
- [ ] 944/944 unit clean, functional clean.

## Affected files

- `src/backend/transform/transform_match.c` (primary)
- Possibly `src/backend/transform/sql_builder.c` if a new
  "LEFT JOIN with deferred ON" primitive is useful.

## Notes

PR #70 (commit 4ec1219 + 1fdc153 + the older Match4/Match5 fixes)
has the surface-level fixes that DON'T require this restructure.
This ticket captures the deeper structural work that does.

## Status Updates

### 2026-05-22 — Partial fix landed in PR #70 (commit 4ecbf2a)

Implemented the deferred-endpoint mechanism. **+4 TCK**:
- Match7 [3] (unbound source, bound target).
- Match7 [14] (OPTIONAL+varlen with length predicate).
- Match7 [19] (mixed non-varlen + varlen with unbound endpoints).
- WithWhere1 [4] (unbound source filter).

Mechanism:
- Pre-loop analysis computes `defer_to_rel[i]` flags per element.
- Iterative pass tracks already-deferred endpoints so multi-rel
  paths work (rel N+1's endpoint considered available because
  rel N will emit it).
- Path-element loop skips deferred nodes (still registers the var
  for downstream lookup).
- Rel handler receives `src_deferred` / `tgt_deferred` flags;
  emits the deferred node LEFT JOIN AFTER the edge/CTE LEFT JOIN
  with ON tying it to `edge.source_id` / `edge.target_id` (or
  `cte.start_id` / `cte.end_id` for varlen).
- Source-already-in-scope check prevents duplicate-alias JOINs
  when a multi-rel chain reuses an anonymous endpoint.

All "near 'AND': syntax error" failures from the original ticket
are resolved (count: 3 → 0).

**Remaining out of this fix's scope** (different root causes,
need separate work):
- Match7 [12] (varlen rel-uniqueness LOOP edge case).
- Match7 [9] (OPTIONAL row-multiplication semantics).
- Match7 [4] (bound-rel reuse in OPTIONAL).
- Match7 [8]/[27] (OPTIONAL chained with OPTIONAL).
- Match4 [7], Match9 [9] (bound-rel multi-MATCH).
- MatchWhere6 family (OPTIONAL MATCH WHERE doesn't filter rows
  pre-outer-preservation in our impl — needs a different model).

These will need follow-on tickets when picked up.