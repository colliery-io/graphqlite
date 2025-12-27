---
id: add-create-index-and-create
level: task
title: "Add CREATE INDEX and CREATE CONSTRAINT support"
short_code: "GQLITE-T-0008"
created_at: 2025-12-24T01:49:49.354673+00:00
updated_at: 2025-12-24T01:49:49.354673+00:00
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

# Add CREATE INDEX and CREATE CONSTRAINT support

## Objective

Add support for CREATE INDEX and CREATE CONSTRAINT schema commands for query optimization and data integrity.

## Priority
- [x] P3 - Low (SQLite handles indexing automatically in many cases)

## Business Justification
- **User Value**: Explicit control over indexing and constraints
- **Business Value**: OpenCypher compliance, performance optimization
- **Effort Estimate**: M (Medium)

## Acceptance Criteria

- [ ] `CREATE INDEX ON :Person(name)` creates index on Person.name
- [ ] `CREATE INDEX FOR (n:Person) ON (n.name)` (newer syntax)
- [ ] `CREATE CONSTRAINT ON (n:Person) ASSERT n.id IS UNIQUE`
- [ ] `CREATE CONSTRAINT ON (n:Person) ASSERT EXISTS (n.name)` (property existence)
- [ ] `DROP INDEX` and `DROP CONSTRAINT` supported
- [ ] `SHOW INDEXES` and `SHOW CONSTRAINTS` for introspection

## Implementation Notes

### Technical Approach
1. **Grammar**: Add schema command productions
2. **Transform**: Generate SQLite `CREATE INDEX` and constraint triggers
3. **Metadata**: Store constraint info in metadata tables for `SHOW` commands

### SQLite Mapping
```cypher
CREATE INDEX ON :Person(name)
```
Becomes:
```sql
CREATE INDEX idx_person_name ON node_props_text(value) 
WHERE key_id = (SELECT id FROM property_keys WHERE key = 'name')
  AND node_id IN (SELECT node_id FROM node_labels WHERE label_id = (SELECT id FROM labels WHERE name = 'Person'));
```

### Unique Constraint
```cypher
CREATE CONSTRAINT ON (n:Person) ASSERT n.id IS UNIQUE
```
Becomes:
```sql
CREATE UNIQUE INDEX unique_person_id ON node_props_text(value) WHERE ...
-- Plus trigger to enforce across property tables
```

### Challenges
- Property values spread across 4 tables (text, int, real, bool)
- Need to create indexes on each relevant table
- Unique constraints need triggers to check across tables

## Investigation Notes (Dec 2025)

### Indexing Already Complete

All properties and edges are already fully indexed in `cypher_schema.c`:

| Table | Index | Columns |
|-------|-------|---------|
| edges | idx_edges_source | (source_id, type) |
| edges | idx_edges_target | (target_id, type) |
| edges | idx_edges_type | (type) |
| node_labels | idx_node_labels_label | (label, node_id) |
| node_props_* | idx_node_props_*_key_value | (key_id, value, node_id) |
| edge_props_* | idx_edge_props_*_key_value | (key_id, value, edge_id) |

**Conclusion**: `CREATE INDEX` syntax provides no performance benefit - SQLite already uses these indexes for property lookups.

### UNIQUE Constraint Complexity

SQLite partial indexes don't support subqueries in WHERE clauses, so label-scoped unique indexes aren't possible directly.

**Option 1: Triggers** - Need 4 triggers per constraint (one per property table) plus 4 more for UPDATE. Complex and slow.

**Option 2: Denormalized column** - Add computed column to nodes table, create unique index on it. Simpler indexing but requires maintaining denormalized values.

### EXISTS Constraint Timing Problem

Labels are inserted before properties in CREATE, so a trigger on `node_labels` fires before properties exist. Would need either:
- Deferred validation (SQLite doesn't support natively)
- Application-level validation in executor after CREATE completes

### Recommendation

Hold off on this feature. Complexity vs. value tradeoff isn't favorable:
- Indexing: Already done automatically
- UNIQUE: Complex trigger approach or schema changes needed
- EXISTS: Timing issues require application-level handling

Revisit if users specifically request constraint enforcement for data integrity.