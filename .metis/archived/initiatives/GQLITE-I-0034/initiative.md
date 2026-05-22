---
id: call-subquery-support-opencypher
level: initiative
title: "CALL {} Subquery Support (openCypher)"
short_code: "GQLITE-I-0034"
created_at: 2026-03-29T00:59:41.386138+00:00
updated_at: 2026-05-21T17:15:47.758110+00:00
parent: GQLITE-V-0001
blocked_by: []
archived: true

tags:
  - "#initiative"
  - "#phase/completed"


exit_criteria_met: false
estimated_complexity: L
initiative_id: call-subquery-support-opencypher
---

# CALL {} Subquery Support (openCypher) Initiative

**GitHub Issue**: #40
**openCypher Spec**: Sections 3 (Subqueries) and 6.5 (CALL subquery)

## Context

`CALL { ... }` subqueries are a core openCypher feature that allows executing inner queries with access to outer scope variables via `WITH`. This is the primary mechanism for:
- Batch ingestion with multiple relationship types per record
- Conditional relationship creation (UNION branches)
- Correlated subqueries that return aggregates

The `CALL` keyword already exists as a reserved keyword in the grammar but has no production rules. No AST node, transform, or executor support exists.

## Goals

- Parse `CALL { <subquery> }` syntax including `WITH` variable import
- Parse `CALL { <subquery> UNION <subquery> }` with UNION branches
- Transform subqueries to SQL (likely as CTEs or correlated subqueries)
- Execute subqueries with outer variable binding

## Non-Goals

- `CALL <procedure>` (stored procedures) — different feature
- `COUNT { }` and `COLLECT { }` subquery expressions — separate, simpler follow-on
- `EXISTS { }` — already tracked as GQLITE-T-0139

## Architecture

### Layers Affected

1. **Grammar** (`cypher_gram.y`): Add `CALL '{' query_list '}'` production rule in the `clause` rule
2. **AST** (`cypher_ast.h/c`): Add `AST_NODE_CALL_SUBQUERY` with `clauses` list and optional `UNION` branches
3. **Transform** (`cypher_transform.c`): Handle `AST_NODE_CALL_SUBQUERY` — likely generate a CTE per branch, with outer variables passed via column references
4. **Executor** (`query_dispatch.c`): Add `CLAUSE_CALL` handling in the pattern matcher; dispatch subquery branches

### SQL Generation Strategy

```cypher
MATCH (c:Company {id: 'acme'})
CALL {
    WITH c
    MATCH (d:Department {id: 'eng'})
    MERGE (c)-[:HAS_DEPT]->(d)
}
```

Could translate to:
```sql
-- CTE for outer MATCH
WITH _outer AS (SELECT c.id AS c_id FROM nodes c ...)
-- Execute subquery for each outer row
-- This requires either:
-- (a) A loop in the executor (row-by-row subquery execution)
-- (b) A correlated CTE pattern
-- (c) A LATERAL join (not supported in SQLite)
```

The most practical approach is **(a) executor-level iteration**: the transform layer generates SQL for the inner query with placeholders for outer variables, and the executor iterates over outer rows, binding variables and executing the inner query per row.

## Tasks (to be created during decompose)

| # | Task | Layer | Effort | Blocked By |
|---|------|-------|--------|------------|
| 1 | Grammar rules for CALL { } with UNION (T-0173) | Parser | 1-2 days | — |
| 2 | AST node for CALL subquery (T-0174) | Parser | 0.5 days | T-0173 |
| 3 | Transform: generate inner SQL from subquery (T-0175) | Transform | 2-3 days | T-0174 |
| 4 | Executor: iterate outer rows, execute inner query per row (T-0176) | Executor | 2-3 days | T-0175 |
| 5 | WITH advanced expressions and scope isolation (T-0177) | Transform+Executor | 1-2 days | T-0175, T-0176 |
| 6 | UNION branch support inside CALL (T-0178) | All layers | 1-2 days | T-0175, T-0176 |
| 7 | Integration tests + openCypher compliance tests (T-0179) | Testing | 2 days | T-0176, T-0177, T-0178 |

**Critical path**: T-0173 → T-0174 → T-0175 → T-0176 → T-0179
**Parallel after T-0176**: T-0177 and T-0178 can run concurrently
**Nested CALL** (`CALL { CALL { ... } }`) is covered in T-0175 (transform) and T-0176 (executor), tested in T-0179

## Dependencies

- Grammar conflict resolution: adding CALL to the clause list may introduce new S/R conflicts with the GLR parser. Current baseline is `%expect 4` (S/R) and `%expect-rr 3` (R/R) — verify before and after.
- **`CALL` is not currently a keyword** in the scanner or grammar — it must be added as a new token.
- The MERGE+WITH pipeline handler (from #36 fix) provides a pattern for executor-level clause chaining that this feature can build on.