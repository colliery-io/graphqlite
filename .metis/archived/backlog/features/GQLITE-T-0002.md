---
id: support-multiple-labels-syntax-a-b
level: task
title: "Support multiple labels syntax (:A:B)"
short_code: "GQLITE-T-0002"
created_at: 2025-12-24T01:49:48.522655+00:00
updated_at: 2025-12-24T02:41:12.493315+00:00
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

# Support multiple labels syntax (:A:B)

## Objective

Support multiple labels on nodes using the `(:Label1:Label2)` syntax, allowing nodes to have multiple type classifications.

## Priority
- [x] P1 - High (common OpenCypher pattern)

## Business Justification
- **User Value**: Enables richer data modeling with nodes belonging to multiple categories
- **Business Value**: Standard OpenCypher feature, improves compatibility with Neo4j queries
- **Effort Estimate**: S (Small)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `CREATE (n:Person:Employee {name: "Alice"})` creates node with both labels
- [ ] `MATCH (n:Person:Employee)` matches only nodes with BOTH labels
- [ ] `MATCH (n:Person)` matches nodes that have Person label (even if they have others)
- [ ] `labels(n)` returns array of all labels: `["Person", "Employee"]`
- [ ] Works in CREATE, MATCH, and MERGE clauses

## Implementation Notes

### Technical Approach
1. **Grammar**: Modify `node_pattern` in `cypher_gram.y` to accept multiple labels (`:Label1:Label2`)
2. **AST**: Change `cypher_node_pattern.label` from `char*` to `ast_list*` of labels
3. **Schema**: Current schema already supports multiple labels via `node_labels` junction table
4. **Transform CREATE**: Insert multiple rows into `node_labels` table
5. **Transform MATCH**: Generate `AND EXISTS` conditions for each required label

### Current Schema (already supports this)
```sql
CREATE TABLE node_labels (
    node_id INTEGER,
    label_id INTEGER,
    PRIMARY KEY (node_id, label_id)
);
```

### Example Transformation
```cypher
MATCH (n:Person:Employee) RETURN n
```
Transforms to:
```sql
SELECT ... FROM nodes n
WHERE EXISTS (SELECT 1 FROM node_labels nl JOIN labels l ON nl.label_id = l.id 
              WHERE nl.node_id = n.id AND l.name = 'Person')
  AND EXISTS (SELECT 1 FROM node_labels nl JOIN labels l ON nl.label_id = l.id 
              WHERE nl.node_id = n.id AND l.name = 'Employee')
```

## Status Updates

*To be added during implementation*