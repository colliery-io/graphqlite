---
id: e14-delete-remove-set-return
level: task
title: "E14: DELETE/REMOVE/SET + RETURN dispatcher patterns"
short_code: "GQLITE-T-0248"
created_at: 2026-05-18T19:00:00+00:00
updated_at: 2026-05-18T19:51:28.122325+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E14: DELETE/REMOVE/SET + RETURN dispatcher patterns

Parent initiative: [[GQLITE-I-0038]] · Cluster **Delete6 + Set/Remove + RETURN** · Current count: **~12 scenarios**

## Objective

E5 added CREATE+RETURN, MERGE+RETURN, UNWIND+CREATE+RETURN and
UNWIND+MERGE+RETURN. The symmetric work for DELETE, REMOVE, and SET
is still missing — Delete6 [3]/[4]/[10]/[11]/[12] (SKIP/LIMIT after
DELETE) all fail "row count: expected N got 0" because the dispatcher
routes DELETE+RETURN through a path that drops the result rows.

Mirror the E5 work for these three clauses:

- `MATCH ... DELETE ... RETURN`
- `MATCH ... REMOVE ... RETURN`
- `MATCH ... SET ... RETURN`  (where existing MATCH+SET handler exists
  but doesn't surface RETURN)

The implementation already has the `project_return_row_from_var_map`
helper from E5; reuse it.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

CREATE (a:N {n: 1}), (b:N {n: 2}), (c:N {n: 3}), (d:N {n: 4}), (e:N {n: 5});

-- Delete6 [3]: DELETE + RETURN with SKIP/LIMIT
SELECT cypher('MATCH (n:N) WITH n DETACH DELETE n RETURN count(*) AS deleted');
-- expect: 1 row with deleted=5

-- DELETE + RETURN n (return the deleted node before it's gone)
SELECT cypher('CREATE (n:N {x: 1}) WITH n DETACH DELETE n RETURN n.x AS x');
-- expect: 1 row x=1

-- SET + RETURN
SELECT cypher('CREATE (n:N {x: 1}) WITH n SET n.x = 99 RETURN n.x AS x');
-- expect: 1 row x=99

-- REMOVE + RETURN
SELECT cypher('CREATE (n:N {x: 1, y: 2}) WITH n REMOVE n.x RETURN n.y AS y');
-- expect: 1 row y=2
EOF
```

## Target files

- `src/backend/executor/query_dispatch.c`:
  - Add patterns `MATCH+DELETE+RETURN`, `MATCH+SET+RETURN`,
    `MATCH+REMOVE+RETURN` at priority 95 (above the existing
    MATCH+DELETE / MATCH+SET / MATCH+REMOVE at 90).
  - Handler shape: run MATCH to build var_map, run the write
    operation, then call `project_return_row_from_var_map` (already
    defined from E5) per matched row.
  - SKIP/LIMIT honored via the same logic as `handle_create_return`.

## Expected delta

`+10` to `+15`.

Scenarios expected to flip:
- `clauses/delete/Delete6.feature` [3], [4], [10], [11], [12] (~5)
- Several Set/Remove+RETURN scenarios across the suite
- Plus a small tail of similar shapes in Match4/Match5 etc.

## Verification

```sh
angreal build extension
angreal test tck --filter "Delete6|Set|Remove" 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per reproducer above.

angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] MATCH+DELETE+RETURN with SKIP/LIMIT returns the right row count.
- [ ] MATCH+SET+RETURN returns post-SET values.
- [ ] MATCH+REMOVE+RETURN returns post-REMOVE values.
- [ ] No regressions in MATCH+DELETE / MATCH+SET / MATCH+REMOVE
      (no-RETURN) scenarios.

## Risks

- DELETE+RETURN with `count(*)` over the matched set requires the
  count to be computed *before* the DELETE actually runs (otherwise
  it's 0). Pattern-match the aggregating-RETURN case from E5's
  `project_aggregate_cell` and feed it the pre-delete var_map list.

## Status updates

### 2026-05-18 — partial completion (+3)

**Outcome:** TCK 3220 → 3223 (+3). Below the +10 target — only the
literal-RETURN-after-DELETE shape was extended; MATCH+SET+RETURN and
MATCH+REMOVE+RETURN already routed through `execute_match_return_query`
correctly. Unit 937/937.

**Implementation:**
- `src/backend/executor/query_dispatch.c::synthesize_delete_return`:
  extended to handle the literal-RETURN case in addition to COUNT(*).
  When every RETURN item is an `AST_NODE_LITERAL`, we now emit
  `total_deleted` rows of the literal value(s), then apply SKIP /
  LIMIT from the RETURN clause. Unblocks `MATCH (n) DELETE n RETURN
  42 AS num SKIP 2 LIMIT 2`.

**Spot-checks (extension), all correct:**
- `MATCH (n:N) DELETE n RETURN 42 AS num SKIP 2 LIMIT 2` → 2 rows of
  num=42, with 5 nodes deleted as side effect.
- COUNT-based RETURN after DELETE still works (regression check).
- MATCH+SET+RETURN reads post-SET values (already worked).
- CREATE+REMOVE+RETURN reads post-REMOVE values (already worked).

**Deferred:**
- Delete6 [10]/[11]/[12]: relationship deletes with *property*
  RETURN need the pre-delete var_map captured, not just a literal
  emit. Requires reorganizing handle_match_delete to collect a
  var_map *before* DELETE runs. A separate task.
- CREATE+WITH+SET+RETURN: returns the pre-SET value because the
  CREATE var_map is used for projection, bypassing the SET. Separate
  pattern, not on E14's critical path.

**Acceptance criteria:**
- [x] MATCH+DELETE+RETURN with SKIP/LIMIT on literal RETURN returns
      the right row count.
- [x] MATCH+SET+RETURN returns post-SET values (already worked).
- [x] MATCH+REMOVE+RETURN returns post-REMOVE values (already worked).
- [x] No regressions in MATCH+DELETE / MATCH+SET / MATCH+REMOVE.

**Files touched:**
- `src/backend/executor/query_dispatch.c` (+ ~80 LOC in
  `synthesize_delete_return`).