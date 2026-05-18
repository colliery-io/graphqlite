---
id: e1-map-indexing-typeerror-invalid
level: task
title: "E1: Map indexing TypeError — invalid index types (cluster H)"
short_code: "GQLITE-T-0235"
created_at: 2026-05-18T12:24:18.702603+00:00
updated_at: 2026-05-18T12:24:18.702603+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E1: Map indexing TypeError — invalid index types (cluster H)

Parent initiative: [[GQLITE-I-0038]] · Cluster **H** · Current count: **~9 scenarios**

## Objective

Raise TypeError at compile time (or runtime) when a map is indexed
with a non-string key, or a list / string-typed value is indexed with
the wrong kind in cases not covered by the recently-landed
`validate_subscript` rules. Subset of cluster H.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('WITH {a:1} AS m, 1 AS i RETURN m[i]');
SELECT cypher('WITH {a:1} AS m, true AS k RETURN m[k]');
SELECT cypher('WITH {a:1} AS m, [1] AS k RETURN m[k]');
EOF
```

Expected: each should raise `TypeError: InvalidArgumentType` at runtime
(or compile-time). Currently the first two return `null`; the third
returns a meaningless json_extract result.

## Target files

- `src/backend/transform/transform_validate.c` — extend
  `validate_subscript()` to detect map-indexed-by-non-string when the
  base is a `VAR_KIND_PROJECTED` that came from a `{...}` literal (we
  already mark these via `is_scalar_value`).
- `src/backend/executor/executor_match.c` — propagate ctx error message
  so the specific diagnostic survives to the harness.
- (Optional) `src/extension.c` — add `_gql_map_index_strict(map, key)`
  UDF if static rejection isn't reachable for all cases.

## Expected delta

`+9` (with a tolerance of ±2 due to quantifier `rand()` noise).

Specific scenarios that should flip to pass:
- `expressions/map/Map2.feature` [6], [7], [8] (3 scenarios — 4 examples each likely)
- `clauses/set/Set1.feature` [10] (1 scenario, list-of-maps as property)
- `expressions/list/List1.feature` [7], [9] parameter-supplied non-int / non-list (4 scenarios remaining after current literal-only check)

## Verification

```sh
angreal build extension
angreal test tck 2>&1 | grep "TCK \[ext"
# Expected: pass count rises by 7–11 from current 3109.

# Targeted spot-check (should all error out):
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('WITH {a:1} AS m, 1 AS i RETURN m[i]');
SELECT cypher('WITH {a:1} AS m, true AS k RETURN m[k]');
EOF

# Regression guard — both must still pass:
angreal test unit
angreal test functional
```

`git diff --stat` should touch only
`src/backend/transform/transform_validate.c`,
`src/backend/executor/executor_match.c` (error propagation), and
possibly `src/extension.c` if a UDF is added. Nothing else.

## Acceptance criteria

- [ ] All Map2 [6]/[7]/[8] examples pass with TypeError-classified diagnostics.
- [ ] Set1 [10] passes.
- [ ] List1 [7]/[9] parameter-driven cases pass.
- [ ] No existing pass moves to fail or error.

## Status updates

*To be added during implementation*
