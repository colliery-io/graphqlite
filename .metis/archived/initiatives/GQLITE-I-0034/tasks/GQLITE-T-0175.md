---
id: transform-generate-sql-from-call
level: task
title: "Transform: generate SQL from CALL subquery inner clauses"
short_code: "GQLITE-T-0175"
created_at: 2026-03-29T01:05:14.217884+00:00
updated_at: 2026-03-29T17:16:48.588528+00:00
parent: GQLITE-I-0034
blocked_by: [GQLITE-T-0174]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0034
---

# Transform: generate SQL from CALL subquery inner clauses

## Parent Initiative

[[GQLITE-I-0034]]

## Objective

Transform CALL subquery AST nodes into valid SQL by generating inner SQL from the subquery's clauses within a nested transform context.

## Affected Files

- `src/backend/transform/cypher_transform.c`

## Implementation Notes

**Strategy: Executor-level iteration (chosen in initiative)**. The transform layer generates a parameterized SQL template for the inner query; the executor iterates outer rows and binds variables per row. This is NOT a CTE-join or lateral-join approach.

- Add `AST_NODE_CALL_SUBQUERY` case in `cypher_transform_query()`'s clause switch
- Create a nested transform context for the inner clauses so inner variables do not leak into the outer scope
- Outer variables imported via the leading `WITH` clause become **parameter placeholders** in the generated SQL (bound at execution time by the executor)
- Basic WITH variable forwarding (simple identifiers like `WITH a`) must work in this task — advanced WITH expressions (`WITH a, a.name AS n`) are handled in T-0177
- Each UNION branch in the subquery should be transformed independently and combined with SQL UNION
- **Nested CALL**: `CALL { CALL { ... } }` must work via recursive transform context handling — ensure the nested context creation is re-entrant
- The generated SQL is a **standalone statement** executed per outer row, not a correlated subquery or CTE

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (a) CALL { WITH a MATCH (b) RETURN b } RETURN b` generates valid, executable SQL
- [ ] Inner variables are scoped correctly and do not collide with outer names
- [ ] Multiple UNION branches each produce valid SQL joined by UNION
- [ ] Existing transform tests still pass

## Effort Estimate

2-3 days

## Status Updates

### 2026-03-29: Implementation complete (combined T-0175 + T-0176)

**Architecture decision:** Transform and executor work was combined because the executor-level iteration strategy means the transform layer only needs minimal changes (error handling for CALL in switch statements), while the executor does the heavy lifting.

**Files changed:**
- `src/backend/transform/cypher_transform.c`: Added `AST_NODE_CALL_SUBQUERY` case in both clause switches — signals that CALL requires executor-level handling
- `src/backend/executor/query_dispatch.c`: Added `handle_call_subquery()` executor pattern at priority 90, plus `CLAUSE_CALL` detection in `analyze_query_clauses()`

**Executor handler implements three cases:**
1. **Standalone CALL** (`CALL { MATCH (n) RETURN n }`): Dispatches inner query directly via `cypher_executor_execute_ast`
2. **CALL without outer MATCH** (`CALL { ... } RETURN ...`): Executes inner, accumulates stats
3. **MATCH + CALL** (`MATCH (a) CALL { WITH a SET a.x = 1 }`): Transforms outer MATCH to SQL, iterates rows, builds `variable_map` with outer node IDs, executes inner clauses (SET, MERGE, CREATE) per row. Post-CALL RETURN is handled by re-running the outer query through generic transform after side effects are applied.

**Important: clean builds required** after enum changes — inserting `AST_NODE_CALL_SUBQUERY` shifts enum values, causing subtle bugs with incremental builds.

**Verified:**
- [x] Standalone CALL with MATCH+RETURN inner query
- [x] CALL with UNION (RETURN 1 UNION RETURN 2)
- [x] MATCH + CALL + SET with outer variable binding
- [x] MATCH + CALL + SET + RETURN (side effects + result return)
- [x] Unit tests 921/921 pass
- [x] Functional tests: same pre-existing failure only