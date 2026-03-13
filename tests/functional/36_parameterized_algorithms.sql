-- ========================================================================
-- Test 36: Parameterized Graph Algorithms
-- ========================================================================
-- PURPOSE: Test parameter binding ($param) in graph algorithm functions
-- COVERS:  bfs, dfs, dijkstra, astar, nodeSimilarity, knn with params
-- BUG:     GitHub Issue #27 - segfault on parameterized traversals
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 36: Parameterized Graph Algorithms ===' as test_section;

-- =======================================================================
-- SETUP: Create a graph for traversal and algorithm testing
-- =======================================================================
SELECT '=== Setup: Creating test graph ===' as section;

-- Create nodes with user_id properties for algorithm lookups
SELECT cypher('CREATE (a:City {id: "A", name: "Alpha", lat: 0.0, lon: 0.0})') as setup;
SELECT cypher('CREATE (b:City {id: "B", name: "Bravo", lat: 1.0, lon: 0.0})') as setup;
SELECT cypher('CREATE (c:City {id: "C", name: "Charlie", lat: 1.0, lon: 1.0})') as setup;
SELECT cypher('CREATE (d:City {id: "D", name: "Delta", lat: 0.0, lon: 1.0})') as setup;

-- Create edges with weights: A->B(1), A->C(3), B->C(1), C->D(1)
SELECT cypher('MATCH (a:City {id: "A"}), (b:City {id: "B"}) CREATE (a)-[:ROAD {weight: 1}]->(b)') as setup;
SELECT cypher('MATCH (a:City {id: "A"}), (c:City {id: "C"}) CREATE (a)-[:ROAD {weight: 3}]->(c)') as setup;
SELECT cypher('MATCH (b:City {id: "B"}), (c:City {id: "C"}) CREATE (b)-[:ROAD {weight: 1}]->(c)') as setup;
SELECT cypher('MATCH (c:City {id: "C"}), (d:City {id: "D"}) CREATE (c)-[:ROAD {weight: 1}]->(d)') as setup;

SELECT '=== Setup complete ===' as section;

-- =======================================================================
-- SECTION 1: BFS with parameters (GitHub Issue #27 - segfault)
-- =======================================================================
SELECT '=== Section 1: BFS with parameters ===' as section;

SELECT 'Test 1.1 - BFS with literal (baseline):' as test_name;
SELECT cypher('RETURN bfs("A")') as result;

SELECT 'Test 1.2 - BFS with string parameter:' as test_name;
SELECT cypher('RETURN bfs($start)', '{"start": "A"}') as result;

SELECT 'Test 1.3 - BFS with parameter and max depth:' as test_name;
SELECT cypher('RETURN bfs($start, 1)', '{"start": "A"}') as result;

SELECT 'Test 1.4 - BFS parameter with different start node:' as test_name;
SELECT cypher('RETURN bfs($node)', '{"node": "C"}') as result;

SELECT 'Test 1.5 - BFS with both parameters:' as test_name;
SELECT cypher('RETURN bfs($start, $maxDepth)', '{"start": "A", "maxDepth": 1}') as result;

-- =======================================================================
-- SECTION 2: DFS with parameters
-- =======================================================================
SELECT '=== Section 2: DFS with parameters ===' as section;

SELECT 'Test 2.1 - DFS with literal (baseline):' as test_name;
SELECT cypher('RETURN dfs("A")') as result;

SELECT 'Test 2.2 - DFS with string parameter:' as test_name;
SELECT cypher('RETURN dfs($start)', '{"start": "A"}') as result;

SELECT 'Test 2.3 - DFS with parameter and max depth:' as test_name;
SELECT cypher('RETURN dfs($start, 1)', '{"start": "A"}') as result;

SELECT 'Test 2.4 - DFS parameter with different start node:' as test_name;
SELECT cypher('RETURN dfs($node)', '{"node": "C"}') as result;

SELECT 'Test 2.5 - DFS with both parameters:' as test_name;
SELECT cypher('RETURN dfs($start, $maxDepth)', '{"start": "A", "maxDepth": 1}') as result;

-- =======================================================================
-- SECTION 3: Dijkstra with parameters
-- =======================================================================
SELECT '=== Section 3: Dijkstra with parameters ===' as section;

SELECT 'Test 3.1 - Dijkstra with literals (baseline):' as test_name;
SELECT cypher('RETURN dijkstra("A", "D")') as result;

SELECT 'Test 3.2 - Dijkstra with source parameter:' as test_name;
SELECT cypher('RETURN dijkstra($src, "D")', '{"src": "A"}') as result;

SELECT 'Test 3.3 - Dijkstra with target parameter:' as test_name;
SELECT cypher('RETURN dijkstra("A", $dst)', '{"dst": "D"}') as result;

SELECT 'Test 3.4 - Dijkstra with both parameters:' as test_name;
SELECT cypher('RETURN dijkstra($src, $dst)', '{"src": "A", "dst": "D"}') as result;

-- =======================================================================
-- SECTION 4: A* with parameters
-- =======================================================================
SELECT '=== Section 4: A* with parameters ===' as section;

SELECT 'Test 4.1 - A* with literals (baseline):' as test_name;
SELECT cypher('RETURN astar("A", "D")') as result;

SELECT 'Test 4.2 - A* with source parameter:' as test_name;
SELECT cypher('RETURN astar($src, "D")', '{"src": "A"}') as result;

SELECT 'Test 4.3 - A* with both parameters:' as test_name;
SELECT cypher('RETURN astar($src, $dst)', '{"src": "A", "dst": "D"}') as result;

-- =======================================================================
-- SECTION 5: Node Similarity with parameters
-- =======================================================================
SELECT '=== Section 5: Node Similarity with parameters ===' as section;

SELECT 'Test 5.1 - nodeSimilarity with literal source and baseline (baseline):' as test_name;
SELECT cypher('RETURN nodeSimilarity("A", "B")') as result;

SELECT 'Test 5.2 - nodeSimilarity with source parameter:' as test_name;
SELECT cypher('RETURN nodeSimilarity($n1, "B")', '{"n1": "A"}') as result;

SELECT 'Test 5.3 - nodeSimilarity with source and target parameters:' as test_name;
SELECT cypher('RETURN nodeSimilarity($n1, $n2)', '{"n1": "A", "n2": "B"}') as result;

SELECT 'Test 5.4 - nodeSimilarity with literal threshold and top_k (baseline):' as test_name;
SELECT cypher('RETURN nodeSimilarity(0.5, 3)') as result;

SELECT 'Test 5.5 - nodeSimilarity with threshold and top_k parameters:' as test_name;
SELECT cypher('RETURN nodeSimilarity($threshold, $topK)', '{"threshold": 0.5, "topK": 3}') as result;

-- =======================================================================
-- SECTION 6: KNN with parameters
-- =======================================================================
SELECT '=== Section 6: KNN with parameters ===' as section;

SELECT 'Test 6.1 - KNN with literal (baseline):' as test_name;
SELECT cypher('RETURN knn("A", 3)') as result;

SELECT 'Test 6.2 - KNN with node parameter:' as test_name;
SELECT cypher('RETURN knn($node, 3)', '{"node": "A"}') as result;

SELECT 'Test 6.2 - KNN with both parameters:' as test_name;
SELECT cypher('RETURN knn($node, $k)', '{"node": "A", "k": 3}') as result;

-- =======================================================================
-- SECTION 7: pageRank with parameters
-- =======================================================================
SELECT '=== Section 7: pageRank with parameters ===' as section;

SELECT 'Test 7.1 - pageRank with literal (baseline):' as test_name;
SELECT cypher('RETURN pageRank(0.5, 101)') as result;

SELECT 'Test 7.2 - pageRank with node parameters:' as test_name;
SELECT cypher('RETURN pageRank($damping, $iterations)', '{"damping": 0.5, "iterations": 100}') as result;

-- =======================================================================
-- SECTION 8: topPageRank with parameters
-- =======================================================================
SELECT '=== Section 8: topPageRank with parameters ===' as section;

SELECT 'Test 8.1 - topPageRank with literal (baseline):' as test_name;
SELECT cypher('RETURN topPageRank(3, 0.5, 101)') as result;

SELECT 'Test 8.2 - topPageRank with node parameters:' as test_name;
SELECT cypher('RETURN topPageRank($top, $damping, $iterations)', '{"top": 3, "damping": 0.5, "iterations": 100}') as result;

-- =======================================================================
-- SECTION 9: labelPropagation with parameters
-- =======================================================================
SELECT '=== Section 9: labelPropagation with parameters ===' as section;

SELECT 'Test 9.1 - labelPropagation with literal (baseline):' as test_name;
SELECT cypher('RETURN labelPropagation(10)') as result;

SELECT 'Test 9.2 - labelPropagation with node parameter:' as test_name;
SELECT cypher('RETURN labelPropagation($iterations)', '{"iterations": 101}') as result;

-- =======================================================================
-- SECTION 10: louvain with parameters
-- =======================================================================
SELECT '=== Section 10: louvain with parameters ===' as section;

SELECT 'Test 10.1 - louvain with literal (baseline):' as test_name;
SELECT cypher('RETURN louvain(-0.5)') as result;

SELECT 'Test 10.2 - louvain with node parameter:' as test_name;
SELECT cypher('RETURN louvain($resolution)', '{"resolution": -0.5}') as result;

-- =======================================================================
-- SECTION 11: eigenvector with parameters
-- =======================================================================
SELECT '=== Section 11: eigenvectorCentrality with parameters ===' as section;

SELECT 'Test 11.1 - eigenvectorCentrality with literal (baseline):' as test_name;
SELECT cypher('RETURN eigenvectorCentrality(100)') as result;

SELECT 'Test 11.2 - eigenvectorCentrality with node parameter:' as test_name;
SELECT cypher('RETURN eigenvectorCentrality($iterations)', '{"iterations": 100}') as result;

-- =======================================================================
-- SECTION 12: Edge cases
-- =======================================================================
SELECT '=== Section 12: Edge cases ===' as section;

SELECT 'Test 12.1 - BFS param with nonexistent node:' as test_name;
SELECT cypher('RETURN bfs($start)', '{"start": "NONEXISTENT"}') as result;

SELECT 'Test 12.2 - Dijkstra param with nonexistent source:' as test_name;
SELECT cypher('RETURN dijkstra($src, "D")', '{"src": "NONEXISTENT"}') as result;

-- =======================================================================
-- TEARDOWN
-- =======================================================================
SELECT '=== Teardown: Cleaning up ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

SELECT '=== Test 36 Complete ===' as test_section;
