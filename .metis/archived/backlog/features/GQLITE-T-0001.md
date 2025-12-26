---
id: implement-union-union-all-clause
level: task
title: "Implement UNION / UNION ALL clause support"
short_code: "GQLITE-T-0001"
created_at: 2025-12-24T01:49:48.401416+00:00
updated_at: 2025-12-24T02:26:57.086473+00:00
parent: 
blocked_by: []
archived: true

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Implement UNION / UNION ALL clause support

## Objective

Add support for UNION and UNION ALL operators to combine results from multiple queries, a core OpenCypher feature for query composition.

## Priority
- [x] P1 - High (important for OpenCypher compliance)

## Business Justification
- **User Value**: Enables combining results from multiple queries without application-level merging
- **Business Value**: Required for OpenCypher compliance, enables more complex analytical queries
- **Effort Estimate**: M (Medium)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `RETURN 1 AS x UNION RETURN 2 AS x` returns two rows
- [ ] `RETURN 1 AS x UNION ALL RETURN 1 AS x` returns two identical rows
- [ ] UNION removes duplicates, UNION ALL keeps all rows
- [ ] Column count and types must match between queries
- [ ] Works with full MATCH...RETURN queries on both sides
- [ ] Proper error messages for mismatched columns

## Implementation Notes

### Technical Approach
1. **Grammar**: Add `UNION` and `UNION ALL` tokens and production rules in `cypher_gram.y`
2. **AST**: Create `cypher_union` node type with `is_all` flag and list of queries
3. **Transform**: Generate SQL `UNION` or `UNION ALL` between transformed subqueries
4. **Executor**: No changes needed - SQLite handles UNION natively

### Key Files
- `src/backend/parser/cypher_gram.y` - Grammar rules
- `src/include/parser/cypher_ast.h` - AST node definition
- `src/backend/parser/cypher_ast.c` - AST construction
- `src/backend/transform/cypher_transform.c` - SQL generation

### Example Transformation
```cypher
MATCH (a:Person) RETURN a.name UNION MATCH (b:Company) RETURN b.name
```
Transforms to:
```sql
SELECT ... FROM nodes WHERE label='Person' 
UNION 
SELECT ... FROM nodes WHERE label='Company'
```

## Status Updates

*To be added during implementation*