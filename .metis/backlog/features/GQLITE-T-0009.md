---
id: implement-call-procedure-syntax
level: task
title: "Implement CALL procedure syntax"
short_code: "GQLITE-T-0009"
created_at: 2025-12-24T01:49:49.503861+00:00
updated_at: 2025-12-24T01:49:49.503861+00:00
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

# Implement CALL procedure syntax

## Objective

Implement CALL clause for invoking stored procedures and built-in database functions.

## Priority
- [x] P3 - Low (extension-specific, not core query functionality)

## Business Justification
- **User Value**: Access to database metadata and admin functions
- **Business Value**: OpenCypher compliance, better tooling integration
- **Effort Estimate**: L (Large - requires procedure registry and execution framework)

## Acceptance Criteria

- [ ] `CALL db.labels()` returns all labels in the database
- [ ] `CALL db.relationshipTypes()` returns all relationship types
- [ ] `CALL db.propertyKeys()` returns all property keys
- [ ] `CALL db.schema.visualization()` returns schema info
- [ ] `CALL dbms.procedures()` lists available procedures
- [ ] YIELD clause to select output columns: `CALL db.labels() YIELD label`

## Implementation Notes

### Technical Approach
1. **Grammar**: Add `CALL procedure_name(args) [YIELD columns]` clause
2. **Procedure Registry**: Create table/struct mapping procedure names to implementations
3. **Built-in Procedures**: Implement core db.* procedures
4. **Transform**: Generate appropriate SQL queries for each procedure

### Core Procedures to Implement
| Procedure | Description |
|-----------|-------------|
| `db.labels()` | List all node labels |
| `db.relationshipTypes()` | List all relationship types |
| `db.propertyKeys()` | List all property keys |
| `db.schema.nodeTypeProperties()` | Node type and their properties |
| `db.schema.relTypeProperties()` | Relationship type and their properties |
| `dbms.procedures()` | List available procedures |

### Example Transformation
```cypher
CALL db.labels() YIELD label RETURN label
```
Becomes:
```sql
SELECT name AS label FROM labels;
```

### Extensibility
Consider allowing user-defined procedures via:
- SQL functions registered in SQLite
- Plugin system for custom procedures

## Status Updates

*To be added during implementation*