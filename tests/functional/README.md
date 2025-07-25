# GraphQLite Functional Test Suite

This directory contains comprehensive functional tests for GraphQLite, organized to demonstrate the full breadth of graph database interaction capabilities.

## Test Organization

The functional tests are numbered sequentially and cover all aspects of GraphQLite functionality:

### 01_extension_loading.sql
**Purpose**: Verifies that the GraphQLite extension loads correctly  
**Covers**: Extension loading, schema creation, basic cypher() function  
**Key Tests**: Extension test function, core schema tables, basic CREATE/MATCH operations

### 02_node_operations.sql  
**Purpose**: Comprehensive testing of node creation and property handling  
**Covers**: Node creation patterns, property types, label variations, variable naming  
**Key Tests**: Empty nodes, labeled nodes, property types (string/int/float/bool/null), multiple nodes, complex properties

### 03_relationship_operations.sql
**Purpose**: Complete relationship creation, direction handling, and properties  
**Covers**: Directed/undirected relationships, relationship types, edge properties  
**Key Tests**: Forward/reverse relationships, relationship variables, property preservation, complex patterns

### 04_query_patterns.sql
**Purpose**: Advanced MATCH patterns and WHERE clause filtering  
**Covers**: Node patterns, label matching, property matching, WHERE clauses  
**Key Tests**: Label-based queries, property filtering, complex conditions, RETURN variations

### 05_return_clauses.sql
**Purpose**: RETURN clause modifiers and result formatting  
**Covers**: DISTINCT filtering, ORDER BY sorting, LIMIT/SKIP pagination  
**Key Tests**: DISTINCT with duplicates, multi-column sorting, pagination patterns, combined clauses

### 06_property_access.sql
**Purpose**: Node and edge property access and manipulation  
**Covers**: Property access patterns, type preservation, edge properties  
**Key Tests**: Property type handling, NULL values, edge property creation, complex filtering

### 07_agtype_compatibility.sql
**Purpose**: AGE-compatible output format verification  
**Covers**: ::vertex annotations, ::edge annotations, scalar formatting  
**Key Tests**: Vertex/edge annotations, mixed return types, array formats, special values

### 08_complex_queries.sql
**Purpose**: Advanced graph query patterns and multi-hop relationships  
**Covers**: Multi-hop paths, complex patterns, relationship chains  
**Key Tests**: Two/three-hop traversals, fan-out patterns, complex filtering, graph analysis

### 09_edge_cases.sql
**Purpose**: Boundary conditions, error handling, special values  
**Covers**: Error conditions, special values, boundary tests, malformed queries  
**Key Tests**: Empty strings, extreme numbers, NULL handling, malformed syntax, performance edge cases

### 10_match_create_patterns.sql
**Purpose**: MATCH...CREATE vs standalone CREATE patterns  
**Covers**: Node reuse vs creation, relationship creation with existing nodes  
**Key Tests**: Node reuse verification, error handling for missing nodes, efficiency patterns

## Running Tests

### Run All Functional Tests
```bash
make test-functional
```

### Run Individual Test
```bash
sqlite3 -bail < tests/functional/01_extension_loading.sql
```

### Run Specific Test Category
```bash
# Node operations only
sqlite3 -bail < tests/functional/02_node_operations.sql

# Relationship operations only  
sqlite3 -bail < tests/functional/03_relationship_operations.sql
```

## Test Coverage

The functional test suite provides comprehensive coverage of:

✅ **Core Graph Operations**
- Node creation (all variations: empty, labeled, properties, multiple types)
- Relationship creation (directed, undirected, with properties, different types)  
- Pattern matching (nodes, relationships, complex patterns)
- Property access and filtering
- Mixed node/edge queries

✅ **Advanced Features**
- DISTINCT, ORDER BY, LIMIT, SKIP clauses
- Edge properties with multiple data types
- AGE-compatible output format (::vertex, ::edge annotations)
- MATCH...CREATE patterns (existing vs new nodes)
- Complex multi-hop relationship patterns

✅ **Data Types & Edge Cases**
- All property types (string, int, float, boolean, null)
- Special values (empty strings, negatives, zero, large numbers)
- Error conditions and non-existent data
- Performance and memory edge cases
- Malformed query handling

## Expected Behavior

Each test file is designed to:

1. **Load the extension**: Every test starts with `.load ./build/graphqlite`
2. **Create test data**: Set up appropriate test data for the specific feature area
3. **Execute test cases**: Run comprehensive test scenarios with clear descriptions
4. **Verify results**: Include verification queries to confirm expected behavior
5. **Provide clear output**: Use descriptive test names and section headers

## Test Data Management

- Each test file is **self-contained** and creates its own test data
- Tests can be run **independently** or as a complete suite
- Database state is **not shared** between test files
- Test data is designed to be **realistic** and cover **edge cases**

## Maintenance

When adding new features to GraphQLite:

1. **Identify the appropriate test file** based on the feature category
2. **Add test cases** following the existing pattern and naming convention
3. **Include verification queries** to confirm the feature works correctly
4. **Update this README** if adding new test files or major functionality

## Dependencies

- **SQLite 3**: Required for running the extension
- **GraphQLite extension**: Must be built (`make extension`) before running tests
- **File system access**: Tests read from `./build/graphqlite.dylib` (macOS) or `./build/graphqlite.so` (Linux)

## Troubleshooting

### Extension Not Found
```
Error: near line 4: cannot open shared object file: No such file or directory
```
**Solution**: Run `make extension` to build the GraphQLite extension first

### Permission Denied
```
Error: near line 4: file is encrypted or is not a database
```
**Solution**: Ensure you have read/execute permissions on the extension file

### Test Failures
Individual test failures will show the specific SQL that failed. Check:
1. Extension is properly built and accessible
2. Database schema was created correctly
3. Previous test sections completed successfully

For comprehensive troubleshooting, run tests individually to isolate issues.