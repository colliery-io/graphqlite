---
id: add-parameterized-query-support-to
level: task
title: "Add parameterized query support to prevent SQL injection"
short_code: "GQLITE-T-0013"
created_at: 2025-12-24T15:24:24.833594+00:00
updated_at: 2025-12-24T16:21:29.239377+00:00
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

# Add parameterized query support to prevent SQL injection

## Objective

Add support for parameterized Cypher queries to prevent SQL injection vulnerabilities and eliminate the need for manual string escaping in client code.

## Backlog Item Details

### Type
- [x] Feature - New functionality or enhancement  

### Priority
- [x] P1 - High (important for user experience)

### Business Justification
- **User Value**: Users can safely pass untrusted input without manual escaping. Queries become cleaner and more readable.
- **Business Value**: Security is a baseline expectation for any database. Without parameterized queries, GraphQLite cannot be safely used with user-provided data.
- **Effort Estimate**: L

## Problem Statement

Currently, users must manually escape all string values before embedding them in Cypher queries:

```python
# Current workaround (from nanographrag demo)
def _escape_string(self, s: str) -> str:
    return (s.replace("\\", "\\\\")
             .replace("'", "\\'")
             .replace('"', '\\"')
             .replace("\n", " ")
             .replace("\r", " ")
             .replace("\t", " "))

# Usage - error-prone and ugly
query = f"CREATE (n:Person {{name: '{self._escape_string(name)}'}})"
```

This is error-prone, verbose, and a security risk if escaping is forgotten.

## Desired API

```python
# Ideal API with parameters
result = db.cypher(
    "CREATE (n:Person {name: $name, age: $age})",
    {"name": "O'Brien", "age": 30}
)

# Or with positional parameters
result = db.cypher(
    "MATCH (n {id: ?}) RETURN n",
    ["node-123"]
)
```

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] Support named parameters with `$param` syntax in Cypher queries
- [ ] Parameters are properly bound at the SQLite level (true parameterization, not string interpolation)
- [ ] String values with quotes, backslashes, and special characters work correctly
- [ ] NULL values can be passed as parameters
- [ ] Numeric and boolean parameters work correctly
- [ ] Python bindings expose parameter dict argument
- [ ] C API provides parameter binding functions
- [ ] Unit tests cover injection attempts that should be safely handled

## Implementation Notes

### Technical Approach
1. Extend parser to recognize `$identifier` as parameter placeholders
2. During SQL transform, convert `$param` to SQLite's `?` placeholders
3. Track parameter order mapping during transform
4. Pass parameters through to `sqlite3_bind_*` functions
5. Update Python/Rust bindings to accept parameter dict/map

### Dependencies
- Parser changes to recognize parameter syntax
- Transform layer changes to track and map parameters
- Executor changes to bind parameters to SQLite statements

### Risk Considerations
- Parameter syntax must not conflict with existing Cypher syntax
- Need to handle type coercion between Cypher types and SQLite types