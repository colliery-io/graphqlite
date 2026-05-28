---
id: p4-named-path-through-with
level: task
title: "P4: Named path through WITH / OPTIONAL varlen null"
short_code: "GQLITE-T-0337"
created_at: 2026-05-27T02:49:38.770497+00:00
updated_at: 2026-05-28T01:42:08.412157+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P4: Named path through WITH / OPTIONAL varlen null

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

Two related path-variable bugs:
1. **Named path through WITH** — `MATCH p=... WITH p ...` errors
   **"no such column: p"** because the path variable is not carried through the
   WITH CTE projection.
2. **OPTIONAL varlen named path** — an OPTIONAL variable-length named path that
   doesn't match returns `[]` instead of SQL `null`.

Target scenarios: **With1 [4], Match9 [9]**.

## Acceptance Criteria

- [x] With1 [4] and Match9 [9] move to pass.
- [x] `MATCH p=... WITH p RETURN p` projects the path through the WITH boundary.
- [x] OPTIONAL varlen named path with no match returns null, not `[]`.
- [x] Zero TCK regressions; unit 944/944; functional clean.
- [x] TCK delta logged here and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

The path variable is currently a synthetic column produced during pattern
hydration but not added to the WITH CTE's projection list. Carry the path's
backing column(s) (elem_ids + hydration expr) through the WITH projection so
downstream clauses can reference `p`. For the OPTIONAL-null case, distinguish
"matched empty path" from "no match" — emit null when the OPTIONAL pattern
produced no row rather than an empty-list hydration. Changes in
`transform_with.c` (projection threading) and the path-hydration emission.

### Dependencies

Path hydration shares code with P1/P2's varlen work; sequence after P1 if the
elem_ids column shape changes. OPTIONAL-null overlaps P3's OPTIONAL emission.

### Risk Considerations

WITH-projection threading is used by many scenarios; a regression here is
broad. Diff the full WITH-family TCK features specifically.

## Status Updates

### 2026-05-27 — Root-caused With1 [4]; fix is a path-projection extraction refactor

`MATCH p = (a) WITH p RETURN p` errors `no such column: p`. Dumped SQL:
- Without WITH: `MATCH p = (a) RETURN p` → `SELECT '[' || alias.id || ']' …`
  (the path is hydrated by transform_return's path-projection block).
- With WITH: `WITH _with_0 AS (SELECT p FROM nodes AS alias) SELECT _with_0.p
  FROM _with_0` — the WITH projection emits a **bare `p`** column. In
  `transform_with.c` (item loop ~389), a path variable has no table alias
  (`transform_var_get_alias` returns NULL) and no `AST_NODE_PROPERTY`/identifier
  branch handles it, so it falls to `dbuf_append(&col_buf, id->name)` → literal
  `p` → no such column.

**Root cause:** the path-hydration projection (`'[' || …elem_ids/path_ids… ']'`,
or the comprehension `json_object('nodes',…,'rels',…)`) lives **only** in
`transform_return.c` (lines ~836-965, gated on `transform_var_is_path`). WITH
has no equivalent, so path variables can't cross a WITH boundary.

**Fix (deferred — refactor):** extract that ~150-line path-projection block
into a shared helper (e.g. `emit_path_value(ctx, path_var)`) and call it from
both `transform_return.c` and the `transform_with.c` item loop (projecting
`<path_sql> AS p` and registering `p` as a projected column so the outer
RETURN reads `_with_0.p`). Mechanical but touches WITH projection, which is
broad — the initiative flags WITH-threading as high regression risk, so it
needs the full WITH-family TCK diff. Match9 [9] (OPTIONAL varlen named path →
null) is a further case layered on the same path-through-WITH plumbing plus
P3's OPTIONAL-null handling.

Left failing; analysis captured for the focused follow-up.

### 2026-05-27 — With1 [4] FIXED (+1); the refactor turned out to be tractable

The path-projection logic already lives in `transform_expression` (the
`AST_NODE_IDENTIFIER` path branch) — no extraction needed. The real gap was
that `transform_with.c`'s item loop has its **own** identifier projection that
bypassed `transform_expression`, so path vars fell through to a bare column.

Implemented:
- **transform_with.c**: path-var WITH items now emit their hydration SQL (via
  `transform_expression_to_string`) `AS <col>`. Path metadata (`path_elements`
  + `path_type`) is preserved across the var-ctx reset (new parallel arrays)
  and the output is re-registered as a path var whose `table_alias` is the CTE
  column.
- **transform_return.c**: the path projection emits that CTE column directly
  when the path var is projected (`table_alias` contains a `.`) instead of
  rebuilding from out-of-scope element aliases; the executor still hydrates the
  stored text into a path object via the preserved `path_elements`.

Scoped tightly: non-path WITH items and non-projected RETURN paths fall through
unchanged. **Verified: +1 (With1 [4]), zero regressions** (rigorous pass-set
diff vs HEAD~1), unit 944/944, functional clean. Commit ce… (P4).

### 2026-05-27 — Match9 [9] FIXED (+1); P4 COMPLETE

`OPTIONAL MATCH p = (a)-[r*]->(x) RETURN r, x, p` returned `r=[]` on the
no-match row (x=C) instead of `r=null`. The varlen edge-list projection
(`SELECT json_group_array(…) FROM json_each('[' || alias.elem_ids || ']')`)
yields `'[]'` when `elem_ids` is NULL (OPTIONAL miss): `json_each(NULL)` → 0
rows → `json_group_array()` returns an empty array, not NULL. Wrapped the
projection in `CASE WHEN alias.elem_ids IS NULL THEN NULL ELSE (…) END`
(commit 9ee353c). The path `p` and node `x` columns were already correct.

**Verified: +1 (Match9 [9]), zero regressions** (full pass-set diff), unit
944/944, functional clean.

**P4 (T-0337) COMPLETE: +2** (With1 [4] path-through-WITH; Match9 [9] OPTIONAL
varlen named-path null).