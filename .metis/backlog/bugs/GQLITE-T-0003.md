---
id: fix-parameter-support-in-where
level: task
title: "Fix parameter support in WHERE clauses"
short_code: "GQLITE-T-0003"
created_at: 2025-12-24T01:49:48.647722+00:00
updated_at: 2025-12-24T02:07:01.517901+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Fix parameter support in WHERE clauses

## Objective

Fix parameter binding (`$param`) support in WHERE clauses and other expressions. Parameters are parsed but fail during transformation.

## Priority
- [x] P1 - High (essential for parameterized queries)

## Impact Assessment
- **Affected Users**: Anyone using parameterized queries for security/performance
- **Reproduction Steps**: 
  1. Run `SELECT cypher('MATCH (n) WHERE n.name = $name RETURN n')`
  2. Observe "Failed to transform MATCH clause" error
- **Expected vs Actual**: Should accept parameter placeholder; currently fails in transform

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n) WHERE n.name = $name RETURN n` parses and transforms successfully
- [ ] Parameters work in WHERE clause comparisons
- [ ] Parameters work in property maps: `CREATE (n:Person {name: $name})`
- [ ] Parameters work in SET clauses: `SET n.name = $name`
- [ ] Transform generates SQL with `?` placeholders or named parameters
- [ ] Parameter values can be passed via executor API

## Implementation Notes

### Technical Approach
1. **Investigate**: Check if parser correctly creates `AST_NODE_PARAMETER` nodes
2. **Transform**: Add handling for `AST_NODE_PARAMETER` in `transform_expression()`
3. **SQL Generation**: Output `?` placeholder or `:param_name` for SQLite
4. **Executor API**: Add parameter binding to `cypher_executor_execute()`

### Current State
- Parser likely handles `$param` syntax (scanner has PARAMETER token)
- Transform may be missing the `AST_NODE_PARAMETER` case
- Need to trace exactly where the failure occurs

### Key Files
- `src/backend/transform/transform_return.c` - Expression handling
- `src/backend/transform/transform_match.c` - WHERE clause handling
- `src/executor/cypher_executor.c` - Parameter binding

## Status Updates

*To be added during implementation*