-- Test 14: List Comprehension and Map Functionality
-- Tests list comprehension syntax: [x IN list WHERE cond | transform]
-- Tests map literal syntax: {key: value, ...}
-- Tests map projection syntax: n{.prop1, .prop2}

.load ./build/graphqlite

SELECT '=== Test 14: List Comprehensions and Maps ===' AS test_section;

-- Test 1: Basic list comprehension (identity)
SELECT '--- Test 1: Basic List Comprehension ---' AS test_section;

SELECT cypher('RETURN [x IN [1, 2, 3]]') AS result;
-- Expected: [1, 2, 3]

-- Test 2: List comprehension with transform
SELECT '--- Test 2: List Comprehension with Transform ---' AS test_section;

SELECT cypher('RETURN [x IN [1, 2, 3] | x * 2]') AS result;
-- Expected: [2, 4, 6]

-- Test 3: List comprehension with WHERE filter
SELECT '--- Test 3: List Comprehension with Filter ---' AS test_section;

SELECT cypher('RETURN [x IN [1, 2, 3, 4, 5] WHERE x > 2]') AS result;
-- Expected: [3, 4, 5]

-- Test 4: List comprehension with WHERE and transform
SELECT '--- Test 4: List Comprehension with Filter and Transform ---' AS test_section;

SELECT cypher('RETURN [x IN [1, 2, 3, 4, 5] WHERE x > 2 | x * 10]') AS result;
-- Expected: [30, 40, 50]

-- Test 5: Simple expression return
SELECT '--- Test 5: Simple Expression Return ---' AS test_section;

SELECT cypher('RETURN 1 + 2') AS result;
-- Expected: 3

-- Test 6: String return
SELECT '--- Test 6: String Return ---' AS test_section;

SELECT cypher('RETURN "hello"') AS result;
-- Expected: "hello"

-- ============================================
-- Map Literal Tests
-- ============================================

SELECT '=== Map Literal Tests ===' AS test_section;

-- Test 7: Empty map literal
SELECT '--- Test 7: Empty Map Literal ---' AS test_section;

SELECT cypher('RETURN {}') AS result;
-- Expected: {}

-- Test 8: Simple map literal
SELECT '--- Test 8: Simple Map Literal ---' AS test_section;

SELECT cypher('RETURN {name: "Alice", age: 30}') AS result;
-- Expected: {"name":"Alice","age":30}

-- Test 9: Map literal with expressions
SELECT '--- Test 9: Map Literal with Expressions ---' AS test_section;

SELECT cypher('RETURN {sum: 1 + 2, product: 3 * 4}') AS result;
-- Expected: {"sum":3,"product":12}

-- ============================================
-- Map Projection Tests
-- ============================================

SELECT '=== Map Projection Tests ===' AS test_section;

-- Setup test data for map projections
SELECT cypher('CREATE (p:ProjectionTest {name: "Bob", age: 25, city: "NYC"})');

-- Test 10: Basic map projection
SELECT '--- Test 10: Basic Map Projection ---' AS test_section;

SELECT cypher('MATCH (n:ProjectionTest) RETURN n{.name, .age}') AS result;
-- Expected: {"name":"Bob","age":"25"}

-- Test 11: Map projection with all properties
SELECT '--- Test 11: Map Projection Single Property ---' AS test_section;

SELECT cypher('MATCH (n:ProjectionTest) RETURN n{.city}') AS result;
-- Expected: {"city":"NYC"}

-- Cleanup test data
SELECT cypher('MATCH (n:ProjectionTest) DELETE n');

SELECT '=== List Comprehension and Map Tests Complete ===' AS test_section;
