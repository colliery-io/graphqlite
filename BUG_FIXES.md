# Bug Fixes and Known Issues

This document tracks bugs, issues, and areas for improvement in GraphQLite's AGE compatibility implementation.

## Column Naming Issues

### Issue: Generic Column Names Instead of Semantic Names
**Status**: ✅ COMPLETED  
**Priority**: Medium  
**AGE Compatibility**: Restored semantic column naming

**Description:**
Property access queries were returning generic column names (`column_0`, `column_1`) instead of meaningful names based on the query structure.

**Previous Behavior:**
```sql
MATCH (n:Person) RETURN n.age
[{"column_0": 30}]
```

**Current AGE-Compatible Behavior:**
```sql
MATCH (n:Person) RETURN n.age
[{"age": 30}]
```

**Root Cause (Fixed):**
The `build_query_results()` function in `cypher_executor.c` was falling back to generic column names because it didn't properly handle `AST_NODE_PROPERTY` and `AST_NODE_IDENTIFIER` cases for semantic naming.

**Fix Applied:**
Enhanced column name generation logic in `cypher_executor.c:633-653` to:
1. Extract property name from `cypher_property` AST node (e.g., `n.age` → `"age"`)
2. Use variable name for identifier expressions (e.g., `n` → `"n"`)
3. Use explicit aliases when provided (e.g., `n.age AS person_age` → `"person_age"`)
4. Only fall back to `column_N` for complex expressions without clear names

**Test Status**: ✅ All column naming tests passing - 5 comprehensive CUnit tests added

**Test Coverage:**
- **Property Access**: `RETURN p.name, p.age` → `{"name": "Alice", "age": 30}`
- **Variable Access**: `RETURN p` → `{"p": {...}}`
- **Explicit Aliases**: `RETURN p.name AS person_name` → `{"person_name": "Alice"}`
- **Mixed Types**: `RETURN p, p.name, p.age` → `{"p": {...}, "name": "Alice", "age": 30}`
- **Complex Expressions**: `RETURN count(p)` → `{"column_0": 5}` (fallback to generic)

---

## Missing Cypher Functions



### Issue: COUNT() Function Not Implemented
**Status**: ✅ COMPLETED  
**Priority**: High  
**AGE Compatibility**: Blocks basic aggregate functionality

**Description:**
The `COUNT()` aggregate function is not implemented, causing parse errors in common Cypher queries.

**Current Behavior:**
```sql
MATCH (n) RETURN count(n)
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```sql
MATCH (n) RETURN count(n)
5
```

**Root Cause:**
- No `COUNT` function defined in grammar (`cypher_gram.y`)
- No aggregate function support in expression parsing
- Missing implementation in executor for aggregate operations

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Grammar rules for function calls
- `src/backend/transform/transform_return.c` - Expression transformation
- `src/backend/executor/cypher_executor.c` - Aggregate execution logic

**Solution Approach:**
1. Add `COUNT` function to grammar as function call expression
2. Implement aggregate detection in transform layer
3. Add aggregate execution logic to handle GROUP BY semantics
4. Support both `count(*)` and `count(variable)` syntax

**Test Cases Needed:**
- `RETURN count(*)` → integer count
- `RETURN count(n)` → integer count  
- `RETURN n.name, count(*)` → grouped results
- `RETURN count(r)` → relationship count

---

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

## Parser Syntax Issues

### Issue: NOT Label Syntax Not Supported
**Status**: ✅ COMPLETED  
**Priority**: High  
**AGE Compatibility**: Breaks standard Cypher filtering

**Description:**
The `NOT n:Label` syntax for excluding nodes by label is valid Cypher but fails to parse.

**Current Behavior:**
```cypher
MATCH (n) WHERE NOT n:Person AND NOT n:Company RETURN n
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n) WHERE NOT n:Person AND NOT n:Company RETURN n
[{"id": 1, "label": "Other", "properties": {...}}::vertex]
```

**Location**: `tests/functional/04_query_patterns.sql:226`

**Root Cause:**
- Parser doesn't recognize `NOT` operator with label expressions
- Label expression parsing may be incomplete in WHERE clauses
- Missing grammar rules for negated label checks

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - WHERE clause expression grammar
- Label expression parsing logic

**Solution Approach:**
1. Add NOT operator support for label expressions in grammar
2. Implement negated label checking in WHERE clause evaluation
3. Test with complex combinations: `NOT n:A AND NOT n:B OR n:C`

---

### Issue: ORDER BY DESC Keyword Not Supported
**Status**: ✅ COMPLETED  
**Priority**: High  
**AGE Compatibility**: Breaks standard sorting functionality

**Description:**
The `DESC` keyword in ORDER BY clauses is not recognized by the parser.

**Current Behavior:**
```cypher
MATCH (n:Person) RETURN n.name ORDER BY n.age DESC
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n:Person) RETURN n.name ORDER BY n.age DESC
["David", "Alice", "Bob"]  # sorted by age descending
```

**Location**: `tests/functional/05_return_clauses.sql:59`

**Root Cause:**
- Grammar only supports implicit ascending sort
- Missing `DESC` and `ASC` keywords in ORDER BY grammar rules
- Sort direction not implemented in executor

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - ORDER BY clause grammar
- `src/backend/transform/transform_return.c` - ORDER BY transformation
- Sort execution logic

**Solution Approach:**
1. Add `DESC` and `ASC` keywords to ORDER BY grammar
2. Implement sort direction flag in AST nodes
3. Update executor to handle both ascending and descending sorts
4. Test with multiple columns: `ORDER BY n.name ASC, n.age DESC`

---

## Schema Bug Fixes (Completed)

### Issue: Incorrect Column Reference in Edge Property Queries
**Status**: Fixed  
**Priority**: Medium  
**AGE Compatibility**: Restored proper edge property queries

**Description:**
Edge property verification queries used incorrect column name `element_id` instead of `edge_id`.

**Error:**
```sql
Parse error near line 187: no such column: element_id
```

**Location**: `tests/functional/03_relationship_operations.sql:188-194`

**Root Cause:**
Edge property tables use `edge_id` as foreign key column, not `element_id`.

**Fix Applied:**
Changed all references from `element_id` to `edge_id` in edge property queries.

**Test Status**: ✅ Fixed and verified

---

### Issue: Implement AND/OR Logical Operators in WHERE Clauses
**Status**: ✅ COMPLETED  
**Priority**: High  
**AGE Compatibility**: Critical for compound filtering

**Description:**
Logical operators `AND` and `OR` were not implemented, preventing compound filtering in WHERE clauses.

**Previous Behavior:**
```cypher
MATCH (n) WHERE NOT n:Person AND NOT n:Company RETURN n
Runtime error: Failed to parse query
```

**Current AGE-Compatible Behavior:**
```cypher
MATCH (n) WHERE NOT n:Person AND NOT n:Company RETURN n
[{"id": 1, "properties": {...}}::vertex]
```

**Root Cause (Fixed):**
- Missing AND/OR operators in parser grammar
- No binary operation AST nodes or transform logic
- Incomplete WHERE clause implementation

**Fix Applied:**
1. Added binary operation AST nodes for AND, OR, comparison, and arithmetic operators
2. Updated parser grammar with proper operator precedence
3. Implemented SQL generation for all binary operations
4. Added comprehensive transform support for complex logical expressions

**Test Status**: ✅ All parser and transform tests passing (109/112 overall)

---

### Issue: COUNT() Aggregate Function Not Implemented
**Status**: ✅ COMPLETED  
**Priority**: High  
**AGE Compatibility**: Breaks basic aggregate functionality

**Description:**
Aggregate functions (`count()`, `MIN()`, `MAX()`, `avg()`) and utility functions (`type()`, `length()`) are not implemented, causing parse errors in common queries.

**✅ COMPLETED FUNCTIONS:**
- `COUNT()` - Both `count(*)` and `count(variable)` syntax supported
- `COUNT(DISTINCT variable)` - Distinct counting implemented
- `MIN()`, `MAX()`, `AVG()`, `SUM()` - All basic aggregate functions working

**REMAINING WORK:**
- `type()` function for relationship types still not implemented
- `length()` function still not implemented

**Current Behavior:**
```cypher
MATCH (n) RETURN count(n)
Runtime error: Failed to parse query

MATCH (n:Person) RETURN MIN(n.age), MAX(n.age)  
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n) RETURN count(n)
5

MATCH (n:Person) RETURN MIN(n.age), MAX(n.age)
[25, 35]
```

**Locations**: 
- `tests/functional/05_return_clauses.sql:193` (count)
- `tests/functional/06_property_access.sql:190` (MIN/MAX)

**Root Cause:**
- No `count()` function defined in grammar
- Missing aggregate function support in parser/executor
- No GROUP BY semantics implementation

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Function call grammar
- `src/backend/transform/transform_return.c` - Aggregate transformation
- `src/backend/executor/cypher_executor.c` - Aggregate execution

**Solution Approach:**
1. Add aggregate functions to grammar: `count()`, `MIN()`, `MAX()`, `AVG()`, `SUM()`
2. Implement aggregate detection and execution logic
3. Support both `count(*)` and `count(variable)` syntax
4. Handle GROUP BY semantics for aggregate operations

---

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

### Issue: SET Clause Not Implemented  
**Status**: ✅ COMPLETED  
**Priority**: High  
**AGE Compatibility**: Breaks property update functionality

**Description:**
The `SET` clause for updating node and relationship properties was not implemented.

**Previous Behavior:**
```cypher
MATCH (n:TestNode) SET n.newprop = 42
Runtime error: Failed to parse query
```

**Current AGE-Compatible Behavior:**
```cypher
MATCH (n:TestNode) SET n.newprop = 42
Query executed successfully - properties set: 1
```

**Location**: `tests/functional/06_property_access.sql:158`

**Root Cause (Fixed):**
- No `SET` clause defined in grammar
- Missing property update logic in transform/executor layers
- No support for property modification operations

**Fix Applied:**
1. Added SET clause to grammar with support for multiple property assignments
2. Implemented AST nodes for cypher_set and cypher_set_item
3. Created transform_set.c for SQL generation
4. Added SET clause execution in cypher_executor.c
5. Support for multiple SET operations: `SET n.prop1 = val1, n.prop2 = val2`
6. Added comprehensive unit and functional tests

**Test Status**: ✅ All SET clause tests passing - 133/133 unit tests (100% success rate)

**Comprehensive Data Type Implementation:**
- **Integer Support**: Full support for positive, negative, zero, and large integer values with numeric comparisons
- **Real/Float Support**: Precise decimal handling (3.14159, 0.001, -2.5) with floating-point comparisons  
- **Boolean Support**: True/false values with proper boolean logic and comparisons
- **String Support**: Complete string handling including empty strings, spaces, and special characters
- **Mixed Type Operations**: Support for setting multiple properties of different types in single query
- **Type Overwrite**: Proper handling when changing property types (string→int→boolean) with cleanup

**Advanced Features:**
- **WHERE Clause Integration**: Full filtering support with all data types: `MATCH (p:Person) WHERE p.age > 28 SET p.senior = true`
- **Property Access Context**: Smart handling of comparison vs display contexts for optimal performance
- **Type Safety**: Properties are stored in correct typed tables and retrieved with proper type preservation
- **Batch Operations**: Support for multiple SET operations: `SET n.prop1 = val1, n.prop2 = val2, n.prop3 = val3`

**Critical Bug Fixes:**
- Fixed WHERE clause numeric comparison issue (string comparison '25' > '28' vs numeric 25 > 28)
- Fixed boolean property access in WHERE clauses (added boolean table to COALESCE)
- Fixed string property access in comparisons (removed numeric-only restriction)
- Fixed property type overwrite issue (cleanup old values from other type tables)

**Test Coverage:**
- **Unit Tests**: 12 comprehensive SET clause tests covering all data types and edge cases
- **Functional Tests**: 8 additional functional test scenarios including type conversion and large values
- **Integration Tests**: Full MATCH+SET+WHERE clause combinations with complex filtering

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

- **Total Unit Tests**: 133 tests across 6 suites
- **Unit Test Success Rate**: ✅ **100% (133/133 passing)**
- **Total Functional Tests**: 10+ (extended with comprehensive data type testing)
- **Major Parser Issues Fixed**: 4 (NOT label syntax, ORDER BY DESC, aggregate functions, SET clause)
- **Critical Bug Fixes**: 4 (property type overwrite, boolean/string comparisons, numeric WHERE clauses)
- **Schema Issues Fixed**: 1 (element_id → edge_id)
- **Overall Unit Test Coverage**: ✅ **Complete with comprehensive edge case testing**

**Major Functionality Status:**
- ✅ **SET Clause**: Complete implementation with all data types and WHERE integration
- ✅ **ORDER BY DESC/ASC**: Complete with proper sort direction support
- ✅ **Logical Operators**: AND/OR/NOT fully implemented with proper precedence
- ✅ **Aggregate Functions**: COUNT, MIN, MAX, AVG, SUM with DISTINCT support
- ✅ **Data Types**: Integer, Real, Boolean, String with type safety and conversions
- ✅ **Property Access**: Complete with comparison context awareness
- ✅ **WHERE Clauses**: Full filtering with numeric/string/boolean comparisons

**Test Progress by File:**
1. ✅ **01_extension_loading.sql** - Passes completely
2. ✅ **02_node_operations.sql** - Passes completely  
3. ✅ **03_relationship_operations.sql** - Passes completely (after element_id fix)
4. ✅ **04_query_patterns.sql** - Passes completely (NOT label syntax fixed)
5. ✅ **05_return_clauses.sql** - Passes completely (ORDER BY DESC, count() fixed)
6. ✅ **06_property_access.sql** - Passes completely (SET clause, data types implemented)
7. ✅ **07_agtype_compatibility.sql** - Passes completely
8. ⚠️ **08_complex_queries.sql** - Partial (SQL generation errors remain)
9. ⏸️ **09_edge_cases.sql** - Not yet tested
10. ⏸️ **10_match_create_patterns.sql** - Not yet tested

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