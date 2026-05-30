---
id: cypher-cross-type-total-ordering
level: task
title: "Cypher cross-type total-ordering comparator (ORDER BY orderability)"
short_code: "GQLITE-T-0340"
created_at: 2026-05-29T18:22:22.437480+00:00
updated_at: 2026-05-29T18:22:22.437480+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
initiative_id: NULL
---

# Cypher cross-type total-ordering comparator (ORDER BY orderability)

## Objective

Make `ORDER BY` over heterogeneous values follow Cypher's total orderability so the
mixed-type ORDER-BY scenarios pass. Target TCK scenarios:
- ReturnOrderBy1 [11]/[12] — ORDER BY distinct types asc/desc
- WithOrderBy1 [21]/[22] — sort distinct types asc/desc
- (related) Comparison2 [3] — needs WITH-WHERE input-scope, tracked separately

Branch: `i0339-next6` (PR #87). This is the "full stack" follow-on after the
+11 batch already on that branch.

## Cypher orderability (ascending)

map < node < rel < list < path < string < bool < number < NaN < null

(null sorts LAST in ascending. NaN sorts after all other numbers.)

## Three stacked sub-features (all required before any scenario passes)

### A. Orderability rank key — `_gql_order_rank(value)` + two-column ORDER BY
- New UDF returns int rank 0..9 by Cypher type. Detection from the SQLite value:
  - NULL -> 9 (null). (NaN must be a non-NULL sentinel, see B.)
  - INTEGER/REAL -> 7 (number).
  - TEXT: boolean subtype or 'true'/'false' -> 6; starts `{` -> inspect keys
    (nodes&rels -> path 4; labels(&id) -> node 1; type&startNode* -> rel 2; else map 0);
    starts `[` -> list 3; NaN sentinel -> 8; else string 5.
- Change `sql_order_by` (src/backend/transform/sql_builder.c:472) to emit
  `_gql_order_rank(expr) <dir>, _gql_order_key(expr) <dir>` — rank groups by type
  (cross-type order), existing `_gql_order_key` sorts within type (homogeneous,
  so SQLite-native sort is correct). Also transform_with.c:688 ORDER BY path.
- RISK: changes cross-type ORDER BY for ALL queries. Must run rigorous pass-set diff.

### B. NaN sentinel value (survives CTE; renders as NaN; ranks 8)
- Subtypes DO NOT survive CTE/subquery boundaries in SQLite (verified). TEXT
  CONTENT does. So NaN = a private sentinel STRING recognized by content, not subtype.
- Emit the sentinel for `0.0/0.0` (compile-time detectable; transform_unwind list
  arm + transform_expr_ops division). Formatter (src/extension.c) renders the
  sentinel as unquoted `NaN`. `_gql_order_rank` detects it -> rank 8.
- Use an unlikely sentinel (e.g. control-char prefix) to avoid colliding with the
  literal Cypher string "NaN".

### C. Path hydration through UNWIND list
- `UNWIND [..., p, ...]` renders path `p` as `[1,1,2]` instead of the path object
  `{"nodes":[...],"rels":[...]}` (direct `RETURN p` renders correctly). Fix the
  path-as-list-element transform to emit the full path object.

## Status Updates

- 2026-05-29: Prerequisite landed on branch (commit 71fe525): UNWIND of an
  entity-containing list no longer crashes (`no such column`); the 4 target
  scenarios moved error->fail.
- 2026-05-29: **Sub-feature A (orderability rank) DONE** — `_gql_order_rank` UDF
  + two-column ORDER BY (`_gql_order_rank(e), _gql_order_key(e)`) in sql_builder.c
  and transform_with.c. Verified: mixed-type ORDER BY now sorts
  map<node<rel<list<string<bool<number correctly; unit 944/944; rigorous
  pass-set diff = zero regressions, zero newly passing (target scenarios still
  need B+C). Committed as correctness groundwork.
- 2026-05-29: **Sub-feature B (NaN value) attempted, reverted.** NaN sentinel
  `\x01NaN` works for the ORDER rank (rank 8 detection kept as forward-compat in
  the UDF). But RENDERING it as `NaN` requires patching 3+ independent formatter
  paths in extension.c, AND the UNWIND result-collection path delivers the value
  already JSON-quoted (`"\x01NaN"`) to the formatter — a content sentinel can't be
  cleanly intercepted everywhere. Reverted the division->sentinel emission + the
  one formatter branch. NEXT: introduce a single shared scalar-render helper in
  extension.c, then re-add the sentinel emission + render in that one place.
- 2026-05-29: **Sub-feature C (path-through-UNWIND hydration) NOT started.**
  `UNWIND [..., p, ...]` renders the path as `[1,1,2]` (elem ids) instead of the
  path object; direct `RETURN p` is correct. Lives in the UNWIND list-element
  path-expr / build_path_from_ids interaction.
- 2026-05-29: **Sub-feature B (NaN value) DONE** (commit d74210e, +1). NaN carried
  as the private string GQL_NAN_SENTINEL (0x01 'N' 'a' 'N'); standalone `0.0/0.0`
  emits `(CHAR(1)||'NaN')`; agtype `create_property_agtype_value` maps it to a
  float NaN whose AGTV_FLOAT serializer prints `NaN`; plain formatter prints it
  too. Fixed WithOrderBy1 [22]. Rigorous diff: zero regressions.
- 2026-05-29: **Sub-feature C (path-as-list-element) DONE** (commit, +3). New
  context flag `emit_hydrated_path` makes the path projection emit the full
  {nodes,rels} object inline (reusing the comprehension builder) for non-varlen
  paths; transform_unwind sets it around each list-element transform. Fixed
  ReturnOrderBy1 [11]/[12], WithOrderBy1 [21].
- 2026-05-29: **STACK COMPLETE.** All four target scenarios pass; mixed-type
  ORDER BY follows Cypher orderability map<node<rel<list<path<string<bool<number
  <NaN<null. 3721 -> 3725 (+4: B +1, C +3; A +0 groundwork). Two full TCK runs
  confirm stability and zero regressions (the ReturnOrderBy1 [1] entry seen in an
  interim `comm` was a baseline-run transient — the scenario is deterministic and
  passes). Task can be marked done.
