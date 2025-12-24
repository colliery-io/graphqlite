---
id: implement-foreach-clause-for
level: task
title: "Implement FOREACH clause for iterative updates"
short_code: "GQLITE-T-0006"
created_at: 2025-12-24T01:49:49.042048+00:00
updated_at: 2025-12-24T01:49:49.042048+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#phase/backlog"
  - "#feature"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Implement FOREACH clause for iterative updates

## Objective

Implement the FOREACH clause for iterating over a list and performing update operations on each element.

## Priority
- [x] P2 - Medium (useful for batch updates)

## Business Justification
- **User Value**: Batch update operations without multiple queries
- **Business Value**: OpenCypher compliance, better performance for bulk operations
- **Effort Estimate**: M (Medium)

## Acceptance Criteria

- [ ] `FOREACH (x IN [1,2,3] | CREATE (n {val: x}))` creates 3 nodes
- [ ] `FOREACH (n IN nodes | SET n.updated = true)` updates all nodes in list
- [ ] Nested FOREACH supported
- [ ] Works with list expressions and collected results
- [ ] Only update operations allowed inside FOREACH (CREATE, SET, DELETE, MERGE)

## Implementation Notes

### Technical Approach
1. **Grammar**: Add `FOREACH` clause with `(var IN list | update_clauses)` syntax
2. **AST**: Create `cypher_foreach` node with variable, list expression, and body clauses
3. **Transform**: Generate loop using SQLite's `json_each()` or recursive CTE

### Complexity
FOREACH is more complex than other clauses because:
- It introduces a new variable scope
- The body can contain multiple update clauses
- Need to iterate and execute updates for each list element

### Example Transformation
```cypher
MATCH (p:Person) 
WITH collect(p) AS people
FOREACH (person IN people | SET person.processed = true)
```
Transforms to:
```sql
WITH people AS (SELECT id FROM nodes WHERE label = 'Person')
UPDATE node_props_bool 
SET value = 1 
WHERE node_id IN (SELECT id FROM people) 
  AND key_id = (SELECT id FROM property_keys WHERE key = 'processed')
```

### Alternative: Workaround
Users can often use UNWIND + SET instead:
```cypher
UNWIND [1,2,3] AS x CREATE (n {val: x})
```

## Status Updates

### 2025-12-23
- **Parser implementation complete**: FOREACH clause parses correctly with full AST support
  - Added `cypher_foreach` struct to cypher_ast.h
  - Added `AST_NODE_FOREACH` node type
  - Added grammar rules for `FOREACH (var IN list | update_clauses)`
  - Supports nested FOREACH
  - Supports all update clause types: CREATE, SET, DELETE, MERGE, REMOVE
- **Transform partially implemented**: Basic CTE generation works, but execution of update clauses inside FOREACH not yet complete
  - Users can use UNWIND + update clause as alternative (e.g., `UNWIND [1,2,3] AS x CREATE (n {val: x})`)
- All 388 tests pass