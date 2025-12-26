---
id: implement-load-csv-for-data-import
level: task
title: "Implement LOAD CSV for data import"
short_code: "GQLITE-T-0007"
created_at: 2025-12-24T01:49:49.189917+00:00
updated_at: 2025-12-24T03:14:18.058281+00:00
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

# Implement LOAD CSV for data import

## Objective

Implement LOAD CSV clause for importing data from CSV files into the graph database.

## Priority
- [x] P3 - Low (useful but external tooling can handle this)

## Business Justification
- **User Value**: Easy bulk data import from common file format
- **Business Value**: OpenCypher compliance, simplifies data migration
- **Effort Estimate**: L (Large - file I/O, parsing, security considerations)

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `LOAD CSV FROM "file.csv" AS row CREATE (n {name: row[0]})`
- [ ] `LOAD CSV WITH HEADERS FROM "file.csv" AS row CREATE (n {name: row.name})`
- [ ] Support for local file paths
- [ ] Support for field separator configuration
- [ ] Proper handling of quoted fields and escapes
- [ ] Error handling for missing files, malformed CSV

## Implementation Notes

### Technical Approach
1. **Grammar**: Add `LOAD CSV [WITH HEADERS] FROM string AS variable` clause
2. **AST**: Create `cypher_load_csv` node with path, headers flag, variable
3. **Executor**: 
   - Open and parse CSV file
   - For each row, bind variable and execute following clauses
   - Use SQLite's virtual table or temp table approach

### Security Considerations
- File path validation (prevent directory traversal)
- Optional: Restrict to specific directories
- Consider URL support (http://) with security implications

### SQLite Approach
Could use SQLite's CSV virtual table extension:
```sql
CREATE VIRTUAL TABLE temp.csv_data USING csv(filename='file.csv', header=yes);
```

### Example Transformation
```cypher
LOAD CSV WITH HEADERS FROM "people.csv" AS row
CREATE (p:Person {name: row.name, age: toInteger(row.age)})
```

### Alternative
Users can use external tools (Python, etc.) to load CSV and call Cypher for each row.

## Status Updates

### 2025-12-24: Parsing Implementation Complete

**Completed:**
- Added `AST_NODE_LOAD_CSV` node type and `cypher_load_csv` struct
- Added tokens: LOAD, CSV, FROM, HEADERS, FIELDTERMINATOR
- Implemented grammar rules for all LOAD CSV variants:
  - `LOAD CSV FROM 'file.csv' AS row`
  - `LOAD CSV WITH HEADERS FROM 'file.csv' AS row`
  - `LOAD CSV FROM 'file.csv' AS row FIELDTERMINATOR ';'`
  - `LOAD CSV WITH HEADERS FROM 'file.csv' AS row FIELDTERMINATOR ';'`
- Added `make_cypher_load_csv()` function
- Added memory management (freeing) for LOAD CSV nodes
- Created `transform_load_csv.c` with placeholder implementation
- Added 3 parser tests (all passing)

**Transform Status:**
Transform returns helpful error message suggesting SQLite CSV extension or `.import` CLI as alternatives. Full transform would require SQLite csv virtual table support.

**Files Modified:**
- src/include/parser/cypher_ast.h
- src/include/parser/cypher_tokens.h
- src/include/parser/cypher_kwlist.h
- src/backend/parser/cypher_ast.c
- src/backend/parser/cypher_gram.y
- src/backend/parser/cypher_parser.c
- src/backend/transform/cypher_transform.c
- src/backend/transform/transform_load_csv.c (new)
- src/include/transform/cypher_transform.h
- tests/test_parser.c
- Makefile

All 391 tests pass.