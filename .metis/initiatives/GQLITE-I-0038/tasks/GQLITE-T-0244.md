---
id: e10-match5-varlen-path-bounds-off
level: task
title: "E10: Match5 — varlen path bounds (off-by-N, expected 14 got X)"
short_code: "GQLITE-T-0244"
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

# E10: Match5 — varlen path bounds (off-by-N, expected 14 got X)

Parent initiative: [[GQLITE-I-0038]] · Cluster **Match5 varlen bounds** · Current count: **~15 scenarios**

## Objective

Variable-length relationship matches like `(a)-[:R*0..2]->(b)`,
`(a)-[:R*0]->(b)`, and `(a)-[:R*2..3]->(b)` consistently return the
full transitive closure (14 rows on Match5's test graph) instead of
honoring the upper / lower bounds. The "empty interval" cases
(`[*3..1]`) should return 0 rows but return 14.

The bug is in the varlen recursive CTE — it generates all reachable
nodes without filtering by hop count, OR the hop-count column isn't
being compared against the min/max from the AST.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite
-- Match5 [3]: zero hops should bind c=a
CREATE (a:A {name: 'n0'})-[:LIKES]->(:B {name: 'n00'});
SELECT cypher('MATCH (a:A) MATCH (a)-[:LIKES*0]->(c) RETURN c.name');
-- expect: [{"c.name":"n0"}]

-- Match5 [12]: empty interval — upper < lower
SELECT cypher('MATCH (a:A) MATCH (a)-[:LIKES*3..1]->(c) RETURN c.name');
-- expect: 0 rows
EOF
```

## Target files

- `src/backend/transform/transform_match.c` — varlen CTE generation.
  Find the section that emits the recursive `WITH RECURSIVE varlen_N`
  CTE and confirm:
  - The base case for hop=0 returns `(start_node, start_node)` only
    when min_hops is 0 (zero-hop semantics).
  - The recursive step increments `hops` and stops when `hops >= max`.
  - The final SELECT filters `WHERE hops BETWEEN min AND max`.
- `src/backend/parser/cypher_gram.y` and
  `src/include/parser/cypher_ast.h::cypher_varlen_range` — verify
  that the parser distinguishes `*0` (min=0, max=0), `*0..2`
  (min=0, max=2), `*3..1` (min=3, max=1 — must produce empty result),
  and unbounded forms.

## Expected delta

`+10` to `+15`.

Scenarios expected to flip:
- `clauses/match/Match5.feature` [3], [6], [8], [12], [13], [16]
- Possibly related Match9 / Pattern2 varlen scenarios

## Verification

```sh
angreal build extension
angreal test tck --filter Match5 2>&1 | tail -5
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks
sqlite3 :memory: <<'EOF'
.load build/graphqlite
CREATE (a:A {name: 'a'})-[:R]->(b:B {name: 'b'})-[:R]->(c:C {name: 'c'});
SELECT cypher('MATCH (a:A) MATCH (a)-[:R*0]->(x) RETURN x.name'); -- ['a']
SELECT cypher('MATCH (a:A) MATCH (a)-[:R*1]->(x) RETURN x.name'); -- ['b']
SELECT cypher('MATCH (a:A) MATCH (a)-[:R*2]->(x) RETURN x.name'); -- ['c']
SELECT cypher('MATCH (a:A) MATCH (a)-[:R*0..2]->(x) RETURN x.name'); -- ['a','b','c']
SELECT cypher('MATCH (a:A) MATCH (a)-[:R*3..1]->(x) RETURN x.name'); -- []
EOF

# Regression guard
angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] `*0` zero-hop returns just the start node.
- [ ] Bounded `*N..M` returns only paths of length N..M.
- [ ] Empty interval (`*M..N` with M > N) returns zero rows.
- [ ] No regressions in other varlen MATCH scenarios.

## Risks

- Some test fixtures load a binary tree with 14 reachable nodes from
  the root; if the CTE itself is right but the hop-count filter is
  applied to the wrong column, the symptom "14 rows" stays the same.
  Verify by inspecting the generated SQL via `CYPHER_DEBUG`.

## Status updates

*To be added during implementation*
