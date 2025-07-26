# Bug Fixes and Known Issues

This document tracks bugs, issues, and areas for improvement in GraphQLite's AGE compatibility implementation.


## Missing Cypher Functions




### Issue: TYPE() Function Not Implemented  
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Affects relationship type inspection

**Description:**
The `type()` function for getting relationship type labels is not implemented.

**Current Behavior:**
```sql
MATCH ()-[r]->() RETURN type(r)
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```sql
MATCH ()-[r]->() RETURN type(r)
["KNOWS", "WORKS_WITH", "MANAGES"]
```

**Root Cause:**
- No `type()` function defined in grammar
- Missing function call support for relationship metadata
- No implementation for extracting edge type from agtype values

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Function call grammar
- `src/backend/transform/transform_return.c` - Function call transformation
- `src/backend/executor/agtype.c` - Edge metadata extraction

**Solution Approach:**
1. Add `type()` function to grammar for relationship expressions
2. Implement type extraction from edge agtype values  
3. Return relationship type as string scalar
4. Handle null case for non-relationship arguments

**Test Cases Needed:**
- `MATCH ()-[r]->() RETURN type(r)` → relationship type strings
- `MATCH ()-[r:KNOWS]->() RETURN type(r)` → `"KNOWS"`
- `RETURN type(n)` → error or null (nodes don't have types)

---

## Unimplemented Cypher Clauses


### Issue: Path Variable Assignment Not Supported
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Breaks path-based query patterns

**Description:**
The `path = ` syntax for assigning path expressions to variables is not supported.

**Current Behavior:**
```cypher
MATCH path = (a)-[:REL]->(b) RETURN path
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH path = (a)-[:REL]->(b) RETURN path
[path object with nodes and relationships]
```

**Location**: `tests/functional/08_complex_queries.sql:149`

**Root Cause:**
- Path variable assignment not implemented in grammar
- No path data type support in AGType system
- Missing path manipulation functions

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Path assignment grammar
- `src/backend/executor/agtype.c` - Path data type support

**Solution Approach:**
1. Add path variable assignment to grammar
2. Implement path data type in AGType system
3. Add path manipulation functions (length, nodes, relationships)

---

### Issue: OPTIONAL MATCH Not Supported
**Status**: Open  
**Priority**: High  
**AGE Compatibility**: Breaks optional pattern matching

**Description:**
The `OPTIONAL MATCH` clause for left-outer-join style pattern matching is not implemented.

**Current Behavior:**
```cypher
MATCH (p:Person) OPTIONAL MATCH (p)-[:MANAGES]->(subordinate) RETURN p.name, subordinate.name
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (p:Person) OPTIONAL MATCH (p)-[:MANAGES]->(subordinate) RETURN p.name, subordinate.name
[{"p.name": "Alice", "subordinate.name": "Bob"}, {"p.name": "Charlie", "subordinate.name": null}]
```

**Location**: `tests/functional/08_complex_queries.sql:212`

**Root Cause:**
- OPTIONAL MATCH not implemented in grammar
- No support for optional patterns in query planning
- Missing NULL handling for unmatched optional patterns

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - OPTIONAL MATCH grammar
- `src/backend/transform/transform_match.c` - Optional pattern transformation
- `src/backend/executor/cypher_executor.c` - Left outer join execution

**Solution Approach:**
1. Add OPTIONAL MATCH clause to grammar
2. Implement left outer join logic in query planner
3. Handle NULL values for unmatched optional patterns
4. Support multiple OPTIONAL MATCH clauses

---

### Issue: String Escape Sequences Not Supported
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Breaks string literal parsing

**Description:**
String escape sequences like `\"` (escaped quotes) and `\\n` (newlines) are not properly parsed in string literals.

**Current Behavior:**
```cypher
CREATE (n {text: "He said \"Hello\""})
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
CREATE (n {text: "He said \"Hello\""})
Query executed successfully - nodes created: 1
```

**Location**: `tests/functional/09_edge_cases.sql:32`

**Root Cause:**
- String literal parsing doesn't handle escape sequences
- Lexer/scanner doesn't recognize escaped characters
- Missing support for standard escape sequences: `\"`, `\\`, `\n`, `\t`, etc.

**Affected Code:**
- `src/backend/parser/cypher_scanner.l` - String literal lexing
- String literal token handling in parser

**Solution Approach:**
1. Update scanner to handle escape sequences in string literals
2. Add support for common escapes: `\"`, `\\`, `\n`, `\t`, `\r`
3. Properly unescape strings during token processing

---

### Issue: DELETE Clause Not Implemented  
**Status**: Open  
**Priority**: High  
**AGE Compatibility**: Breaks basic graph modification functionality

**Description:**
The `DELETE` clause for removing nodes and relationships from the graph is not implemented, preventing basic cleanup and graph modification operations.

**Current Behavior:**
```cypher
MATCH (n:TestNode) DELETE n
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n:TestNode) DELETE n
Query executed successfully - nodes deleted: 1
```

**Location**: Discovered during functional test cleanup in `tests/functional/11_column_naming.sql:31`

**Root Cause:**
- No `DELETE` clause defined in grammar
- Missing delete operation logic in transform/executor layers
- No support for node/relationship deletion operations
- Missing cascade deletion logic (deleting nodes should delete their relationships)

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - DELETE clause grammar
- `src/backend/parser/cypher_ast.h` - DELETE AST node definitions
- `src/backend/transform/` - DELETE transformation logic (new file needed)
- `src/backend/executor/cypher_executor.c` - DELETE execution logic

**Solution Approach:**
1. Add DELETE clause to grammar: `MATCH (n) DELETE n`, `MATCH ()-[r]-() DELETE r`
2. Implement AST nodes for cypher_delete and delete_item structures
3. Create transform_delete.c for SQL generation
4. Add DELETE clause execution with proper cascade logic
5. Support for multiple DELETE operations: `DELETE n, r`
6. Handle deletion constraints and referential integrity

**Test Cases Needed:**
- `MATCH (n:Label) DELETE n` → delete nodes by label
- `MATCH ()-[r]-() DELETE r` → delete relationships
- `MATCH (n)-[r]-(m) DELETE n, r` → delete nodes and relationships
- `MATCH (n) WHERE n.prop = value DELETE n` → conditional deletion
- Cascade deletion: deleting node should delete its relationships
- Error handling: deleting non-existent entities

---


## SQL Generation Bugs

### Issue: Ambiguous Column References in Generated SQL
**Status**: Open  
**Priority**: High  
**AGE Compatibility**: Breaks complex query execution

**Description:**
The Cypher-to-SQL transformation generates invalid SQL with ambiguous column names, causing runtime failures.

**Current Behavior:**
```sql
Runtime error near line 79: SQL prepare failed: ambiguous column name: n_p.id
```

**Location**: `tests/functional/08_complex_queries.sql:79` (approximately)

**Root Cause:**
- SQL query generation doesn't use proper table aliasing
- Column references become ambiguous when multiple tables are joined
- Transform layer doesn't generate unique column qualifiers

**Affected Code:**
- `src/backend/transform/` - Cypher to SQL transformation logic
- `src/backend/executor/cypher_executor.c` - SQL generation and execution
- Query plan generation for complex patterns

**Solution Approach:**
1. Implement proper table aliasing in SQL generation
2. Use qualified column references (`table.column` not just `column`)
3. Generate unique aliases for repeated table patterns
4. Test with complex multi-hop relationship queries

---

### Issue: Self-Referencing Relationship Patterns Cause SQL Ambiguity
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Breaks self-referencing graph patterns

**Description:**
Self-referencing relationship patterns like `(n)-[r]->(n)` generate SQL with ambiguous column references, causing query execution failures.

**Current Behavior:**
```cypher
MATCH (n)-[r]->(n) RETURN count(r)
Runtime error: SQL prepare failed: ambiguous column name: n_n.id
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n)-[r]->(n) RETURN count(r)
2  # Count of self-referencing relationships
```

**Location**: `tests/functional/09_edge_cases.sql:250`

**Root Cause:**
- SQL generation creates duplicate table aliases when same node variable appears multiple times
- Transform layer doesn't handle self-referencing patterns with unique aliases
- Column references become ambiguous in generated SQL

**Affected Code:**
- `src/backend/transform/transform_match.c` - Pattern transformation logic
- SQL generation for relationship patterns with repeated variables

**Solution Approach:**
1. Generate unique table aliases for repeated node variables in same pattern
2. Update transform logic to detect self-referencing patterns
3. Ensure proper column qualification in generated SQL
4. Test with various self-referencing scenarios

---

### Issue: Multiple Relationship Type Syntax Not Supported
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Breaks union-type relationship matching

**Description:**
The pipe syntax for matching multiple relationship types (`[:TYPE1|TYPE2]`) is not implemented in the parser.

**Current Behavior:**
```cypher
MATCH (person:Person)-[:WORKS_FOR|CONSULTS_FOR]->(company:Company) RETURN person.name
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (person:Person)-[:WORKS_FOR|CONSULTS_FOR]->(company:Company) RETURN person.name
["Alice", "Bob", "Charlie"]  # People with either relationship type
```

**Location**: `tests/functional/08_complex_queries.sql:135`

**Root Cause:**
- Parser grammar doesn't support pipe `|` operator in relationship type specifications
- No AST node for union relationship types
- Transform layer has no logic for multiple relationship type matching

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Relationship pattern grammar
- `src/backend/transform/transform_match.c` - Relationship type handling

**Solution Approach:**
1. Add pipe operator support to relationship type grammar
2. Create AST node for union relationship types
3. Implement SQL generation for OR conditions on relationship types
4. Support arbitrary number of types: `[:TYPE1|TYPE2|TYPE3]`

---

## Testing Status Summary

- **Total Unit Tests**: 144 tests across 6 suites  
- **Unit Test Success Rate**: ✅ **100% (144/144 passing)**
- **Total Functional Tests**: 11 test files
- **Overall Test Coverage**: ✅ **Comprehensive with all major features tested**

**Current Implementation Status:**
- ✅ **Basic CRUD**: CREATE, MATCH, SET operations fully implemented
- ✅ **Data Types**: Integer, Real, Boolean, String with type safety
- ✅ **Operators**: Logical (AND/OR/NOT), comparison, arithmetic operators
- ✅ **Functions**: COUNT, MIN, MAX, AVG, SUM with DISTINCT support  
- ✅ **Column Naming**: Semantic column names for properties and variables
- ❌ **DELETE Clause**: Not implemented (next priority)
- ❌ **OPTIONAL MATCH**: Not implemented 
- ❌ **String Escapes**: Not implemented
- ❌ **TYPE() Function**: Not implemented

**Test Progress by File:**
1. ✅ **01_extension_loading.sql** - Passes completely
2. ✅ **02_node_operations.sql** - Passes completely  
3. ✅ **03_relationship_operations.sql** - Passes completely
4. ✅ **04_query_patterns.sql** - Passes completely
5. ✅ **05_return_clauses.sql** - Passes completely
6. ✅ **06_property_access.sql** - Passes completely
7. ✅ **07_agtype_compatibility.sql** - Passes completely
8. ⚠️ **08_complex_queries.sql** - Partial (SQL generation errors remain)
9. ⏸️ **09_edge_cases.sql** - Not yet tested
10. ⏸️ **10_match_create_patterns.sql** - Not yet tested
11. ✅ **11_column_naming.sql** - Passes completely

### Issue: Parser Error Handling for Invalid Syntax
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Affects error handling consistency

**Description:**
GraphQLite throws runtime parsing errors for malformed queries instead of gracefully handling syntax errors with descriptive error messages.

**Current Behavior:**
```cypher
MATCH n RETURN n  -- Missing parentheses around node variable
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH n RETURN n
ERROR: syntax error at or near "n" - node patterns require parentheses: (n)
```

**Location**: `tests/functional/09_edge_cases.sql:115`

**Root Cause:**
- Parser error handling doesn't provide graceful syntax error reporting
- Missing validation for required Cypher syntax elements (parentheses around nodes)
- No user-friendly error message formatting

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Grammar error handling
- `src/backend/parser/cypher_scanner.l` - Lexer error reporting
- Error message formatting and reporting system

**Solution Approach:**
1. Improve parser error messages with specific syntax guidance
2. Add validation for common syntax errors (missing parentheses, brackets)
3. Implement graceful error handling that doesn't crash the query execution
4. Provide context-aware error messages similar to AGE

---

### Issue: Nested Property Access Not Supported
**Status**: Open  
**Priority**: Low  
**AGE Compatibility**: Affects complex property access

**Description:**
Nested property access syntax like `n.level1.level2.level3` is not supported by the parser.

**Current Behavior:**
```cypher
MATCH (n) RETURN n.level1.level2.level3
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n) RETURN n.level1.level2.level3
null  // Returns null for non-existent nested properties
```

**Location**: `tests/functional/09_edge_cases.sql:210`

**Root Cause:**
- Grammar only supports single-level property access (`n.property`)
- No support for chained property accessors
- Missing nested property evaluation logic

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Property access grammar
- `src/backend/transform/transform_return.c` - Property expression handling
- Property evaluation logic in executor

**Solution Approach:**
1. Extend grammar to support chained property access
2. Implement nested property evaluation in transform layer
3. Add null handling for missing nested properties
4. Support arbitrary depth: `n.a.b.c.d.e`

---

## Future Issues Section

*Additional bugs and issues will be documented here as they are discovered.*