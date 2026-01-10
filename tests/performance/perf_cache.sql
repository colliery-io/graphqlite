-- Performance tests for CSR Graph Caching
-- Run with: sqlite3 :memory: < tests/performance/perf_cache.sql
--
-- Tests the ~28x speedup from graph caching for algorithm execution

.load ./build/graphqlite
.mode column
.headers on

SELECT '=== Performance: Graph Caching ===' AS test;
SELECT '';

-- ============================================
-- Setup: Build test graph (1000 nodes, 5000 edges)
-- ============================================
SELECT '--- Setup: Creating test graph (1000 nodes, ~5000 edges) ---' AS phase;
.timer off

-- Suppress output during graph creation
.output /dev/null

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 1000)
SELECT cypher('CREATE (:Node {id: "n' || x || '"})') FROM cnt;

WITH RECURSIVE cnt(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM cnt WHERE x < 5000)
SELECT cypher('MATCH (a:Node {id: "n' || ((x % 1000) + 1) || '"}), (b:Node {id: "n' || (((x * 7) % 1000) + 1) || '"}) CREATE (a)-[:EDGE]->(b)') FROM cnt;

.output stdout

SELECT 'Graph created: 1000 nodes, ~5000 edges' AS status;
SELECT '';

-- ============================================
-- Test 1: UNCACHED PageRank Performance
-- ============================================
SELECT '=== UNCACHED PageRank (3 runs) ===' AS test;
.timer on

SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;
SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;
SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;

.timer off
SELECT '';

-- ============================================
-- Test 2: Load Graph Cache
-- ============================================
SELECT '=== Loading graph into cache ===' AS test;
.timer on
SELECT gql_load_graph() AS cache_result;
.timer off
SELECT '';

-- ============================================
-- Test 3: CACHED PageRank Performance
-- ============================================
SELECT '=== CACHED PageRank (3 runs) ===' AS test;
.timer on

SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;
SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;
SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;

.timer off
SELECT '';

-- ============================================
-- Test 4: Multiple Algorithms with Cache
-- ============================================
SELECT '=== CACHED Multiple Algorithms ===' AS test;
.timer on

SELECT 'PageRank' AS algo, length(cypher('RETURN pageRank()')) AS json_len;
SELECT 'LabelProp' AS algo, length(cypher('RETURN labelPropagation()')) AS json_len;
SELECT 'DegreeCent' AS algo, length(cypher('RETURN degreeCentrality()')) AS json_len;
SELECT 'Louvain' AS algo, length(cypher('RETURN louvain()')) AS json_len;
SELECT 'WCC' AS algo, length(cypher('RETURN connectedComponents()')) AS json_len;

.timer off
SELECT '';

-- ============================================
-- Test 5: Cache Status Functions
-- ============================================
SELECT '=== Cache Status Functions ===' AS test;
.timer on

SELECT gql_graph_loaded() AS is_loaded;
SELECT gql_reload_graph() AS reload_result;
SELECT gql_unload_graph() AS unload_result;
SELECT gql_graph_loaded() AS is_loaded_after_unload;

.timer off
SELECT '';

-- ============================================
-- Test 6: Post-Unload Performance (back to uncached)
-- ============================================
SELECT '=== UNCACHED PageRank after unload (verify) ===' AS test;
.timer on

SELECT length(cypher('RETURN topPageRank(5)')) AS json_len;

.timer off

SELECT '';
SELECT '=== Cache Performance Tests Complete ===' AS status;
