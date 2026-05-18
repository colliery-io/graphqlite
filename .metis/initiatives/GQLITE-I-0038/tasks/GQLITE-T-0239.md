---
id: e5-write-then-return-return-after
level: task
title: "E5: Write-then-return — RETURN after CREATE / MERGE / UNWIND (cluster K+N)"
short_code: "GQLITE-T-0239"
created_at: 2026-05-18T13:00:00+00:00
updated_at: 2026-05-18T13:00:00+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E5: Write-then-return — RETURN after CREATE / MERGE / UNWIND (cluster K+N)

Parent initiative: [[GQLITE-I-0038]] · Clusters **K+N** · Current count: **19 scenarios** (10 NotImpl + 9 Unsupported)

## Objective

Queries that combine a write clause (CREATE / MERGE) with UNWIND
and/or RETURN dispatch through the generic transform pipeline, hit
`transform_return.c:136` (`QUERY_TYPE_WRITE` branch) or
`cypher_transform.c:516/691` (unsupported clause type in transform),
and bail with `NotImplementedError: RETURN after CREATE not yet
implemented` or `TypeError: Unsupported clause type`.

The pattern dispatcher in `query_dispatch.c` covers `UNWIND+CREATE`
(no RETURN), `CREATE+RETURN`, and `MATCH+CREATE+RETURN`, but **not**
`UNWIND+CREATE+RETURN` or any of the MERGE+SET+RETURN / WITH+MERGE
variants that Merge5 / Merge9 / Unwind1 [14] / Match8 [2] exercise.
Add the missing patterns and a shared RETURN-projection helper.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

-- Create6 [3]: UNWIND + CREATE + RETURN (currently NotImpl)
SELECT cypher('UNWIND [42, 42, 42, 42, 42] AS x CREATE (n:N {num: x}) RETURN n.num AS num SKIP 2 LIMIT 2');

-- Create6 [6]: aggregating return after CREATE
SELECT cypher('UNWIND [1,2,3,4,5] AS x CREATE (n:N {num: x}) RETURN sum(n.num) AS sum');

-- Merge9 [1]: UNWIND + MERGE
SELECT cypher('UNWIND [{a: 1}, {a: 2}] AS props MERGE (n {a: props.a}) RETURN n');

-- Match8 [2]: MATCH + MERGE + OPTIONAL MATCH + RETURN
SELECT cypher('MATCH (a:A) MERGE (a)-[:R]->(b) RETURN count(*)');
EOF
```

Expected per scenario: side-effects (nodes/edges created) AND a
result set matching the RETURN clause. Currently each raises
`NotImplementedError` or `Unsupported clause type`.

## Target files

- `src/backend/executor/query_dispatch.c`
  - **Add patterns**:
    - `UNWIND+CREATE+RETURN` — bind UNWIND row to var_map, run CREATE
      per row (already done by `handle_unwind_create`), then run RETURN
      against the accumulated var_map.
    - `UNWIND+MERGE+RETURN` — same shape with MERGE.
    - `UNWIND+MERGE` (no RETURN) — Merge9 [1] is currently dispatched
      somewhere that errors with Unsupported; verify which path.
    - `MATCH+MERGE+RETURN` — Match8 [2].
    - `WITH+MERGE+RETURN` — Merge5 / Merge9 [4] aliasing + predicate
      WITH variants.
  - **Helper**: factor out the result-projection logic from
    `handle_create_return` (lines ~2010-2100) into
    `project_return_from_var_map(executor, ret, var_map, result)`
    callable from the new patterns.
- `src/backend/transform/transform_return.c:136` — once dispatch covers
  these patterns the generic-path fallback shouldn't be hit. Confirm
  and, if a write-then-return query still slips through, relax the
  hard NotImpl error and let the generic path try.

## Expected delta

`+15` to `+19` (cluster size = 19; allow tolerance for any scenarios
that fail downstream on result correctness rather than the dispatcher
wall).

Scenarios expected to flip to pass:
- `clauses/create/Create6.feature` [3], [4], [5], [6], [7], [10],
  [11], [12], [13], [14] — 10 NotImpl scenarios
- `clauses/match/Match8.feature` [2]
- `clauses/merge/Merge1.feature` [9]
- `clauses/merge/Merge5.feature` [16], [17], [18], [19]
- `clauses/merge/Merge9.feature` [1], [4]
- `clauses/unwind/Unwind1.feature` [14]

## Verification

```sh
angreal build extension
angreal test tck 2>&1 | grep "TCK \[ext"
# Expected: pass count rises by 15–19 from current 3157.

# Spot checks (each must return rows and report side effects):
sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('UNWIND [42, 42, 42] AS x CREATE (n:N {num: x}) RETURN n.num AS num');
EOF
# Expected: 3 rows, num=42 each, side-effect: 3 nodes created.

sqlite3 :memory: <<'EOF'
.load build/graphqlite
SELECT cypher('UNWIND [1, 2] AS x CREATE (n:N {num: x}) RETURN sum(n.num) AS s');
EOF
# Expected: 1 row, s=3.

# Regression guard:
angreal test unit
angreal test functional
```

`git diff --stat` should be concentrated in
`src/backend/executor/query_dispatch.c` (most of the work) plus small
touches to `cypher_transform.c`/`transform_return.c` if the
generic-path fallback can be relaxed.

## Acceptance criteria

- [ ] All 10 Create6 [3]–[14] scenarios pass with both rows and
      side-effect counters matching.
- [ ] At least 6 of the 9 "Unsupported clause type" scenarios (Match8,
      Merge1, Merge5, Merge9, Unwind1) flip to pass.
- [ ] `handle_create_return`'s RETURN projection logic is extracted
      into a reusable helper so the new patterns can call it.
- [ ] No regressions: unit + functional + every other TCK scenario
      previously passing.

## Implementation plan

1. Read `handle_create_return` (query_dispatch.c:1961) end-to-end to
   understand how it walks var_map → projects RETURN columns. Note
   how it handles aggregating expressions (`sum(n.num)`).
2. Extract that projection into a helper. Keep `handle_create_return`
   as a thin wrapper calling it.
3. Add `handle_unwind_create_return`: loop over UNWIND values, bind
   each to the var_ctx, execute every CREATE with the binding,
   append resulting node IDs into a shared var_map, finally invoke
   the helper.
4. Repeat the shape for MERGE: `handle_unwind_merge_return`,
   `handle_match_merge_return`, `handle_with_merge_return`.
5. Register each new pattern in the `patterns[]` array with priority
   higher than the catch-all "create" patterns.
6. Targeted spot-check; full TCK; commit per pattern so we can
   bisect if anything regresses.

## Risks

- The aggregating RETURN case (`sum(n.num)` in Create6 [6]/[7]) needs
  the projection helper to run after **all** UNWIND iterations
  finish, not per-row. Make sure the helper takes the fully-populated
  var_map.
- MERGE has match-vs-create branching; ensure the var_map captures
  both ON-CREATE and ON-MATCH bindings.
- `WITH+MERGE` (Merge5 [16-19]) involves variable aliasing — the
  alias propagation must reach the MERGE clause.

## Status updates

*To be added during implementation*
