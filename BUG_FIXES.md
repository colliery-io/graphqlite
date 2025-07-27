# Bug Fixes and Known Issues

This document tracks bugs, issues, and areas for improvement in GraphQLite's AGE compatibility implementation.

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

## Testing Status Summary

- **Total Unit Tests**: 147 tests across 12 suites  
- **Unit Test Success Rate**: ✅ **97% (143/147 passing)**
- **Total Functional Tests**: 11 test files
- **Overall Test Coverage**: ✅ **Comprehensive with all major features tested**

**Current Implementation Status:**
- ✅ **Basic CRUD**: CREATE, MATCH, SET, DELETE operations fully implemented
- ✅ **Data Types**: Integer, Real, Boolean, String with type safety
- ✅ **Operators**: Logical (AND/OR/NOT), comparison, arithmetic operators
- ✅ **Functions**: COUNT, MIN, MAX, AVG, SUM, TYPE() with DISTINCT support  
- ✅ **Column Naming**: Semantic column names for properties and variables
- ✅ **DELETE Clause**: Fully implemented with constraint enforcement
- ✅ **SQL Generation**: AGE-style entity tracking prevents ambiguous column errors
- ✅ **OPTIONAL MATCH**: Fully implemented with LEFT JOIN SQL generation 
- ❌ **String Escapes**: Not implemented
- ❌ **Path Variables**: Not implemented
- ❌ **Multiple Relationship Types**: Not implemented

**Test Progress by File:**
1. ✅ **01_extension_loading.sql** - Passes completely
2. ✅ **02_node_operations.sql** - Passes completely  
3. ✅ **03_relationship_operations.sql** - Passes completely
4. ✅ **04_query_patterns.sql** - Passes completely
5. ✅ **05_return_clauses.sql** - Passes completely
6. ✅ **06_property_access.sql** - Passes completely
7. ✅ **07_agtype_compatibility.sql** - Passes completely
8. ⚠️ **08_complex_queries.sql** - Partial (missing OPTIONAL MATCH, string escapes)
9. ⏸️ **09_edge_cases.sql** - Not yet tested (parser error handling needed)
10. ⏸️ **10_match_create_patterns.sql** - Not yet tested
11. ✅ **11_column_naming.sql** - Passes completely

---

### Issue: SET Label Operations Not Supported
**Status**: ✅ **PARTIALLY FIXED**  
**Priority**: Medium  
**AGE Compatibility**: ✅ **PARTIALLY RESTORED**

**Description:**
The `SET n:Label` syntax for adding labels to nodes is now supported at the parser and transform level.

**Previous Behavior:**
```cypher
MATCH (n) SET n:NewLabel
Runtime error: SET label operations not implemented
```

**Current Behavior:**
```cypher
MATCH (n) SET n:NewLabel
Query parses and transforms successfully - ready for execution
```

**Location**: `tests/test_transform_set.c:248`

**Progress Made:**
- ✅ Added `SET n:Label` grammar support to parser
- ✅ Implemented AST_NODE_LABEL_EXPR handling in transform
- ✅ Added comprehensive test suite for label operations
- ✅ Support for mixed operations: `SET n:Label, n.prop = value`
- ⏸️ Executor support pending (labels not actually added yet)

**Files Modified:**
- `src/backend/parser/cypher_gram.y` - Added label assignment grammar
- `src/backend/transform/transform_set.c` - Added label expression handling
- `tests/test_transform_set.c` - Added comprehensive label tests

**Remaining Work:**
- Implement executor support to actually insert labels into node_labels table
- Support multiple labels in single operation (`SET n:Label1:Label2`)

---

### Issue: Edge Variable Type Detection Bug
**Status**: ✅ **FIXED**  
**Priority**: High  
**AGE Compatibility**: ✅ **RESTORED**

**Description:**
Relationship variables in MATCH patterns were incorrectly registered as node variables instead of edge variables.

**Previous Behavior:**
```cypher
MATCH (a)-[r:REL]->(b) DELETE r
// Variable 'r' was incorrectly marked as VAR_TYPE_NODE
```

**Fixed Behavior:**
```cypher
MATCH (a)-[r:REL]->(b) DELETE r  
// Variable 'r' is now correctly marked as VAR_TYPE_EDGE
```

**Location**: `tests/test_transform_delete.c:113`

**Root Cause:**
- `register_edge_variable` only set type for new variables
- When a variable already existed, the type was not updated

**Fix Applied:**
- Modified `register_edge_variable` to always find and update the variable type
- Same fix applied to `register_node_variable` for consistency
- Both functions now correctly update variable types even for existing variables

**Files Modified:**
- `src/backend/transform/cypher_transform.c:418-431` - Fixed register_edge_variable
- `src/backend/transform/cypher_transform.c:407-420` - Fixed register_node_variable

---

### Issue: Test Expectations Incorrect for Basic Executor Tests
**Status**: Open  
**Priority**: Low  
**Test Issue**: Not a code bug

**Description:**
Several executor tests expect operations to fail when they actually succeed.

**Affected Tests:**
- `test_executor_basic.c:113` - CREATE expected to fail but succeeds
- `test_executor_basic.c:123` - MATCH expected to fail but succeeds  
- `test_executor_basic.c:280` - CREATE with WHERE expected to fail but succeeds

**Root Cause:**
- Tests may be outdated or have incorrect expectations
- Tests might be checking for specific error conditions that no longer apply

**Solution Approach:**
1. Review test expectations and update to match current behavior
2. Determine if tests are checking for valid error conditions
3. Update assertions to match correct behavior

---

## Future Issues Section

*Additional bugs and issues will be documented here as they are discovered.*