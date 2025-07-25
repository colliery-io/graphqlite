# Bug Fixes and Known Issues

This document tracks bugs, issues, and areas for improvement in GraphQLite's AGE compatibility implementation.

## Column Naming Issues

### Issue: Generic Column Names Instead of Semantic Names
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Affects compatibility

**Description:**
Property access queries return generic column names (`column_0`, `column_1`) instead of meaningful names based on the query structure.

**Current Behavior:**
```sql
MATCH (n:Person) RETURN n.age
[{"column_0": 30}]
```

**Expected AGE-Compatible Behavior:**
```sql
MATCH (n:Person) RETURN n.age
[{"age": 30}]
```

**Root Cause:**
The `build_query_results()` function in `cypher_executor.c` falls back to generic column names when it doesn't extract proper semantic names from property access expressions (`AST_NODE_PROPERTY`).

**Affected Code:**
- `src/backend/executor/cypher_executor.c:586-594` - Column name generation logic
- Property access expressions don't populate meaningful column names

**Solution Approach:**
1. Extract property name from `cypher_property` AST node for column naming
2. Use variable name for identifier expressions (`n` → `"n"`)
3. Use explicit aliases when provided (`n.age AS person_age` → `"person_age"`)
4. Only fall back to `column_N` for complex expressions without clear names

**Test Cases Needed:**
- `RETURN n.age` → `{"age": 30}`
- `RETURN n.age AS person_age` → `{"person_age": 30}`
- `RETURN n` → `{"n": {...}}`
- `RETURN n, n.age` → `{"n": {...}, "age": 30}`

---

## Missing Cypher Functions



### Issue: COUNT() Function Not Implemented
**Status**: Open  
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

### Issue: COUNT() Aggregate Function Not Implemented
**Status**: Open  
**Priority**: High  
**AGE Compatibility**: Breaks basic aggregate functionality

**Description:**
Aggregate functions (`count()`, `MIN()`, `MAX()`, `avg()`) and utility functions (`type()`, `length()`) are not implemented, causing parse errors in common queries.

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

### Issue: SET Clause Not Implemented  
**Status**: Open  
**Priority**: High  
**AGE Compatibility**: Breaks property update functionality

**Description:**
The `SET` clause for updating node and relationship properties is not implemented.

**Current Behavior:**
```cypher
MATCH (n:TestNode) SET n.newprop = 42
Runtime error: Failed to parse query
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n:TestNode) SET n.newprop = 42
Query executed successfully - nodes modified: 1
```

**Location**: `tests/functional/06_property_access.sql:158`

**Root Cause:**
- No `SET` clause defined in grammar
- Missing property update logic in transform/executor layers
- No support for property modification operations

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - SET clause grammar
- `src/backend/transform/` - SET operation transformation
- `src/backend/executor/cypher_executor.c` - Property update execution

**Solution Approach:**
1. Add SET clause to grammar after MATCH
2. Implement property update transformation logic
3. Add database update operations for node/edge properties
4. Support multiple SET operations: `SET n.prop1 = val1, n.prop2 = val2`

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

## Testing Status Summary

- **Total Functional Tests**: 10
- **Tests with Parser Errors**: 4 (NOT label syntax, ORDER BY DESC, aggregate functions, SET clause)
- **Tests with SQL Generation Errors**: 1 (ambiguous column references)
- **Tests Passing Completely**: 5-6
- **Schema Issues Fixed**: 1 (element_id → edge_id)
- **Overall Coverage**: ~70% functional tests run partially, ~50% complete successfully

**Test Progress by File:**
1. ✅ **01_extension_loading.sql** - Passes completely
2. ✅ **02_node_operations.sql** - Passes completely  
3. ✅ **03_relationship_operations.sql** - Passes completely (after element_id fix)
4. ⚠️ **04_query_patterns.sql** - Mostly passes (NOT label syntax issue)
5. ⚠️ **05_return_clauses.sql** - Mostly passes (ORDER BY DESC, count() issues)
6. ⚠️ **06_property_access.sql** - Mostly passes (SET clause, MIN/MAX issues)
7. ✅ **07_agtype_compatibility.sql** - Passes completely
8. ❌ **08_complex_queries.sql** - SQL generation error (ambiguous columns)
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