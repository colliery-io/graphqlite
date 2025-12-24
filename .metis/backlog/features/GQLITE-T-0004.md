---
id: implement-remove-clause-for
level: task
title: "Implement REMOVE clause for property deletion"
short_code: "GQLITE-T-0004"
created_at: 2025-12-24T01:49:48.772403+00:00
updated_at: 2025-12-24T02:49:08.449297+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#feature"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Implement REMOVE clause for property deletion

## Objective

Implement the REMOVE clause for deleting properties from nodes and relationships, and removing labels from nodes.

## Priority
- [x] P2 - Medium (useful but SET n.prop = null is a workaround)

## Business Justification
- **User Value**: Clean syntax for removing properties without setting to null
- **Business Value**: OpenCypher compliance, cleaner data management
- **Effort Estimate**: S (Small)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MATCH (n) REMOVE n.property RETURN n` removes the property
- [ ] `MATCH (n) REMOVE n:Label RETURN n` removes the label from the node
- [ ] Multiple removes in one clause: `REMOVE n.a, n.b, n:Label`
- [ ] Works with WHERE filtering
- [ ] Returns updated node after removal

## Implementation Notes

### Technical Approach
1. **Grammar**: Add `REMOVE` keyword and clause production in `cypher_gram.y`
2. **AST**: Create `cypher_remove` node with list of items (properties or labels)
3. **Transform**: 
   - Property removal: `DELETE FROM node_props_* WHERE node_id = ? AND key_id = ?`
   - Label removal: `DELETE FROM node_labels WHERE node_id = ? AND label_id = ?`

### Difference from DELETE
- `DELETE n` removes the entire node
- `REMOVE n.prop` only removes a property
- `REMOVE n:Label` only removes a label

### Example Transformation
```cypher
MATCH (n:Person {name: "Alice"}) REMOVE n.age RETURN n
```
Transforms to:
```sql
DELETE FROM node_props_int WHERE node_id = ? AND key_id = (SELECT id FROM property_keys WHERE key = 'age');
-- then SELECT to return updated node
```

## Status Updates

*To be added during implementation*