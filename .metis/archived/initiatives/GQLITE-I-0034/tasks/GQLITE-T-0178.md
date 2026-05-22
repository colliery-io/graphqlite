---
id: union-branch-support-inside-call
level: task
title: "UNION branch support inside CALL subquery"
short_code: "GQLITE-T-0178"
created_at: 2026-03-29T01:05:17.703934+00:00
updated_at: 2026-03-29T17:49:00.376956+00:00
parent: GQLITE-I-0034
blocked_by: [GQLITE-T-0175, GQLITE-T-0176]
archived: true

tags:
  - "#task"
  - "#phase/completed"


exit_criteria_met: false
initiative_id: GQLITE-I-0034
---

# UNION branch support inside CALL subquery

## Parent Initiative

[[GQLITE-I-0034]]

## Objective

Support UNION branches inside CALL subqueries so that multiple independent query branches can execute and combine results within a single CALL block.

## Affected Files

- `src/backend/parser/cypher_gram.y` (grammar for UNION inside CALL)
- `src/backend/transform/cypher_transform.c` (transform each branch independently)
- `src/backend/executor/query_dispatch.c` (execute and combine branches)

## Implementation Notes

- Each UNION branch in `CALL { branch1 UNION branch2 }` is an independent query that shares the same imported WITH variables
- The grammar support comes from GQLITE-T-0173's `query_list` rule; this task handles the transform and executor sides
- Transform: generate SQL for each branch and join them with SQL UNION (or UNION ALL depending on semantics)
- Executor: execute each branch against the same outer-row context, combine result sets
- For write-only branches (no RETURN), each branch executes its side effects independently

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `CALL { WITH c MERGE (c)-[:A]->(x) UNION WITH c MERGE (c)-[:B]->(y) }` creates both relationships
- [ ] `CALL { RETURN 1 AS n UNION RETURN 2 AS n }` returns two rows
- [ ] Column names must match across UNION branches (error if mismatch)
- [ ] WITH imports are shared correctly across all branches

## Effort Estimate

1-2 days

## Status Updates

### 2026-03-29: Complete

Added UNION branch handling in `handle_call_subquery()` for outer-MATCH case. Flattens UNION tree into branch array, executes each with its own scoped variable map. Tests in 37_call_subquery.sql section 8b. All ACs verified.