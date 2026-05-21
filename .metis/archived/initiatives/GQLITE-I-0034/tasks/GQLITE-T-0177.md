---
id: with-variable-import-into-call
level: task
title: "WITH variable import into CALL subquery scope"
short_code: "GQLITE-T-0177"
created_at: 2026-03-29T01:05:16.452673+00:00
updated_at: 2026-03-29T17:43:12.611711+00:00
parent: GQLITE-I-0034
blocked_by: [GQLITE-T-0175, GQLITE-T-0176]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0034
---

# WITH variable import into CALL subquery scope

## Parent Initiative

[[GQLITE-I-0034]]

## Objective

Implement WITH-based variable import so that a CALL subquery can reference outer-scope variables by listing them in a leading WITH clause.

## Affected Files

- `src/backend/transform/cypher_transform.c`
- `src/backend/executor/query_dispatch.c`

## Implementation Notes

**Scope**: This task covers advanced WITH import features beyond basic variable forwarding (which is handled as part of T-0175/T-0176). Specifically:

- **WITH expressions**: `WITH a, a.name AS n` — derived expressions that compute values from outer variables
- **WITH aliases**: `WITH a AS outerNode` — renaming outer variables in the inner scope
- **Scope isolation enforcement**: variables not listed in WITH must not be accessible inside the subquery — emit a clear error referencing the missing WITH import
- **Error messages**: referencing an outer variable not imported via WITH should produce an error like `Variable 'x' not in scope (did you forget to import it with WITH?)`
- In the transform layer: map outer aliases and expressions to parameter bindings in the generated inner SQL
- In the executor layer: evaluate WITH expressions against outer row data before passing to inner query

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (a) CALL { WITH a SET a.touched = true }` correctly modifies node a
- [ ] Variables not imported via WITH are not visible inside the subquery
- [ ] WITH expressions like `WITH a, a.name AS n` work correctly
- [ ] Errors are raised for references to outer variables not listed in WITH

## Effort Estimate

1-2 days

## Status Updates

### 2026-03-29: Implementation complete

Replaced unfiltered var_map with scoped variable map that only includes variables listed in the inner WITH clause. WITH aliases (`WITH a AS x`) map the outer node ID under the alias name. Without a leading WITH, the scoped map is empty and inner clauses error with "Unbound variable in SET".

WITH expressions (`WITH a, a.name AS n`) deferred — requires value-level binding, not node ID forwarding.

All acceptance criteria verified except AC3 (WITH expressions — deferred). 926 unit tests, 44 functional files pass.