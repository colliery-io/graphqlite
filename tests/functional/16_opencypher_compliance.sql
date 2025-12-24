-- Test 16: Core OpenCypher Compliance Features
-- Tests XOR operator, list predicates, reduce(), temporal functions, EXPLAIN

.load ./build/graphqlite

SELECT '=== Test 16: Core OpenCypher Compliance ===' AS test_section;

-- ============================================
-- XOR Operator Tests
-- ============================================

SELECT '=== XOR Operator Tests ===' AS test_section;

-- Test 1.1: XOR true/false combinations
SELECT '--- Test 1.1: XOR true XOR false ---' AS test_section;
SELECT cypher('RETURN true XOR false AS result');
-- Expected: 1 (true)

SELECT '--- Test 1.2: XOR true XOR true ---' AS test_section;
SELECT cypher('RETURN true XOR true AS result');
-- Expected: 0 (false)

SELECT '--- Test 1.3: XOR false XOR false ---' AS test_section;
SELECT cypher('RETURN false XOR false AS result');
-- Expected: 0 (false)

SELECT '--- Test 1.4: XOR false XOR true ---' AS test_section;
SELECT cypher('RETURN false XOR true AS result');
-- Expected: 1 (true)

-- Test 1.5: XOR with expressions
SELECT '--- Test 1.5: XOR with expressions ---' AS test_section;
SELECT cypher('RETURN (1 > 0) XOR (2 < 1) AS result');
-- Expected: 1 (true XOR false = true)

-- ============================================
-- List Predicate Tests: any()
-- ============================================

SELECT '=== any() Predicate Tests ===' AS test_section;

-- Test 2.1: any() returns true
SELECT '--- Test 2.1: any() with match ---' AS test_section;
SELECT cypher('RETURN any(x IN [1, 2, 3] WHERE x > 2) AS result');
-- Expected: 1 (true - 3 > 2)

-- Test 2.2: any() returns false
SELECT '--- Test 2.2: any() no match ---' AS test_section;
SELECT cypher('RETURN any(x IN [1, 2, 3] WHERE x > 5) AS result');
-- Expected: 0 (false)

-- Test 2.3: any() with empty list
SELECT '--- Test 2.3: any() empty list ---' AS test_section;
SELECT cypher('RETURN any(x IN [] WHERE x > 0) AS result');
-- Expected: 0 (false - no elements)

-- ============================================
-- List Predicate Tests: all()
-- ============================================

SELECT '=== all() Predicate Tests ===' AS test_section;

-- Test 3.1: all() returns true
SELECT '--- Test 3.1: all() with all match ---' AS test_section;
SELECT cypher('RETURN all(x IN [1, 2, 3] WHERE x > 0) AS result');
-- Expected: 1 (true - all positive)

-- Test 3.2: all() returns false
SELECT '--- Test 3.2: all() partial match ---' AS test_section;
SELECT cypher('RETURN all(x IN [1, 2, 3] WHERE x > 1) AS result');
-- Expected: 0 (false - 1 is not > 1)

-- Test 3.3: all() with empty list
SELECT '--- Test 3.3: all() empty list ---' AS test_section;
SELECT cypher('RETURN all(x IN [] WHERE x > 0) AS result');
-- Expected: 1 (true - vacuous truth)

-- ============================================
-- List Predicate Tests: none()
-- ============================================

SELECT '=== none() Predicate Tests ===' AS test_section;

-- Test 4.1: none() returns true
SELECT '--- Test 4.1: none() with no match ---' AS test_section;
SELECT cypher('RETURN none(x IN [1, 2, 3] WHERE x > 5) AS result');
-- Expected: 1 (true - none > 5)

-- Test 4.2: none() returns false
SELECT '--- Test 4.2: none() with match ---' AS test_section;
SELECT cypher('RETURN none(x IN [1, 2, 3] WHERE x > 2) AS result');
-- Expected: 0 (false - 3 > 2)

-- Test 4.3: none() with empty list
SELECT '--- Test 4.3: none() empty list ---' AS test_section;
SELECT cypher('RETURN none(x IN [] WHERE x > 0) AS result');
-- Expected: 1 (true - vacuous truth)

-- ============================================
-- List Predicate Tests: single()
-- ============================================

SELECT '=== single() Predicate Tests ===' AS test_section;

-- Test 5.1: single() returns true
SELECT '--- Test 5.1: single() exactly one match ---' AS test_section;
SELECT cypher('RETURN single(x IN [1, 2, 3] WHERE x = 2) AS result');
-- Expected: 1 (true - exactly one element equals 2)

-- Test 5.2: single() returns false (multiple matches)
SELECT '--- Test 5.2: single() multiple matches ---' AS test_section;
SELECT cypher('RETURN single(x IN [1, 2, 3] WHERE x > 1) AS result');
-- Expected: 0 (false - 2 and 3 both > 1)

-- Test 5.3: single() returns false (no matches)
SELECT '--- Test 5.3: single() no matches ---' AS test_section;
SELECT cypher('RETURN single(x IN [1, 2, 3] WHERE x > 5) AS result');
-- Expected: 0 (false - no matches)

-- ============================================
-- reduce() Function Tests
-- ============================================

SELECT '=== reduce() Function Tests ===' AS test_section;

-- Test 6.1: reduce() sum
SELECT '--- Test 6.1: reduce() sum ---' AS test_section;
SELECT cypher('RETURN reduce(acc = 0, x IN [1, 2, 3, 4] | acc + x) AS sum');
-- Expected: 10

-- Test 6.2: reduce() product
SELECT '--- Test 6.2: reduce() product ---' AS test_section;
SELECT cypher('RETURN reduce(acc = 1, x IN [1, 2, 3, 4] | acc * x) AS product');
-- Expected: 24

-- Test 6.3: reduce() with empty list
SELECT '--- Test 6.3: reduce() empty list ---' AS test_section;
SELECT cypher('RETURN reduce(acc = 100, x IN [] | acc + x) AS result');
-- Expected: 100 (initial value)

-- Test 6.4: reduce() with subtraction
SELECT '--- Test 6.4: reduce() subtraction ---' AS test_section;
SELECT cypher('RETURN reduce(acc = 100, x IN [10, 20, 30] | acc - x) AS result');
-- Expected: 40 (100 - 10 - 20 - 30)

-- ============================================
-- Temporal Function Tests
-- ============================================

SELECT '=== Temporal Function Tests ===' AS test_section;

-- Test 7.1: date() returns current date
SELECT '--- Test 7.1: date() current ---' AS test_section;
SELECT cypher('RETURN date() AS today');
-- Expected: current date in YYYY-MM-DD format

-- Test 7.2: time() returns current time
SELECT '--- Test 7.2: time() current ---' AS test_section;
SELECT cypher('RETURN time() AS now');
-- Expected: current time in HH:MM:SS format

-- Test 7.3: datetime() returns current datetime
SELECT '--- Test 7.3: datetime() current ---' AS test_section;
SELECT cypher('RETURN datetime() AS now');
-- Expected: current datetime in YYYY-MM-DD HH:MM:SS format

-- Test 7.4: date() with string parsing
SELECT '--- Test 7.4: date() with string ---' AS test_section;
SELECT cypher('RETURN date("2024-06-15") AS parsed');
-- Expected: 2024-06-15

-- ============================================
-- EXPLAIN Tests
-- ============================================

SELECT '=== EXPLAIN Tests ===' AS test_section;

-- Test 8.1: EXPLAIN simple query
SELECT '--- Test 8.1: EXPLAIN MATCH ---' AS test_section;
SELECT cypher('EXPLAIN MATCH (n) RETURN n');
-- Expected: SQL query string (not executed)

-- Test 8.2: EXPLAIN with WHERE
SELECT '--- Test 8.2: EXPLAIN with WHERE ---' AS test_section;
SELECT cypher('EXPLAIN MATCH (n:Person) WHERE n.age > 21 RETURN n.name');
-- Expected: SQL query string with WHERE clause

-- Test 8.3: EXPLAIN CREATE (should return INSERT SQL)
SELECT '--- Test 8.3: EXPLAIN CREATE ---' AS test_section;
SELECT cypher('EXPLAIN CREATE (n:Test {name: "test"})');
-- Expected: SQL INSERT statement

-- ============================================
-- Combined Feature Tests
-- ============================================

SELECT '=== Combined Feature Tests ===' AS test_section;

-- Test 9.1: XOR with list predicates
SELECT '--- Test 9.1: XOR with any() ---' AS test_section;
SELECT cypher('RETURN any(x IN [1,2,3] WHERE x > 2) XOR any(x IN [1,2,3] WHERE x < 0) AS result');
-- Expected: 1 (true XOR false = true)

-- Test 9.2: reduce() with multiplication
SELECT '--- Test 9.2: Complex reduce ---' AS test_section;
SELECT cypher('RETURN reduce(acc = 0, x IN [1, 2, 3] | acc + x * x) AS sum_of_squares');
-- Expected: 14 (1 + 4 + 9)

SELECT '=== Core OpenCypher Compliance Tests Complete ===' AS test_section;
