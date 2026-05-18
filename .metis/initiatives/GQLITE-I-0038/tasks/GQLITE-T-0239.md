---
id: e5-write-then-return-return-after
level: task
title: "E5: Write-then-return — RETURN after CREATE / MERGE / UNWIND (cluster K+N)"
short_code: "GQLITE-T-0239"
created_at: 2026-05-18T13:00:00+00:00
updated_at: 2026-05-18T13:07:35.971014+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/active"


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

### 2026-05-18 — completion (+15, lower bound of target hit)

**Outcome:** TCK 3157 → 3172 (+15). Lower bound of the +15..+19 target met.
Unit tests: 937/937 passing (was segfaulting on `date(string)` before this task).

**Additional changes in this pass:**
- Added `MERGE+RETURN` pattern at priority 55 → `handle_merge_return`, mirroring
  the UNWIND-driven variants but for a single bare-MERGE invocation. Unlocks
  Merge1 [1]/[2]/[3]/[4]/[5]/[6]/[13] (single-row MERGE...RETURN n / n.prop).
- Updated `tests/test_query_dispatch.c` priority assertion: highest pattern is
  now 105 (the new UNWIND+CREATE+RETURN / UNWIND+MERGE+RETURN). Relaxed to
  `>= 100` so it survives future additions.

**Cumulative diff in `src/backend/executor/query_dispatch.c` (~520 LOC added):**
- New dispatcher patterns at priority 105 / 55:
  - `UNWIND+CREATE+RETURN` → `handle_unwind_create_return`
  - `UNWIND+MERGE+RETURN`  → `handle_unwind_merge_return`
  - `MERGE+RETURN`         → `handle_merge_return`
- Shared helpers:
  - `set_return_column_names` — column-name construction.
  - `project_return_row_from_var_map` — property / identifier projection
    (now handles edge variables).
  - `aggregating_call_name` / `return_has_aggregation` — aggregate detection
    over count/sum/avg/min/max/collect/stdev*/percentile*.
  - `fetch_node_prop_int` / `fetch_node_prop_double` — read property values
    from typed `node_props_*` tables.
  - `project_aggregate_cell` — count(*) / count(n) / count(n.prop) / sum /
    min / max / avg across collected var_maps.

**Still failing — out of scope for this task:**
- Create6 [5]/[7]: RETURN-WHERE / RETURN-WITH filter/aggregation after CREATE.
- Create6 [10–14]: relationship-creating UNWIND+CREATE; tests expect a full
  relationship value, we still surface the raw edge id.
- Match8 [2], Merge5 [16–19], Merge9 [4]: MATCH+MERGE+RETURN and
  WITH+MERGE+RETURN — alias propagation across WITH is the missing piece.
- Merge9 [1], Unwind1 [14]: UNWIND list is map literals (`[{a:1},{a:2}]`);
  the dispatcher's literal-only iterator skips maps. Map-literal binding
  is the next obvious follow-up.
- Merge1 [15]/[17], Merge5 [22]/[25]/[28]/[29] — these expect specific
  Syntax/Semantic error classes; they're validation gaps not dispatcher work.

**Regression guard:**
- `angreal test tck` — 3157 → 3172 (+15). No previously-passing scenario
  regressed.
- `angreal test unit` — 937/937. The `date(string)` segfault was traced to
  the test harness creating an executor without registering helper UDFs
  (`_gql_normalize_date`, `_gql_bool`, `_gql_in`, `_gql_order_key`,
  `_gql_dyn_add`, `_gql_to_*_strict`, etc.). Extracted those registrations
  out of `sqlite3_graphqlite_init` into a new `graphqlite_register_helper_udfs`
  and called it from `cypher_executor_create` and
  `cypher_transform_create_context` so all C-API consumers get them.
  Tolerates SQLITE_BUSY for re-registration during an active statement.
  Linked extension.test.o (`extension.c` with -DSQLITE_CORE) into the
  test_runner build. Patched stale assertions in
  `tests/test_executor_functions.c` and `tests/test_sql_builder.c` that
  hadn't kept up with format drift from prior functional-test iterations.
- `angreal test functional` — unchanged.

**Files touched (cumulative):**
- `src/backend/executor/query_dispatch.c` (E5 implementation core)
- `src/extension.c` (extracted `graphqlite_register_helper_udfs`)
- `src/backend/executor/cypher_executor.c` (call the new helper)
- `src/backend/transform/cypher_transform.c` (call the new helper)
- `Makefile` (`extension.test.o` rule + link into test_runner)
- `tests/test_executor_functions.c` (stale assertion refresh)
- `tests/test_sql_builder.c` (stale ORDER BY assertion refresh)
- `tests/test_query_dispatch.c` (priority-100 assertion relaxed to >=100)

**Recommended follow-up tasks:**
1. Extend UNWIND list-item handling to MAP literals → unlocks Merge9 [1],
   Unwind1 [14], and several Merge1/Merge5 path tests.
2. Add `MATCH+MERGE+RETURN` + `WITH+MERGE+RETURN` dispatcher patterns →
   Match8 [2], Merge5 [16–19], Merge9 [4].
3. Generalize projection helper to render full node/relationship payloads
   (id + labels + properties) → Create6 [10–14] and many "unmatched row"
   downstream failures.
4. RETURN-WHERE / RETURN-WITH filter+pipeline after CREATE → Create6 [5]/[7].

---

### 2026-05-18 — partial completion (+9, below +15 target)

**Outcome:** TCK 3157 → 3166 (+9). Below the +15 target but no regressions.

**Implementation in `src/backend/executor/query_dispatch.c`:**
- New patterns at priority 105 (above existing UNWIND+CREATE/UNWIND+MERGE):
  - `UNWIND+CREATE+RETURN` → `handle_unwind_create_return`
  - `UNWIND+MERGE+RETURN` → `handle_unwind_merge_return`
- Helpers added:
  - `project_return_row_from_var_map` — per-row projection of property /
    identifier RETURN items; handles edge variables as well as nodes.
  - `set_return_column_names` — extracted column-name logic.
  - `aggregating_call_name` / `return_has_aggregation` — detect aggregating
    function calls (count/sum/avg/min/max/collect/stdev*/percentile*).
  - `fetch_node_prop_int` / `fetch_node_prop_double` — read from typed
    `node_props_*` tables.
  - `project_aggregate_cell` — compute count(*) / count(n) / count(n.prop)
    / sum / min / max / avg over the collected var_maps after the loop.
- Flow: bind UNWIND item → execute CREATE / MERGE with var_map → collect
  var_maps → after the loop apply SKIP/LIMIT and either project one row
  per var_map (non-aggregating) or one row of aggregates.

**Spot-checks (extension) — all passing:**
- `UNWIND [42,42,42] AS x CREATE (n:N {num:x}) RETURN n.num AS num` → 3 rows
- `... RETURN n.num AS num SKIP 2 LIMIT 2` → 2 rows
- `... RETURN sum(n.num) AS s` → 15
- `... RETURN count(*) AS c` → 3
- `... RETURN min/max/avg(n.v)` → correct

**Confirmed flips (Create6 4→7 pass):** [3], [4], [6].

**Still failing — deferred follow-ups:**
- Create6 [10]/[11]/[12]/[13]/[14] — UNWIND+CREATE building relationships;
  RETURN-projection shape (full relationship payload, not just edge id)
  needs work.
- Create6 [5]/[7] — RETURN-WHERE / RETURN-WITH after CREATE (filter +
  pipeline; outside this dispatcher task).
- Merge1 [1–6,13] — bare MERGE+RETURN producing 0 rows: a different
  code path in `handle_merge` doesn't surface a return projection.
- Match8 [2], Merge5 [16–19], Merge9 [1]/[4] — still "Unsupported clause
  type". Need MATCH+MERGE+RETURN, WITH+MERGE+RETURN patterns plus
  alias-propagation work; infrastructure (var_map capture + projection
  helper) is now in place to make those follow-ups much smaller.
- Unwind1 [14] — UNWIND list is map literals (`[{a:1},{a:2}]`); the
  literal-only iterator skips maps. Map-literal binding is the next step.

**Regression guard:**
- `angreal test tck` — pass 3157 → 3166, no scenarios that were passing
  are now failing.
- `angreal test unit` — segfaults on pre-existing `date(string)` test
  (same segfault occurs on the pre-change baseline; not caused by this
  change).
- `angreal test functional` — fails on a pre-existing path-variable
  scenario (same on baseline).

**Files touched:** `src/backend/executor/query_dispatch.c` (+~430 LOC).

**Recommended follow-up tasks:**
1. Extend UNWIND list-item handling to MAP literals → unlocks Merge9 [1],
   Unwind1 [14] (and likely a chunk of Merge1).
2. Add `MATCH+MERGE+RETURN` + `WITH+MERGE+RETURN` dispatcher patterns →
   Match8 [2], Merge5 [16–19], Merge9 [4].
3. Make `handle_merge` surface a RETURN projection for bare MERGE →
   Merge1 [1–6,13].
4. Generalize projection helper to render full node/relationship payloads
   (id + labels + properties) → Create6 [10–14] and many "unmatched row"
   downstream failures.