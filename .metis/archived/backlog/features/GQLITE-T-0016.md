---
id: support-relationship-property
level: task
title: "Support relationship property updates"
short_code: "GQLITE-T-0016"
created_at: 2025-12-24T15:24:25.101289+00:00
updated_at: 2025-12-24T18:31:18.559294+00:00
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

# Support relationship property updates

## Objective

Enable updating properties on existing relationships using `SET` clause, matching the behavior already supported for nodes.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P2 - Medium (nice to have)

### Business Justification
- **User Value**: Relationships often need metadata updates (e.g., updating a "weight" or "lastAccessed" property). Currently requires delete + recreate.
- **Business Value**: Standard Cypher feature expected by users migrating from Neo4j. Reduces friction in adoption.
- **Effort Estimate**: M

## Problem Statement

Currently, relationship properties cannot be updated after creation. The nanographrag demo works around this by simply skipping updates:

```python
# Current workaround - just skip if edge exists
def upsert_edge(self, source_id: str, target_id: str, edge_data: dict, rel_type: str):
    if self.has_edge(source_id, target_id):
        return  # Skip - edge already exists, can't update it
    
    # Create new edge
    self._conn.cypher(f"MATCH (a), (b) CREATE (a)-[r:{rel_type} {{...}}]->(b)")
```

This means:
- Edge properties are immutable after creation
- No way to update relationship weights, timestamps, or other metadata
- Workaround requires delete + recreate (loses edge identity)

## Desired API

```cypher
-- Update relationship property
MATCH (a:Person)-[r:KNOWS]->(b:Person)
WHERE a.name = 'Alice' AND b.name = 'Bob'
SET r.since = 2024, r.strength = 0.9
RETURN r

-- Add new property to relationship
MATCH ()-[r:FOLLOWS]->()
SET r.active = true

-- Remove property from relationship
MATCH (a)-[r]->(b)
REMOVE r.deprecated
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `SET r.property = value` updates existing relationship property
- [ ] `SET r.property = value` adds new property if it doesn't exist
- [ ] `SET r = {props}` replaces all properties on relationship
- [ ] `SET r += {props}` merges properties (keeps existing, adds/updates new)
- [ ] `REMOVE r.property` removes a property from relationship
- [ ] Updates work with relationship variables bound in MATCH
- [ ] Updates work when relationship is matched by type or properties
- [ ] Unit tests cover update, add, replace, and remove scenarios

## Implementation Notes

### Technical Approach

1. Extend `transform_set.c` to handle relationship property updates
2. Generate SQL UPDATE statements for `edge_props_*` tables
3. Handle the relationship variable resolution (edge rowid)
4. Support all SET variants: `=`, `+=`, and property replacement

### SQL Translation

```cypher
MATCH (a)-[r:KNOWS]->(b) WHERE a.id = 1 SET r.weight = 0.5
```

Translates to:
```sql
-- Update existing property
UPDATE edge_props_real 
SET value = 0.5 
WHERE edge_id = (SELECT rowid FROM edges WHERE source_id = 1 AND type = 'KNOWS')
  AND key = 'weight';

-- Or insert if not exists
INSERT OR REPLACE INTO edge_props_real (edge_id, key, value)
SELECT e.rowid, 'weight', 0.5 
FROM edges e WHERE e.source_id = 1 AND e.type = 'KNOWS';
```

### Dependencies
- Relationship variable binding in MATCH must provide edge rowid
- Property type detection for correct `edge_props_*` table

### Risk Considerations
- Must handle type changes (property was string, now integer)
- Performance with large relationship sets