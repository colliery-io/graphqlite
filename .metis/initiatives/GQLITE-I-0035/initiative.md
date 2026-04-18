---
id: semantic-coverage-matrix-close
level: initiative
title: "Semantic Coverage Matrix: close functional-test blind spots in write/read round-trips"
short_code: "GQLITE-I-0035"
created_at: 2026-04-18T19:43:43.419741+00:00
updated_at: 2026-04-18T23:12:16.956728+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: false

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: M
initiative_id: semantic-coverage-matrix-close
---

# Semantic Coverage Matrix: close functional-test blind spots in write/read round-trips

## Context

GitHub issue #61 reported seven distinct Cypher-to-SQL bugs in v0.4.3. All seven were validated against current `main` on 2026-04-18 and filed as bugs GQLITE-T-0185..T-0191.

The distribution is revealing:
- **2 of 7** are unimplemented features (`SET n += map`, UNWIND-scope propagation to MATCH property filters).
- **5 of 7** are "node path works, relationship path silently broken" or "source endpoint works, target endpoint silently returns NULL" — i.e., **symmetric paths where one side is covered by tests and the sibling is not**.

In particular, GQLITE-T-0190 (bug #61.6: `MATCH (a)-[r]->(b) RETURN b.prop` silently returns NULL) should be impossible to ship against a comprehensive functional suite — yet 926 unit tests and 44 functional SQL files did not catch it. The parser/grammar is well-exercised; the end-to-end **write-then-read-back** matrix has large blind spots.

## Goals & Non-Goals

### Goals
- Build a **semantic coverage matrix** that enumerates the cross-product of write shapes × value sources × scalar types × read-back shapes, and author one test per cell.
- Retrofit the functional test harness so new features land with a filled-in matrix row, not just a one-liner happy-path test.
- Back-fill the matrix to catch the five "sibling path" bugs from #61 and any other latent regressions.

### Non-Goals
- Not a rewrite of the test framework itself; extend the existing `tests/functional/` harness.
- Not a fix for the individual bugs in #61 (those are tracked separately under T-0185..T-0191).
- Not aimed at performance/scale testing.

## The Matrix (initial sketch)

Axes to cover pairwise:

| Axis | Values |
|------|--------|
| Write shape | `CREATE`, `MERGE`, `MERGE ... ON CREATE SET`, `MERGE ... ON MATCH SET`, `SET x.k = v`, `SET x += {..}` |
| Target | node variable, relationship variable |
| Value source | literal, `$param`, UNWIND-bound, map-entry, nested property access |
| Scalar type | TEXT, INTEGER, REAL, BOOLEAN, JSON/MAP, LIST |
| Read-back shape | `MATCH (n) RETURN n.k`, `MATCH (s)-[r]->(t) RETURN s.k, r.k, t.k`, `MATCH ...-[r]->(t) RETURN t.*` (every variable's properties), `RETURN DISTINCT`, `RETURN ... ORDER BY`, multi-hop `(a)-[r1]->(b)-[r2]->(c)` |

The full cross-product is too large; target the **critical subset** (~60-80 test cases) where each cell exercises one pairwise interaction that a real app would hit.

## Success Criteria

- [ ] Matrix document committed to `docs/testing/semantic-coverage-matrix.md` with every cell either marked "covered by test X" or "intentionally skipped with reason".
- [ ] All five "sibling path" bugs from #61 (T-0186, T-0187, T-0189, T-0190, T-0191) have a regression test that fails against buggy commits and passes against fixes.
- [ ] Relationship variants of every existing node-variable test either exist or are logged with a reason.
- [ ] Target-endpoint property access has at least one test per scalar type.
- [ ] Parameterized-write tests exist for every write shape × node/rel pair.
- [ ] CI enforces that new features in `src/backend/transform/` add a matrix row (PR-template checklist + linter).

## Proposed Decomposition (tasks to be refined)

1. Author the matrix document (the scaffold itself) and ratify axes.
2. Audit existing `tests/functional/*.sql` — map every current test to a cell; find and list gaps.
3. Fill priority-1 gaps: target-endpoint read-back tests (catches #61.6).
4. Fill priority-2 gaps: relationship-variable write paths with `$param` (catches #61.2, #61.3, #61.5).
5. Fill priority-3 gaps: map-merge `SET += {..}` coverage (catches #61.4).
6. Fill priority-4 gaps: UNWIND-bound variable propagation into MATCH filters and write clauses (catches #61.1; overlaps with T-0183).
7. CI / PR-template enforcement: require "matrix cell" annotation on new feature PRs.

## Open Questions

- Should the matrix live as Markdown, YAML, or a generator? Markdown is readable; YAML enables a linter that verifies every cell has a linked test file.
- Does this supersede or complement the existing `Cypher Spec Coverage Audit Procedure` (GQLITE-S-0001)? Likely complementary — spec coverage asks "does this syntax parse?"; this initiative asks "does the executed semantics round-trip correctly?".

## Risks

- **Scope creep**: the full cross-product is combinatorially large. Mitigate by picking pairwise coverage, not exhaustive.
- **Test runtime**: each functional test currently re-initializes the schema. Matrix expansion could 10× CI time. Mitigate by grouping cells into single SQL files where feasible.
- **Maintenance cost**: if every new feature requires a matrix update, early-stage velocity may slow. Accept this cost — the project is maturing toward "correctness-first" per the existing Arch Review Wave 3 (GQLITE-I-0033).

## Status

- 2026-04-18: Initiative created in response to the pattern visible across GQLITE-T-0185..T-0191 (all seven sub-bugs of issue #61).