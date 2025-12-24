-- Performance tests for Core OpenCypher Compliance features
-- Run with: sqlite3 :memory: < tests/performance/perf_opencypher_compliance.sql

.load ./build/graphqlite
.timer on

SELECT '=== Performance: Core OpenCypher Compliance ===' AS test;

-- Setup test data
SELECT '--- Setup: Creating test graph ---' AS test;
SELECT cypher('CREATE (n:Perf {id: 1, value: 100})');
SELECT cypher('CREATE (n:Perf {id: 2, value: 200})');
SELECT cypher('CREATE (n:Perf {id: 3, value: 300})');
SELECT cypher('CREATE (n:Perf {id: 4, value: 400})');
SELECT cypher('CREATE (n:Perf {id: 5, value: 500})');

-- ============================================
-- XOR Operator Performance
-- ============================================
SELECT '=== XOR Performance (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN true XOR false') FROM cnt;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN (1 > 0) XOR (2 < 1) XOR (3 > 2)') FROM cnt;

-- ============================================
-- List Predicate Performance
-- ============================================
SELECT '=== any() Performance (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN any(x IN [1,2,3,4,5,6,7,8,9,10] WHERE x > 5)') FROM cnt;

SELECT '=== all() Performance (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN all(x IN [1,2,3,4,5,6,7,8,9,10] WHERE x > 0)') FROM cnt;

SELECT '=== none() Performance (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN none(x IN [1,2,3,4,5,6,7,8,9,10] WHERE x > 100)') FROM cnt;

SELECT '=== single() Performance (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN single(x IN [1,2,3,4,5,6,7,8,9,10] WHERE x = 5)') FROM cnt;

-- ============================================
-- reduce() Performance
-- ============================================
SELECT '=== reduce() Performance - Small List (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN reduce(acc = 0, x IN [1,2,3,4,5] | acc + x)') FROM cnt;

SELECT '=== reduce() Performance - Medium List (100 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN reduce(acc = 0, x IN [1,2,3,4,5,6,7,8,9,10] | acc + x)') FROM cnt;

SELECT '=== reduce() Performance - Large List (50 iterations) ===' AS test;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 50)
SELECT cypher('RETURN reduce(acc = 0, x IN [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20] | acc + x)') FROM cnt;

-- ============================================
-- Temporal Functions Performance
-- ============================================
SELECT '=== Temporal Functions Performance (100 iterations each) ===' AS test;

SELECT '--- date() ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN date()') FROM cnt;

SELECT '--- time() ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN time()') FROM cnt;

SELECT '--- datetime() ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN datetime()') FROM cnt;

-- ============================================
-- EXPLAIN Performance
-- ============================================
SELECT '=== EXPLAIN Performance (100 iterations) ===' AS test;

SELECT '--- EXPLAIN simple ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('EXPLAIN MATCH (n) RETURN n') FROM cnt;

SELECT '--- EXPLAIN complex ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('EXPLAIN MATCH (n:Perf) WHERE n.value > 200 RETURN n.id, n.value') FROM cnt;

-- ============================================
-- Combined Operations Performance
-- ============================================
SELECT '=== Combined Operations (50 iterations) ===' AS test;

SELECT '--- List predicate + XOR ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 50)
SELECT cypher('RETURN any(x IN [1,2,3] WHERE x > 2) XOR all(x IN [1,2,3] WHERE x > 0)') FROM cnt;

SELECT '--- reduce() + comparison ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 50)
SELECT cypher('RETURN reduce(a = 0, x IN [1,2,3,4,5] | a + x) > 10') FROM cnt;

-- ============================================
-- Scaling Tests
-- ============================================
SELECT '=== Scaling: List Size Impact ===' AS test;

SELECT '--- any() with 5 elements ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN any(x IN [1,2,3,4,5] WHERE x > 3)') FROM cnt;

SELECT '--- any() with 20 elements ---' AS test;
WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 100)
SELECT cypher('RETURN any(x IN [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20] WHERE x > 15)') FROM cnt;

SELECT '=== Performance Tests Complete ===' AS test;

-- Cleanup
SELECT cypher('MATCH (n:Perf) DELETE n');
