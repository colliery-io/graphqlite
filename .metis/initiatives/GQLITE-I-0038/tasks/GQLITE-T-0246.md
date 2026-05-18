---
id: e12-varlen-relationship-list
level: task
title: "E12: Varlen relationship-list projection + heterogeneous UNWIND"
short_code: "GQLITE-T-0246"
created_at: 2026-05-18T17:10:00+00:00
updated_at: 2026-05-18T18:36:11.034931+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E12: Varlen relationship-list projection + heterogeneous UNWIND

Parent initiative: [[GQLITE-I-0038]] · Clusters **Match9 + heterogeneous UNWIND** · Current count: **~25 scenarios** (Match9 9 + WithOrderBy1 [21]/[22] 2 + Comparison2 [3] 4 + Quantifier-rel 5 + scattered others)

## Objective

Two distinct symptoms with the same root cause:

1. **Varlen relationship as a list (Match9 [1]–[5]).**
   `MATCH (a)-[r:R*1..3]->(b) RETURN r` — `r` is bound to a list of
   relationships. We currently emit
   `SELECT _gql_default_alias_2.id FROM ...` against an alias that
   doesn't exist because we never materialise `r` as a list. Need to
   `json_group_array(rel_id)` over the recursive CTE and project that.

2. **Heterogeneous UNWIND (WithOrderBy1 [21]/[22], Comparison2 [3],
   plus the Quantifier "relationships(p)" sub-cluster).**
   `UNWIND [n, r, p, 1.5, 'x', null] AS types` — fails with
   `no such column: _gql_default_alias_0.id` because the transform
   assumes UNWIND row elements are scalars and projects node columns
   from them.

Both stem from the transform layer assuming a single homogeneous type
per variable. Fix together.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

-- Match9 [1]: varlen relationship var as list
CREATE (a:A)-[:R]->(b:B)-[:R]->(c:C);
SELECT cypher('MATCH (a:A)-[r:R*1..3]->(c:C) RETURN r');
-- expect: 1 row with r = list of edge ids (or relationship objects)

-- WithOrderBy1 [21]: heterogeneous UNWIND
CREATE (:N)-[:REL]->();
SELECT cypher('MATCH p = (n:N)-[r:REL]->() UNWIND [n, r, p, 1.5, [''list''], ''text''] AS types WITH types ORDER BY types LIMIT 5 RETURN types');
-- expect: 5 typed rows, sorted by orderability.

-- Comparison2 [3]: UNWIND a mixed list and compare
SELECT cypher('MATCH p = (n)-[r]->() WITH [n, r, p, '''', 1, 3.14, true, null, [], {}] AS types UNWIND range(0, size(types)-1) AS i UNWIND range(0, size(types)-1) AS j WITH types[i] AS lhs, types[j] AS rhs WHERE i <> j WITH lhs, rhs, lhs < rhs AS result WHERE result RETURN lhs, rhs');
-- expect: result rows for numeric pairs only.
EOF
```

## Target files

- `src/backend/transform/transform_match.c` — varlen relationship
  materialisation. The recursive CTE produces one row per hop; for
  `RETURN r`, we need to aggregate edge IDs per path via
  `json_group_array` and project that as a single JSON list.
- `src/backend/transform/transform_unwind.c` — UNWIND row binding.
  When the source list contains node/rel/path/scalar elements
  intermixed, do NOT project assumed scalar columns at transform time.
  Defer to a runtime UDF that returns the right JSON shape per row.
- Possibly a new helper UDF
  `_gql_unwrap_value(json) → text/int/real/json` that inspects each
  element's type tag.

## Expected delta

`+15` to `+25`.

Scenarios expected to flip:
- `clauses/match/Match9.feature` [1]–[5]
- `clauses/with-orderBy/WithOrderBy1.feature` [21], [22]
- `expressions/comparison/Comparison2.feature` [3]
- Several quantifier-over-relationships scenarios that today error
  with `no such column: _gql_default_alias_2.id`.

## Verification

```sh
angreal build extension
angreal test tck --filter "Match9|Comparison2|WithOrderBy1" 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per reproducer.

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] `RETURN r` where `r` is a varlen relationship variable returns
      a list value (JSON array of relationship references / ids).
- [ ] Heterogeneous UNWIND no longer hits the alias-not-found error.
- [ ] No regressions in the homogeneous UNWIND scenarios.

## Risks

- Two interacting code paths (MATCH transform + UNWIND transform);
  changes in one can shift symptoms in the other. Add the var-by-var
  type tracking in `transform_var_ctx` rather than ad-hoc patching.

## Status updates

### 2026-05-18 — blocked / investigation only

**Outcome:** TCK unchanged (3211). No code changes — both sub-clusters
need transform-layer infrastructure that doesn't yet exist.

**Sub-problem 1 — varlen relationship as a list (Match9 [1]–[5]):**
Varlen relationships register as `VAR_KIND_EDGE` (same as a single
edge), so when RETURN projects the variable it emits a single
`<alias>.id` reference. That alias is never created because the
varlen CTE doesn't expose a per-row edge_id column — it produces
`start_id, end_id, depth, path_ids, visited`. To make `RETURN r`
work for a varlen `r`, we need:
- A new variable kind (e.g. `VAR_KIND_REL_LIST`) so the projection
  knows to aggregate over the path.
- The recursive CTE to also accumulate edge IDs
  (`path_edge_ids` column) via `cte.path_edge_ids || ',' || e.id`.
- A new helper UDF `_gql_split_ids(text)` or a JSON shape change to
  emit the IDs as a JSON list rather than a comma-string.

**Sub-problem 2 — heterogeneous UNWIND (WithOrderBy1 [21]/[22],
Comparison2 [3], Quantifier-rel scenarios):**
`UNWIND [n, r, p, 1.5, 'x', null]` fails at transform time because
the transform infers a uniform type for the unwound list from its
first element. When the elements are of different value kinds the
later projection (`ORDER BY types`, `WHERE pred`, `types.x`) emits
SQL against the wrong column kind. Fix needs:
- An "untyped" / "any" var kind that defers column-resolution to
  runtime via a helper UDF.
- Or: a transform pass that detects heterogeneous UNWIND lists and
  expands each element into a typed-tagged JSON value, so
  downstream projections work uniformly through `json_extract`.

Both sub-problems are open-ended transform refactors with no
1-or-2-line fix. They share the same root cause (variable-kind
metadata is a single tag, not a sum type).

**Acceptance criteria:**
- [ ] Varlen relationship list materialization — not addressed.
- [ ] Heterogeneous UNWIND — not addressed.
- [x] No regressions in homogeneous UNWIND or homogeneous-rel cases
      (verified by running the full TCK; count unchanged).

**Recommendation:** Pull both sub-problems out into separate tasks:
(a) introduce `VAR_KIND_REL_LIST` and update varlen CTE + RETURN
projection. (b) introduce a runtime-typed value-kind for
heterogeneous UNWIND with a `_gql_value_kind(json) → text` UDF and
defer column-typed projections.

**Files touched:** none.