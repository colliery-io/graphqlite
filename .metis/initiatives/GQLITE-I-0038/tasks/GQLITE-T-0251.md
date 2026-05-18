---
id: e17-match4-varlen-property
level: task
title: "E17: Match4 varlen + property-predicate alias bug"
short_code: "GQLITE-T-0251"
created_at: 2026-05-18T19:00:00+00:00
updated_at: 2026-05-18T19:56:58.863068+00:00
parent: GQLITE-I-0038
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0038
---

# E17: Match4 varlen + property-predicate alias bug

Parent initiative: [[GQLITE-I-0038]] · Cluster **Match4** · Current count: **~7 scenarios**

## Objective

Match4 has several scenarios that exercise varlen MATCH with a
property predicate, e.g. `MATCH (a:A)-[*]->(b {name: 'x'}) RETURN b`.
They fail with:
```
SQL prepare failed: no such column: _gql_default_alias_X.id
```
or `row count: expected N got 0`. The transform layer emits an
alias for the target node that doesn't get created when the target
also carries a property-map predicate, OR the alias gets named
differently between the WHERE clause and the JOIN.

This is a focused fix in `transform_match.c` — the varlen+property
combination must produce a single consistent alias name across
WHERE, JOIN and projection.

## Reproducer

```sh
sqlite3 :memory: <<'EOF'
.load build/graphqlite

CREATE (:A {n: 'a'})-[:R]->(:M {n: 'b'})-[:R]->(:T {n: 'target'});

-- Match4 [5]: varlen + property predicate on target
SELECT cypher('MATCH (a:A)-[r*]->(t {n: ''target''}) RETURN t.n');
-- expect: 1 row "target"

-- Match4 [1]: fixed-length varlen + label on target
SELECT cypher('MATCH (a:A)-[r*2]->(t:T) RETURN t.n');
-- expect: 1 row "target"

-- Match4 [4]: longer varlen path
SELECT cypher('MATCH (a:A)-[r*1..3]->(t) RETURN t.n');
-- expect: 2 rows "b" and "target"
EOF
```

## Target files

- `src/backend/transform/transform_match.c` — locate the varlen
  block (lines ~1085–1230). The bug is in how the target node alias
  is composed when the target also has properties / a label join.
  Trace the SQL via `CYPHER_DEBUG` and confirm:
  - The CTE's `end_id` column references the same alias used in the
    final WHERE predicate.
  - When property-map JOINs introduce an intermediate alias
    (`_prop_<target>`), the JOIN against `nodes` uses the *same*
    target_alias the CTE expects.
- Possibly `src/include/transform/sql_builder.h` if alias collisions
  trace back to the builder.

## Expected delta

`+7` to `+15`.

Scenarios expected to flip:
- `clauses/match/Match4.feature` [1], [2], [4], [5], [6], [7]
- Possibly related Match6 [15]/[19]/[20], Match7 [13]/[20] (which
  hit the same `_gql_default_alias_X.id` pattern in varlen+named-path
  contexts).

## Verification

```sh
angreal build extension
angreal test tck --filter "Match4|Match6|Match7" 2>&1 | tail -10
angreal test tck 2>&1 | grep "TCK \[ext"

# Spot-checks per reproducer.

# Positive: simple MATCH still works
sqlite3 :memory: <<'EOF'
.load build/graphqlite
CREATE (:A)-[:R]->(:B);
SELECT cypher('MATCH (a:A)-[:R]->(b:B) RETURN b');
EOF

angreal test unit
angreal test functional
```

## Acceptance criteria

- [ ] `MATCH (a)-[*]->(t {prop: val}) RETURN t` returns matching
      target rows.
- [ ] `MATCH (a)-[*N]->(t:Label) RETURN t` returns matching targets.
- [ ] No regressions on existing varlen MATCH (E10's zero-hop +
      bounded scenarios stay green).

## Risks

- Alias generation is shared across many MATCH paths; a fix that
  works for varlen+property can break ordinary MATCH+property. Add
  unit-test-style spot checks for both directions before relying on
  the TCK delta.

## Status updates

### 2026-05-18 — investigation; mostly blocked by E12

**Outcome:** TCK unchanged (3231). No code changes. The Match4
failures decompose into three buckets, each pointing to other tasks.

**Findings (per current Match4 failure):**

| # | Scenario | Root cause | Blocking task |
|---|---|---|---|
| 1 | Fixed-length varlen RETURN r | `RETURN r` where r is varlen — projects to single edge alias that doesn't exist | E12 (varlen-as-list) |
| 2 | Simple varlen pattern via `:CONTAINS` | Parser rejects `CONTAINS` as rel type because keyword | grammar refactor (non_reserved_kw on rel_pattern) |
| 3 | Zero-length varlen mid-pattern via `:CONTAINS` | Same keyword issue | grammar refactor |
| 4 | Longer varlen path | Setup requires UNWIND + collect + list-concat path-build that errors before MATCH | unrelated UNWIND+CREATE chain |
| 5 | Property predicate on varlen rel | Same projection alias as [1], plus property-on-rel filter | E12 |
| 7 | Bound rel + varlen | ambiguous_column on rel binding | varlen alias bug |
| 8 | Match rel into list + varlen | keyword `CONTAINS` parse error | grammar refactor |

**Spot-check (extension), confirms the keyword bottleneck:**
- `MATCH (a {name:"A"})-[:R*]->(x) RETURN x.name` (non-keyword type)
  → returns expected rows ✓
- Same query with `[:CONTAINS*]` → parser error.

**Recommendation:** Don't pursue this as a standalone task —
3 of the 7 fails are an upstream-task dependency (E12), 3 are the
same keyword issue (worth a separate grammar refactor that also
helps other features), and 1 is a setup dependency. Reroute work
to those upstream tasks rather than chasing Match4-specific symptoms.

**Files touched:** none.

**Acceptance criteria:** not met; deferred.