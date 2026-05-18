---
id: e11-match6-match7-path-direction
level: task
title: "E11: Match6/Match7 — path direction + bound-node optional match"
short_code: "GQLITE-T-0245"
created_at: 2026-05-18T17:10:00+00:00
updated_at: 2026-05-18T17:10:00+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/todo"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E11: Match6/Match7 — path direction + bound-node optional match

Parent initiative: [[GQLITE-I-0038]] · Clusters **Match6 + Match7** · Current count: **~31 scenarios**

## Objective

Two related semantic gaps:

1. **Match6 — direction handling**. `MATCH p = (a)-[r]->(b)` and
   `MATCH (a)-[r]-(b)` (undirected) — we over-return when direction
   should constrain. Scenarios [5]–[10]: "Path query should return
   results in written order", "Respecting direction when matching",
   "Handling direction of named paths".

2. **Match7 — OPTIONAL MATCH with bound nodes**. `MATCH (a:A), (b:B)
   OPTIONAL MATCH (a)-[r]->(b) RETURN ...` — we either return too many
   rows ("expected 1 got 4") or fall back to "ambiguous column name"
   on more complex shapes.

Both clusters live in the MATCH transform pipeline and share the
underlying issue of how we materialise pattern endpoints into SQL.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

-- Match6 [7]: Respecting direction
CREATE (a:A)-[:R]->(b:B);
SELECT cypher('MATCH (a:A) MATCH (b:B) MATCH p = (a)-[:R]->(b) RETURN p');
-- expect: 1 row
SELECT cypher('MATCH (a:A) MATCH (b:B) MATCH p = (b)-[:R]->(a) RETURN p');
-- expect: 0 rows (no edge B→A)

-- Match7 [3]: OPTIONAL MATCH with two bound nodes
CREATE (:Person {name: 'Alice'}), (:Person {name: 'Bob'});
SELECT cypher('MATCH (a:Person {name: ''Alice''}), (b:Person {name: ''Bob''}) OPTIONAL MATCH (a)-[r:KNOWS]->(b) RETURN a.name, b.name, r');
-- expect: 1 row (a=Alice, b=Bob, r=null)
EOF
```

## Target files

- `src/backend/transform/transform_match.c` — relationship direction
  emission. For directed `->`, only `source→target` should join; for
  undirected `-`, both directions should join (via UNION ALL or an
  OR predicate). Audit the direction-flag handling in
  `transform_relationship_join`.
- `src/backend/transform/transform_match.c` — OPTIONAL MATCH path
  with both endpoints already bound: should be a LEFT JOIN on the
  full predicate, not a fresh MATCH. Currently produces too many
  rows because we re-bind the endpoints from a fresh MATCH.

## Expected delta

`+15` to `+25`.

Scenarios expected to flip:
- `clauses/match/Match6.feature` [5]–[10] (direction + path order)
- `clauses/match/Match7.feature` [3], [4], [8], [9] (optional with
  bound nodes)

## Verification

```sh
angreal build extension
angreal test tck --filter "Match6|Match7" 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per the reproducer above.

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] Direction-respecting MATCH returns the right rows (no extra,
      no missing).
- [ ] OPTIONAL MATCH with already-bound endpoints produces exactly
      one row per outer binding (with relationship NULL if no edge).
- [ ] No regressions on currently-passing Match5/Match8 scenarios.

## Risks

- The MATCH transform pipeline is one of the largest files in the
  project. Verify a narrow surgical change (direction flag, or
  endpoint-binding for OPTIONAL) instead of a rewrite.

## Status updates

*To be added during implementation*
