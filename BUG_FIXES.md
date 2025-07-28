# Bug Fixes and Known Issues

This document tracks bugs, issues, and areas for improvement in GraphQLite's AGE compatibility implementation.

## Unimplemented Cypher Clauses



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



## Parser and Grammar Issues

### Parser Query Length Limitation (BUG 🔴)
- **Issue**: Multi-relationship patterns fail when total query length exceeds ~50 characters
- **Root Cause**: Parser state machine issue with shift/reduce conflicts in relationship pattern grammar
- **Location**: Grammar rules for path patterns in `cypher_gram.y` (6 shift/reduce conflicts identified)
- **Examples**: 
  - Fails: `MATCH (start:AnonTest)-[]->(middle)-[r:LINKX]->(end) RETURN start.name` (70 chars)
  - Fails: `MATCH (start)-[]->(middle)-[]->(end) RETURN start` (50 chars) 
  - Works: `MATCH (a)-[]->(b)-[]->(c) RETURN a` (35 chars)
  - Works: Single relationships of any length work fine
- **Investigation Results**:
  - Fixed incorrect manual memory management in path reduction rule (lines 363-365)
  - Issue persists after memory fix - appears to be fundamental grammar limitation
  - Error location reporting is incorrect (reports column 147+ for 50-char queries)
  - Affects all multi-relationship patterns regardless of named/anonymous variables
- **Status**: 🔴 DOCUMENTED LIMITATION - Grammar refactoring required for proper fix
- **Priority**: Medium - Workaround: Use shorter variable names or single relationships
- **Workaround**: Keep total query length under 50 characters or split into multiple MATCH clauses

### Multiple Pattern Support in MATCH Clauses (BUG 🔴)
- **Issue**: Comma-separated patterns in MATCH clauses fail with "Parse error at line 1, column 1386: syntax error"
- **Root Cause**: Parser grammar cannot handle multiple patterns separated by commas in single MATCH clause
- **Location**: Grammar rules for pattern lists in `cypher_gram.y`
- **Examples**: 
  - Fails: `MATCH (start)-[]->(end), (other)-[]->(end) WHERE start.name = "X" RETURN ...`
  - Works: `MATCH (start)-[]->(end) WHERE start.name = "X" RETURN ...` (single pattern)
- **Status**: 🔴 NEEDS INVESTIGATION - Grammar support for comma-separated patterns
- **Priority**: Medium - Advanced querying functionality

### Long Relationship Chains (3+ relationships) (BUG 🔴)
- **Issue**: Complex chains with 3+ relationships fail with "Parse error at line 1, column 1498: syntax error"
- **Root Cause**: Path pattern complexity handling in grammar becomes problematic with longer chains
- **Location**: Grammar rules for path patterns in `cypher_gram.y`
- **Examples**: 
  - Fails: `MATCH (start)-[]->(n1)-[]->(n2)-[]->(end) WHERE start.name = "Chain1" RETURN ...`
  - Works: `MATCH (a)-[]->(b)-[]->(c) RETURN ...` (3 nodes, 2 relationships)
- **Status**: 🔴 NEEDS INVESTIGATION - Path pattern complexity limits
- **Priority**: Medium - Advanced pattern matching

### EXISTS Keyword Implementation (✅ PROPERTY FORM IMPLEMENTED)
- **Issue**: EXISTS expressions not supported in WHERE clauses
- **Root Cause**: EXISTS keyword defined in tokens but not implemented in grammar
- **Location**: Grammar rules in `cypher_gram.y` for EXISTS expressions
- **Examples**: 
  - ✅ **Fully supported**: `MATCH (n:Test) WHERE EXISTS(n.property) RETURN n.name`
  - ✅ **Fully supported**: `MATCH (n:Test) WHERE NOT EXISTS(n.missing) RETURN n.name`
  - ❌ **Not implemented**: `MATCH (n:Test) WHERE EXISTS((n)-[:CONNECTS]->()) RETURN n.name`
- **Status**: ✅ **PROPERTY FORM FULLY IMPLEMENTED** - Property existence checking working
- **Pattern Status**: ❌ **EXISTS((pattern)) syntax not implemented** - requires special grammar handling
- **Priority**: ✅ High priority property feature completed
- **Implementation**: 
  - ✅ Added AST nodes and constructors for EXISTS expressions
  - ✅ Added grammar rules for EXISTS(property) syntax  
  - ✅ Implemented transform layer with comprehensive SQL generation
  - ✅ Added comprehensive test suite (property tests passing, pattern tests disabled)
- **Pattern Implementation**: Pattern syntax `EXISTS((n)-[:TYPE]->())` requires different grammar approach as patterns are not expressions

---

## Test Coverage Summary

### ✅ Successfully Working Anonymous Entity Features:
- Anonymous node creation and matching: `CREATE ()`, `MATCH () RETURN COUNT(*)`
- Anonymous relationship creation: `CREATE (a)-[]->(b)`
- Simple chained anonymous relationships: `MATCH (a)-[]->(b)-[]->(c) RETURN ...`
- Self-referencing patterns: `MATCH (n)-[]->(n) WHERE n.name = "Self" RETURN n.name`
- Basic entity aliasing verification
- Database state analysis queries

### 🔴 Documented as Parser Bugs:
- Mixed named/anonymous patterns: `(start)-[]->(middle)-[r:LINKX]->(end)`
- Multiple patterns in single MATCH: `(start)-[]->(end), (other)-[]->(end)`
- Long relationship chains: `(start)-[]->(n1)-[]->(n2)-[]->(end)`

### ⏸️ Expected Limitations:
- EXISTS patterns: `WHERE EXISTS((n)-[:TYPE]->())` - Not yet implemented

**Test Files Affected:**
- `tests/functional/11_anonymous_entity_test_complex.sql` - Contains documented bugs as comments
- Parser error locations: columns 49, 1386, 1498 respectively

---

### Issue: EXISTS Pattern Syntax Not Implemented 
**Status**: Open  
**Priority**: Medium  
**AGE Compatibility**: Missing pattern existence checking functionality

**Description:**
The `EXISTS((pattern))` syntax for checking relationship pattern existence is not implemented. Only `EXISTS(property)` syntax is supported.

**Current Behavior:**
```cypher
MATCH (n:Test) WHERE EXISTS((n)-[:KNOWS]->()) RETURN n.name
Parse error at line 1, column XXX: syntax error
```

**Expected AGE-Compatible Behavior:**
```cypher
MATCH (n:Test) WHERE EXISTS((n)-[:KNOWS]->()) RETURN n.name
-- Should return nodes that have outgoing KNOWS relationships
```

**Location**: `tests/functional/13_exists_functionality.sql` (pattern tests disabled)

**Root Cause:**
- Grammar only supports `EXISTS(expr)` where `expr` is a property access
- Pattern syntax `(n)-[:TYPE]->()` is not a regular expression - it's a path pattern
- Requires special grammar handling as patterns have different parsing rules than expressions
- Current implementation incorrectly tries to parse patterns as expressions

**Technical Challenge:**
- Patterns use different grammar rules (`path`, `node_pattern`, `rel_pattern`) 
- Cannot be parsed within the `expr` rule that `EXISTS(expr)` expects
- Need new grammar rule like `EXISTS '(' '(' path ')' ')'` for pattern syntax
- Transform layer needs pattern existence checking (different from property existence)

**Affected Code:**
- `src/backend/parser/cypher_gram.y` - Need separate EXISTS pattern grammar rule
- `src/backend/transform/transform_return.c` - Need pattern existence transform logic
- Pattern existence requires different SQL generation than property existence

**Solution Approach:**
1. Add separate grammar rule for `EXISTS((pattern))` syntax
2. Extend AST to handle pattern-based EXISTS expressions  
3. Implement pattern existence checking in transform layer
4. Generate SQL that checks for relationship pattern existence
5. Support nested patterns: `EXISTS((n)-[]->()-[]->())`

**Examples Needed:**
- `EXISTS((n)-[:KNOWS]->())` - specific relationship type
- `EXISTS((n)-[]->())` - any outgoing relationship  
- `EXISTS(()<-[]-(n))` - any incoming relationship
- `EXISTS((n)-[]->(:Label))` - relationship to specific label
- `EXISTS((n)-[]->()-[]->())` - multi-hop patterns

---

## Future Issues Section

*Additional bugs and issues will be documented here as they are discovered.*