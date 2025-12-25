---
id: implement-merge-clause-for-proper
level: task
title: "Implement MERGE clause for proper upsert semantics"
short_code: "GQLITE-T-0014"
created_at: 2025-12-24T15:24:24.919195+00:00
updated_at: 2025-12-24T18:19:13.801858+00:00
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

# Implement MERGE clause for proper upsert semantics

## Objective

Implement the Cypher MERGE clause to provide atomic "match or create" semantics, eliminating the need for application-level check-then-create patterns.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P1 - High (important for user experience)

### Business Justification
- **User Value**: Single atomic operation for upserts instead of multiple queries. Standard Cypher compatibility.
- **Business Value**: Essential for idempotent data loading, ETL pipelines, and any scenario where data may be re-processed.
- **Effort Estimate**: L

## Problem Statement

Currently, users must implement upsert logic manually with multiple queries:

```python
# Current workaround (from nanographrag demo)
def upsert_node(self, node_id: str, node_data: dict, label: str = "Entity"):
    if self.has_node(node_id):
        # Update existing - set each property one by one
        for k, v in node_data.items():
            self._conn.cypher(f"MATCH (n {{id: '{node_id}'}}) SET n.{k} = {val}")
    else:
        self._conn.cypher(f"CREATE (n:{label} {{{prop_str}}})")
```

This is:
- Not atomic (race conditions possible)
- Inefficient (multiple round trips)
- Verbose and error-prone
- Non-standard Cypher

## Desired API

```cypher
-- Create node if not exists, update if exists
MERGE (n:Person {id: 'alice'})
ON CREATE SET n.created = timestamp()
ON MATCH SET n.updated = timestamp()
SET n.name = 'Alice'
RETURN n

-- Merge relationships
MATCH (a:Person {id: 'alice'}), (b:Person {id: 'bob'})
MERGE (a)-[r:KNOWS]->(b)
ON CREATE SET r.since = date()
RETURN r
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `MERGE (n:Label {props})` creates node if no match, returns existing if match
- [ ] `MERGE (a)-[r:TYPE]->(b)` works for relationships
- [ ] `ON CREATE SET` clause executes only when creating
- [ ] `ON MATCH SET` clause executes only when matching existing
- [ ] Both `ON CREATE` and `ON MATCH` can be combined
- [ ] MERGE is atomic (no race conditions)
- [ ] Works with node patterns containing labels and properties
- [ ] Works with relationship patterns
- [ ] Unit tests cover create, match, and combined scenarios

## Implementation Notes

### Technical Approach

1. Add MERGE, ON CREATE, ON MATCH tokens to lexer
2. Add grammar rules for MERGE clause in parser
3. Create `AST_NODE_MERGE` and related AST nodes
4. Transform to SQL using `INSERT OR IGNORE` + conditional UPDATE, or use SQLite's UPSERT (`ON CONFLICT`)
5. Handle relationship MERGE by checking edges table

### SQL Translation Strategy

```sql
-- MERGE (n:Person {id: 'alice'}) ON CREATE SET n.name = 'Alice'
INSERT INTO nodes (label) 
SELECT 'Person' WHERE NOT EXISTS (
    SELECT 1 FROM node_props_text 
    WHERE key = 'id' AND value = 'alice'
);
-- Then SET properties...
```

Or leverage SQLite's native UPSERT:
```sql
INSERT INTO nodes (label) VALUES ('Person')
ON CONFLICT(...) DO UPDATE SET ...
```

### Dependencies
- Parser changes for new syntax
- New transform module: `transform_merge.c`
- May benefit from GQLITE-T-0013 (parameterized queries)

### Risk Considerations
- Atomicity requires careful transaction handling
- Matching semantics must align with Neo4j behavior
- Relationship MERGE is more complex than node MERGE