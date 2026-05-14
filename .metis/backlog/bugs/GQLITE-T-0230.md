---
id: tck-phase-a-compile-time-literal
level: task
title: "[TCK] Phase A: Compile-time literal-type validation pass (~150 scenarios)"
short_code: "GQLITE-T-0230"
created_at: 2026-05-14T01:54:01.015759+00:00
updated_at: 2026-05-14T02:06:21.890442+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: NULL
---

# Phase A: Compile-time literal-type validation pass

## Source
Filed during [[GQLITE-T-0211]] negative-test triage of the [[GQLITE-I-0037]] baseline. See `docs/tck/baseline-2026-05-13.md` and the cluster table in [[GQLITE-T-0222]].

## Classification
- Type: bug
- Priority: P1
- Parent cluster: [[GQLITE-T-0222]] (extension accepts queries spec requires rejected)
- Expected scenarios unlocked: ~150 scenarios

## Why this matters

The biggest negative-test cluster (331 of 404 "expected error, none raised" scenarios) — TCK calls them `SyntaxError: InvalidArgumentType`, but they're queries like `RETURN NOT 1`, `RETURN 'a' AND true`, `RETURN 1 OR null`, `RETURN [1] = true`, etc. Our parser accepts these because the grammar allows any expression in NOT/AND/OR operand position. openCypher requires them rejected at compile time.

## Approach

New transform-layer pass `transform_validate_types` that runs after the AST is built but before SQL generation. Walks the AST and emits `SyntaxError: InvalidArgumentType` when:

- `NOT <e>` where `<e>` is a literal of non-boolean type (and not NULL).
- `<e1> AND/OR/XOR <e2>` where either operand is a non-boolean literal.
- `<e1> = <e2>` (and `<>`, `<`, `>`, `<=`, `>=`) on heterogeneous literals where openCypher requires the same type family.

Variables and expressions whose type cannot be statically determined are skipped (no false-positive validation).

## Affected scenarios (clusters)

- Boolean1-4 (~116)
- WithOrderBy 1-3 (~50)
- Pattern1 (~17)
- Match1 negative-tests (~24)

## Acceptance Criteria

- [x] Validation logic implemented in `src/backend/transform/transform_validate.{c,h}` + wiring in `src/backend/executor/cypher_executor.c` and `Makefile`.
- [x] Emits `SyntaxError: InvalidArgumentType: Type mismatch: expected Boolean but was <Type>` per TCK convention.
- [x] No regression on existing passing scenarios (fail count dropped by exactly the same as pass increase; error/skipped unchanged).
- [x] Baseline JSON regenerated; pass count 1466 → 1507 (+41); pass rate 37.8% → 38.8%.

## Resolution — 2026-05-14

Landed as commit `c60146e` (close-out in `b9f6f25`).

**Files:**
- `src/include/transform/transform_validate.h` — public interface (`transform_validate_query`).
- `src/backend/transform/transform_validate.c` — recursive AST walker for compile-time type checks.
- `src/backend/executor/cypher_executor.c` — wired the call into the query execution path before transform/SQL.
- `Makefile` — added the new translation unit.

**What's validated:**
- `NOT <e>` where `<e>` is a non-boolean, non-null literal.
- `<e> AND/OR/XOR <e>` where either operand is a non-boolean, non-null literal.
- Recursive walk through function call args, list elements, IS NULL operands.
- Walked from every RETURN/WITH/MATCH-WHERE clause.

**What's NOT validated (deferred):**
- Comparison operators (`=`, `<>`, `<`, etc.) on heterogeneous literal types — saved for a more careful pass once the simpler rejections are in.
- Variables and expressions whose type isn't statically obvious. By design — avoids false positives. Means `WITH 1 AS x RETURN NOT x` does NOT error (we don't know x's type). TCK's negative tests use literals precisely so this is captured for the in-scope cases.

**Verification** — 3880-scenario baseline, prior `19f7ce2` → this commit:

| metric | before | after | delta |
| --- | ---: | ---: | ---: |
| pass    | 1466 | **1507** | **+41** |
| fail    | 1432 | 1391 | -41 |
| error   |  905 |  905 |  0 |
| skipped |   77 |   77 |  0 |

41 of the projected ~150 unlocked — the rest are scenarios where the operand is a *variable* or *expression* (not a literal), so the pass skipped them by design. Those will get caught when Phase B (function arg validation) and Phase C (list/map function validation) land; some require runtime type checks rather than compile-time.

## Parent
Backlog item filed under initiative [[GQLITE-I-0037]] (openCypher TCK Conformance Audit).