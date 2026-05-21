---
id: call-subquery-integration-tests
level: task
title: "CALL subquery integration tests and openCypher compliance"
short_code: "GQLITE-T-0179"
created_at: 2026-03-29T01:05:18.723369+00:00
updated_at: 2026-03-29T17:55:15.186602+00:00
parent: GQLITE-I-0034
blocked_by: [GQLITE-T-0176, GQLITE-T-0177, GQLITE-T-0178]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0034
---

# CALL subquery integration tests and openCypher compliance

## Parent Initiative

[[GQLITE-I-0034]]

## Objective

Build comprehensive integration tests for CALL subquery covering basic usage, WITH import, UNION branches, nested CALL, error cases, and binding tests.

## Affected Files

- `tests/functional/` (new SQL-based functional tests)
- `bindings/python/tests/` (Python binding tests)
- `bindings/rust/tests/` (Rust binding tests)

## Implementation Notes

### Functional SQL tests (new test file: `tests/functional/37_call_subquery.sql`)

**Basic CALL:**
- `CALL { MATCH (n) RETURN n }` — standalone subquery
- `MATCH (a) CALL { WITH a SET a.x = 1 }` — write-only (no RETURN)
- `MATCH (a) CALL { WITH a MATCH (a)-[r]->() RETURN count(r) AS cnt } RETURN a, cnt` — aggregation in inner query
- `CALL { RETURN 1 AS n }` — literal-only subquery

**Variable scoping:**
- `MATCH (a) CALL { WITH a SET a.touched = true }` — basic WITH import
- `MATCH (a) CALL { WITH a, a.name AS n RETURN n }` — WITH expressions
- Outer variable not imported via WITH — must error
- Inner variables do not leak to outer scope

**UNION branches:**
- `CALL { RETURN 1 AS n UNION RETURN 2 AS n }` — returns two rows
- `CALL { WITH c MERGE (c)-[:A]->(x) UNION WITH c MERGE (c)-[:B]->(y) }` — write UNION
- UNION column name mismatch — must error

**Nested CALL:**
- `CALL { CALL { RETURN 1 AS n } RETURN n }` — two levels deep
- Variable scoping across nesting levels

**Multiple CALL clauses:**
- `MATCH (a) CALL { ... } CALL { ... } RETURN a` — sequential CALL blocks

**ORDER BY / LIMIT in inner query:**
- `CALL { MATCH (n) RETURN n ORDER BY n.name LIMIT 5 }`

**Edge cases:**
- Empty outer result set (zero rows) — subquery should not execute
- Large outer result set (100+ rows) — verify no O(n²) blowup
- CALL with no RETURN — write-only, no columns propagated

### Error case tests
- `CALL` without braces → parse error
- Empty `CALL { }` → parse error or empty result
- Referencing outer variable not imported via WITH → scope error with helpful message
- UNION column name mismatch → error
- CALL inside WHERE or expression position → parse error (CALL is a clause, not an expression)

### Transaction semantics
- Inner subquery failure mid-iteration: verify earlier rows' side effects are rolled back (atomic per outer CALL execution)

### Binding tests
- Python binding tests should verify CALL works through the extension API (at least 3 scenarios: basic, WITH import, UNION)
- Rust binding tests should verify CALL works through the extension API (same 3 scenarios)
- Verify behavior against openCypher spec sections 3 and 6.5

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Functional tests cover all CALL variants listed above
- [ ] Error cases produce clear, correct error messages
- [ ] Python binding tests pass for CALL subquery
- [ ] Rust binding tests pass for CALL subquery
- [ ] All existing tests continue to pass (no regressions)

## Effort Estimate

2 days (functional tests across 6+ categories, error cases, binding tests for Python and Rust, transaction semantics verification)

## Status Updates

### 2026-03-29: Complete

Expanded 37_call_subquery.sql to 13 sections. Added 3 Python and 3 Rust binding tests. All pass: 926 unit, 44 functional files, 332 Python, 233 Rust.