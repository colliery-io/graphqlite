-- ========================================================================
-- Test 17: PageRank Algorithm
-- ========================================================================
-- PURPOSE: Test pageRank() function for graph importance scoring
-- COVERS:  PageRank computation, damping factor, iterations
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 17: PageRank Algorithm ===' as test_section;

-- =======================================================================
-- SETUP: Create a simple web-like graph
-- =======================================================================
SELECT '=== Setup: Creating test graph ===' as section;

-- Classic PageRank example graph:
--   A -> B, A -> C
--   B -> C
--   C -> A
--   D -> C (D is a "dangling" node - only outgoing)
--
-- Expected PageRank order: C > A > B > D
-- C receives links from everyone
-- A receives link from C (high PR)
-- B receives link from A
-- D only gives, never receives

SELECT cypher('CREATE (:Page {name: "A"})') as setup;
SELECT cypher('CREATE (:Page {name: "B"})') as setup;
SELECT cypher('CREATE (:Page {name: "C"})') as setup;
SELECT cypher('CREATE (:Page {name: "D"})') as setup;

-- Create links
SELECT cypher('MATCH (a:Page {name: "A"}), (b:Page {name: "B"}) CREATE (a)-[:LINKS]->(b)') as setup;
SELECT cypher('MATCH (a:Page {name: "A"}), (c:Page {name: "C"}) CREATE (a)-[:LINKS]->(c)') as setup;
SELECT cypher('MATCH (b:Page {name: "B"}), (c:Page {name: "C"}) CREATE (b)-[:LINKS]->(c)') as setup;
SELECT cypher('MATCH (c:Page {name: "C"}), (a:Page {name: "A"}) CREATE (c)-[:LINKS]->(a)') as setup;
SELECT cypher('MATCH (d:Page {name: "D"}), (c:Page {name: "C"}) CREATE (d)-[:LINKS]->(c)') as setup;

SELECT '=== Setup complete ===' as section;

-- =======================================================================
-- Test 1: Basic PageRank Function Call
-- =======================================================================
SELECT '=== Test 1: Basic PageRank() ===' as section;
.echo on

-- PageRank returns (node_id, score) pairs
SELECT cypher('RETURN pageRank()') as result;

.echo off
SELECT 'Expected: Returns PageRank scores for all nodes' as note;

-- =======================================================================
-- Test 2: PageRank with Custom Damping Factor
-- =======================================================================
SELECT '=== Test 2: PageRank with Custom Damping ===' as section;
.echo on

-- Lower damping = more uniform distribution
SELECT cypher('RETURN pageRank(0.5)') as result;

.echo off
SELECT 'Expected: PageRank with damping=0.5 (more uniform)' as note;

-- =======================================================================
-- Test 3: PageRank with Custom Iterations
-- =======================================================================
SELECT '=== Test 3: PageRank with Custom Iterations ===' as section;
.echo on

-- Fewer iterations = less converged
SELECT cypher('RETURN pageRank(0.85, 5)') as result;

.echo off
SELECT 'Expected: PageRank with 5 iterations (less converged)' as note;

-- =======================================================================
-- Test 4: PageRank Score Ranking Verification
-- =======================================================================
SELECT '=== Test 4: Verify PageRank Ranking ===' as section;
.echo on

-- Verify C has highest score (most incoming links)
SELECT cypher('RETURN pageRank(0.85, 20)') as result;

.echo off
SELECT 'Expected: C should have highest PageRank (receives most links)' as note;

-- =======================================================================
-- Test 5: PageRank on Empty Graph
-- =======================================================================
SELECT '=== Test 5: PageRank on Graph After Deletion ===' as section;

-- Clear the graph
SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

.echo on

-- PageRank on empty graph should return empty result
SELECT cypher('RETURN pageRank()') as result;

.echo off
SELECT 'Expected: Empty result for empty graph' as note;

-- =======================================================================
-- Test 6: PageRank on Single Node
-- =======================================================================
SELECT '=== Test 6: PageRank Single Node ===' as section;

SELECT cypher('CREATE (:Page {name: "Lonely"})') as setup;

.echo on

-- Single node with no edges
SELECT cypher('RETURN pageRank()') as result;

.echo off
SELECT 'Expected: Single node gets uniform score (1.0)' as note;

-- =======================================================================
-- Test 7: PageRank on Linear Chain
-- =======================================================================
SELECT '=== Test 7: PageRank Linear Chain ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

-- Create chain: A -> B -> C -> D
SELECT cypher('CREATE (:Node {id: 1})') as setup;
SELECT cypher('CREATE (:Node {id: 2})') as setup;
SELECT cypher('CREATE (:Node {id: 3})') as setup;
SELECT cypher('CREATE (:Node {id: 4})') as setup;
SELECT cypher('MATCH (a:Node {id: 1}), (b:Node {id: 2}) CREATE (a)-[:NEXT]->(b)') as setup;
SELECT cypher('MATCH (b:Node {id: 2}), (c:Node {id: 3}) CREATE (b)-[:NEXT]->(c)') as setup;
SELECT cypher('MATCH (c:Node {id: 3}), (d:Node {id: 4}) CREATE (c)-[:NEXT]->(d)') as setup;

.echo on

SELECT cypher('RETURN pageRank()') as result;

.echo off
SELECT 'Expected: End of chain (node 4) should have lowest PR (no outgoing to pass rank)' as note;

-- =======================================================================
-- Test 8: PageRank on Cyclic Graph
-- =======================================================================
SELECT '=== Test 8: PageRank Cyclic Graph ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

-- Create cycle: A -> B -> C -> A
SELECT cypher('CREATE (:Cycle {id: 1})') as setup;
SELECT cypher('CREATE (:Cycle {id: 2})') as setup;
SELECT cypher('CREATE (:Cycle {id: 3})') as setup;
SELECT cypher('MATCH (a:Cycle {id: 1}), (b:Cycle {id: 2}) CREATE (a)-[:NEXT]->(b)') as setup;
SELECT cypher('MATCH (b:Cycle {id: 2}), (c:Cycle {id: 3}) CREATE (b)-[:NEXT]->(c)') as setup;
SELECT cypher('MATCH (c:Cycle {id: 3}), (a:Cycle {id: 1}) CREATE (c)-[:NEXT]->(a)') as setup;

.echo on

SELECT cypher('RETURN pageRank()') as result;

.echo off
SELECT 'Expected: All nodes should have equal PageRank (symmetric cycle)' as note;

-- =======================================================================
-- Test 9: topPageRank(k) - Get Top K Nodes
-- =======================================================================
SELECT '=== Test 9: topPageRank(k) ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

-- Recreate web graph
SELECT cypher('CREATE (:Page {name: "A"})') as setup;
SELECT cypher('CREATE (:Page {name: "B"})') as setup;
SELECT cypher('CREATE (:Page {name: "C"})') as setup;
SELECT cypher('CREATE (:Page {name: "D"})') as setup;
SELECT cypher('MATCH (a:Page {name: "A"}), (b:Page {name: "B"}) CREATE (a)-[:LINKS]->(b)') as setup;
SELECT cypher('MATCH (a:Page {name: "A"}), (c:Page {name: "C"}) CREATE (a)-[:LINKS]->(c)') as setup;
SELECT cypher('MATCH (b:Page {name: "B"}), (c:Page {name: "C"}) CREATE (b)-[:LINKS]->(c)') as setup;
SELECT cypher('MATCH (c:Page {name: "C"}), (a:Page {name: "A"}) CREATE (c)-[:LINKS]->(a)') as setup;
SELECT cypher('MATCH (d:Page {name: "D"}), (c:Page {name: "C"}) CREATE (d)-[:LINKS]->(c)') as setup;

.echo on

-- Get top 2 nodes by PageRank
SELECT cypher('RETURN topPageRank(2)') as result;

.echo off
SELECT 'Expected: Top 2 nodes (C and A) by PageRank' as note;

-- =======================================================================
-- Test 10: personalizedPageRank - Seed-Biased Ranking
-- =======================================================================
SELECT '=== Test 10: personalizedPageRank() ===' as section;
.echo on

-- Personalized PageRank starting from node 16 (D in current graph)
-- D links to C, so C should rank highly, and C links to A
SELECT cypher('RETURN personalizedPageRank("[16]")') as result;

.echo off
SELECT 'Expected: Scores biased toward D and nodes reachable from D' as note;

-- =======================================================================
-- Test 11: personalizedPageRank with Multiple Seeds
-- =======================================================================
SELECT '=== Test 11: personalizedPageRank Multiple Seeds ===' as section;
.echo on

-- Personalized PageRank from both A (13) and D (16)
SELECT cypher('RETURN personalizedPageRank("[13,16]")') as result;

.echo off
SELECT 'Expected: Scores reflect importance relative to both seeds' as note;

-- =======================================================================
-- TEARDOWN
-- =======================================================================
SELECT '=== Teardown: Cleaning up ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

SELECT '=== PageRank Tests Complete ===' as test_section;
