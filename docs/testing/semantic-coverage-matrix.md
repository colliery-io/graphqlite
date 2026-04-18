# Semantic Coverage Matrix

**Purpose.** Track end-to-end write-then-read-back correctness for every
combination of Cypher write shape × target entity × value source × scalar
type × read-back shape. Complements `docs/cypher-coverage-matrix.md`
(which tracks *syntax* coverage only).

**Why.** GitHub issue #61 surfaced 7 bugs where the parser accepted the
query and the execution returned no error — but the round-trip data was
wrong. Five of the seven were "node path works, relationship path silently
NULL" or "source endpoint works, target endpoint silently NULL" — i.e.
sibling paths the syntax suite never exercised. This matrix makes every
sibling cell visible so gaps are filed instead of shipped.

**How.** Each cell is either:

- `tests/functional/FILE.sql:TEST` — a named test verifying the round-trip
- `— N/A —` with a one-line reason
- `GAP: short description` — known uncovered cell, file a ticket

Cells update via `docs/testing/update-matrix.sh` (TBD) or manually in
the PR that adds the test.

---

## Axes

| Axis | Values |
|------|--------|
| **Write shape** | `CREATE`, `MERGE`, `MATCH+CREATE`, `MATCH+MERGE`, `MATCH+MATCH+CREATE`, `MATCH+MATCH+MERGE`, `MERGE ON CREATE SET`, `MERGE ON MATCH SET`, `SET n.k = v`, `SET n += {..}`, `SET n += $p` |
| **Target entity** | **node variable**, **relationship variable** |
| **Value source** | **literal**, **`$param`**, **UNWIND-bound**, **map-entry (`n += map`)**, **nested property access** |
| **Scalar type** | TEXT, INTEGER, REAL, BOOLEAN, JSON/MAP, LIST |
| **Read-back shape** | `MATCH (n) RETURN n.k`, `MATCH (s)-[r]->(t) RETURN s.k, r.k, t.k`, `MATCH (…)-[r]->(t) RETURN t.*`, `RETURN DISTINCT … ORDER BY …`, multi-hop `(a)-[r1]->(b)-[r2]->(c)` |

The full cross-product is ≈ 5 000 cells; this matrix targets the
**pairwise-interesting subset** (~ 80 cells). Each cell exercises **one**
pairwise interaction that a real application would hit.

---

## 1. Write shape × Target × Value source

Read-back held constant at `MATCH (…) RETURN x.k` (scalar property read-back
on the written variable). Scalar type × read-back permutations are broken
out in sections 2 and 3.

| Write shape | Target | Literal | `$param` | UNWIND-bound | Map-entry |
|-------------|--------|---------|----------|--------------|-----------|
| `CREATE (n {k:v})` | node | `39:…` ✓ (many) | `26:Section 6` ✓ | `26:11.2` ✓ | n/a |
| `CREATE (a)-[:R {k:v}]->(b)` | rel | `03:…` ✓ | `26:7.1`, `39:#61.2` ✓ (T-0186) | GAP — T-0183 | n/a |
| `CREATE (n) SET n.k = v` | node | `39:T-0194a` ✓ | `39:T-0194c` ✓ | GAP | n/a |
| `CREATE (n) SET n += {..}` | node | `39:T-0194b` ✓ | `39:T-0194c` ✓ | GAP | `39:T-0194b` ✓ |
| `MERGE (n {k:v})` | node | `40:…` ✓ | `26:…` ✓ | GAP | n/a |
| `MERGE (a)-[r:R {k:v}]->(b)` | rel | `39:T-0187` ✓ | `39:T-0187` ✓ (T-0186/7) | GAP | n/a |
| `MERGE (n) SET n.k = v` | node | `39:T-0195a` ✓ | GAP | GAP | n/a |
| `MERGE (n) SET n += {..}` | node | `39:T-0195b` ✓ | GAP | GAP | `39:T-0195b` ✓ |
| `MATCH (a) CREATE (a)-[:R]->(b)` | new rel | `10:…` ✓ | GAP | GAP | n/a |
| `MATCH (a) CREATE (a)-[:R]->(b) SET b.k = v` | new node | `39:T-0198c` ✓ | GAP | GAP | n/a |
| `MATCH (a) MATCH (b) CREATE (a)-[:R]->(b)` | rel | `39:T-0197a` ✓ | `39:T-0197c` ✓ | GAP | n/a |
| `MATCH (a) MATCH (b) MERGE (a)-[r]->(b) SET r.k = v` | rel | `39:T-0196` ✓ | `39:T-0196` ✓ | GAP | n/a |
| `MATCH (a) MATCH (b) SET a.k = v` | node | `39:T-0198d` ✓ | GAP | GAP | n/a |
| `MERGE ... ON CREATE SET r.k = v` | rel | `39:T-0195c` ✓ | `39:T-0187` ✓ | GAP | n/a |
| `MERGE ... ON MATCH SET r.k = v` | rel | `39:T-0202a` ✓ | `39:T-0202b` ✓ | GAP | n/a |
| `MERGE ... SET r += {..}` | rel | `39:T-0202c` ✓ | `39:T-0202d` ✓ | GAP | `39:T-0202c` ✓ |
| `UNWIND [..] AS item CREATE (n {k:item.f})` | node | GAP | GAP | — T-0183 — | n/a |
| `UNWIND [..] AS item MERGE (n {k:item.f})` | node | GAP | GAP | — T-0183 — | n/a |
| `UNWIND [{..}] AS item MATCH (n {k:item.f})` (read) | — | `39:T-0185a` ✓ | `39:T-0185b` ✓ | n/a | n/a |
| `FOREACH (x IN [..] \| CREATE ...)` | node | `28:…` ✓ | GAP | GAP | n/a |

**Column legend:** `NN:…` = `tests/functional/NN_*.sql` covers this cell; `✓` = verified write + read-back assertion; ticket IDs (T-NNNN) reference originating bug.

---

## 2. Target endpoint property access in traversals

Read pattern `MATCH (s)-[r]->(t) RETURN s.k, r.k, t.k` — previously a
blind spot (GQLITE-T-0190 / issue #61.6).

| Scalar type | `s.k` | `r.k` | `t.k` | Notes |
|-------------|-------|-------|-------|-------|
| TEXT    | `39:T-0197b` ✓ | `39:T-0196` ✓ | `39:T-0197b` ✓ | |
| INTEGER | `39:T-0201 INTEGER` ✓ | ✓ | ✓ | |
| REAL    | `39:T-0201 REAL` ✓ | ✓ | ✓ | |
| BOOLEAN | `39:T-0201 BOOLEAN` ✓ | ✓ | ✓ | read-back as `true`/`false` |
| JSON    | `39:T-0201 JSON` ✓ | ✓ | ✓ | read-back is JSON text |
| LIST    | `39:T-0201 LIST` ✓ | ✓ | ✓ | read-back is JSON-array text |

Multi-hop `(a)-[r1]->(b)-[r2]->(c) RETURN a.k, r1.k, b.k, r2.k, c.k`:
TEXT covered by `39:T-0203a` ✓ (plus T-0203b for parameterized filters
and T-0203c for DISTINCT+ORDER BY). Other scalar types still GAP —
follow-up to extend T-0203 per type if regressions surface.

---

## 3. Parameterized vs literal symmetry

For every write path that accepts both literal and `$param` RHSes, a
parallel-passing test for each. Marks were reshuffled when #61.2/3/4/5
shipped — remaining gaps below.

| Pair | Literal | `$param` | Status |
|------|---------|----------|--------|
| CREATE rel inline prop | ✓ | ✓ (`39:T-0186`) | |
| MERGE rel inline prop | ✓ | ✓ (`39:T-0187`) | |
| ON CREATE SET rel | ✓ | ✓ (`39:T-0187`) | |
| ON MATCH SET rel | `39:T-0202a` ✓ | `39:T-0202b` ✓ | |
| Trailing SET after MERGE, rel | ✓ | GAP | file follow-up |
| Trailing SET after MATCH+MERGE, rel | ✓ | GAP | file follow-up |
| `SET r +=` on rel var | `39:T-0202c` ✓ | `39:T-0202d` ✓ | |

---

## Out-of-scope (tracked separately, **not** gap-tracked here)

- **Cypher syntax acceptance** (parser/grammar): `docs/cypher-coverage-matrix.md`.
- **Performance / scale** testing: future initiative.
- **openCypher TCK** integration: `tests/tck/` future work.

---

## Process

1. **When fixing a bug**, add a regression test to `tests/functional/39_issue_regression_tests.sql` using the `T-NNNN` naming pattern and link the cell here.
2. **When landing a feature**, add one matrix row and fill every applicable cell before merge (or explicitly mark GAP with a follow-up ticket).
3. **During PR review**, the reviewer checks that the matrix reflects the change. A CI lint (future: GQLITE-T-0204) will block PRs that touch `src/backend/transform/` or `src/backend/executor/` without updating this doc.
4. **Rotation**: once every six months, audit the matrix for rot. Move completed GAPs to "covered" and file follow-ups for any new blind spot.

---

## Current gap census (2026-04-18)

- 38 cells marked covered (linked to tests).
- 22 cells marked GAP (ready for follow-up tickets).
- 6 cells N/A.

This matrix ships with the GQLITE-I-0035 initiative. Task breakdown:

- **GQLITE-T-0200** — This document (matrix scaffolding + initial census).
- **GQLITE-T-0201** — Fill node/rel symmetry gaps (integer/real/bool/json/list traversal read-back).
- **GQLITE-T-0202** — Fill ON MATCH SET + `SET n +=` on rel gaps.
- **GQLITE-T-0203** — Fill multi-hop `(a)-[r1]->(b)-[r2]->(c)` read-back gaps.
- **GQLITE-T-0204** — CI lint: require matrix diff on transform/executor PRs.
