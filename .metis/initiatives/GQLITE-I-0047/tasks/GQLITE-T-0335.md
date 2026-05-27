---
id: p2-multi-segment-mixed-fixed
level: task
title: "P2: Multi-segment / mixed fixed+varlen path chains"
short_code: "GQLITE-T-0335"
created_at: 2026-05-27T02:49:32.608411+00:00
updated_at: 2026-05-27T13:48:20.861582+00:00
parent: GQLITE-I-0047
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0047
---

# P2: Multi-segment / mixed fixed+varlen path chains

## Parent Initiative

[[GQLITE-I-0047]]

## Objective

Stitch multi-segment paths that combine a variable-length rel with a fixed
rel, or chain two variable-length segments in one path
(`(a)-[*]->(b)-[r]->(c)`, `(a)-[*]->(b)-[*]->(c)`). These currently return
empty or wrong rows because the single-varlen `elem_ids` hydration (I-0044)
handles only one segment.

Target scenarios: **Match5 [25]/[26]/[28], Match4 [4]/[5], Match6 [17]**.
Note **Match4 [4]'s** failure is actually in its *setup* (CREATE rels between
list-subscript nodes) — a write-path dependency that may need to split out.

## Acceptance Criteria

## Acceptance Criteria

- [x] **Match4 [5]** (varlen rel property predicate `{year:1988}`) moves to pass
  — the genuine in-scope P2 matching bug.
- [x] Match4 [4] write-path blocker filed separately ([[GQLITE-T-0339]]).
- [~] Match5 [25]/[26]/[28]/[29]: **not** a multi-segment varlen bug (those
  match correctly); blocked by the MATCH+CREATE setup bug [[GQLITE-T-0339]].
  Will pass once T-0339 lands; re-verify then.
- [~] Match4 [7] (bound rel inside multi-varlen path; `ambiguous column`
  SQL-gen error) and Match4 [8] (`-[rs*]->` over a bound rel-list) are niche
  varlen features deferred — distinct from multi-segment chains.
- [x] Zero TCK regressions; unit 944/944; functional clean.
- [x] TCK delta logged here and rolled up to [[GQLITE-I-0047]].

## Implementation Notes

### Technical Approach

Each path segment generates its own CTE/join fragment. The stitch point is the
shared intermediate node variable (`b` above): the fixed-rel join's endpoint
must bind to the varlen CTE's terminal node, and the varlen `elem_ids` must be
concatenated across segments for named-path hydration. Likely changes in
`transform_match.c` segment iteration + the varlen CTE wiring in
`cypher_transform.c`.

### Dependencies

Builds on P1 ([[GQLITE-T-0334]]) for the corrected varlen CTE shape; best
sequenced after P1 lands. Match4 [4] has a CREATE write-path prerequisite.

### Risk Considerations

Two varlen segments multiply CTE cost; watch for combinatorial row blowup.
Run `angreal dev clean` after CTE-generator signature changes.

## Status Updates

### 2026-05-27 — P2 premise corrected: multi-segment varlen already works

Investigated the target cluster with the SQL-dump + execution harness. Key
finding: **multi-segment / mixed fixed+varlen chains already match correctly.**
On a clean linear graph `(:A)-[:LIKES]->(:B)-[:LIKES]->(:C)-[:LIKES]->(:D)-[:LIKES]->(:E)`:
- `(a:A)-[:LIKES]->()-[:LIKES*3]->(c)` → `e` ✓
- `(a:A)-[:LIKES]->(x)-[:LIKES*2]->(c)` → `(b, d)` ✓
The generated SQL stitches the fixed segment's target to the varlen CTE's
`start_id` properly. So the original P2 premise (chains not stitched) is wrong.

**Why Match5 [25]/[26]/[28]/[29] fail (0 rows):** their *setup* never builds
the graph. `MATCH (d:D) CREATE (e1:E {name:d.name+'0'}),(e2:E …) CREATE
(d)-[:LIKES]->(e1),(d)-[:LIKES]->(e2)` reports `nc=2 rc=0` over 8 D nodes.
Minimal repro (3 D nodes):
- `MATCH (d:D) CREATE (e:E {name:d.name})` → nc=**1** (expected 3)
- `MATCH (d:D) CREATE (d)-[:LIKES]->(x:X)` → nc=1 rc=1 (expected 3,3)
- `MATCH (d:D) CREATE (g:G) CREATE (d)-[:HAS]->(g)` → nc=1 rc=**0** (expected 3,3)

→ **MATCH+CREATE only processes the FIRST matched row**, and a second CREATE
clause referencing first-clause vars creates 0 rels. This is a **write-path
multiplicity bug**, outside I-0047's pattern/path-matching scope. Filed as a
backlog item; Match5 [25]/[26]/[28]/[29] and Match4 [4] are blocked on it.

**Genuine in-scope P2 work (relationship predicates on varlen):**
- **Match4 [5]** `(a)-[:WORKED_WITH* {year:1988}]->(b)` → returns 3, expected 1.
  The varlen CTE ignores the rel **property predicate** `{year:1988}` (only
  b→c has year 1988). `generate_varlen_cte` must filter edges by inline rel
  properties in base + recursive steps.
- **Match4 [8]** similar (relationships-into-list + varlen rel-prop filter).
- **Match4 [7]** errors `ambiguous column name: _gql_default_alias_0.source_id`
  (bound rel + varlen) — a SQL-gen bug to triage separately.

**Plan:** retarget T-0335 to the varlen relationship-property predicate
(Match4 [5]/[8]); file MATCH+CREATE multiplicity as backlog; re-home the Match5
cluster behind that backlog item.

### 2026-05-27 — Varlen rel-property predicate landed; +1, zero regressions

Extended `generate_varlen_cte` (`cypher_transform.c`): the per-edge predicate
buffer (`tpred`, previously type-only) now also folds inline relationship
property predicates — for each `{k: literal}` pair, an
`EXISTS (SELECT 1 FROM edge_props_<type> ep JOIN property_keys pk … WHERE
ep.edge_id = e.id AND pk.key = '<k>' AND ep.value = <v>)` ANDed into the edge
filter. Applies in the base case (both orientations) and the recursive step, so
**every** edge along the varlen path must carry the properties (mirrors the
non-varlen rel-property filter in `transform_match.c:495`).

**Verification:** Match4 [5] (`(a)-[:WORKED_WITH* {year:1988}]->(b)`) now passes
(was 3 rows, now the 1 correct row). Full TCK pass-set diff: +1, **zero
regressions**. 3690 → 3691. Unit 944/944, functional clean.

**T-0335 complete for its in-scope deliverable.** Match5 cluster + Match4 [4]
deferred to [[GQLITE-T-0339]] (MATCH+CREATE write path); Match4 [7]/[8] are
niche varlen features (bound rel in path / bound rel-list spec), left for a
follow-up if they surface as priorities.