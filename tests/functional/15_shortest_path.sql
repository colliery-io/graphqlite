-- ========================================================================
-- Test 15: Shortest Path Functions
-- ========================================================================
-- PURPOSE: Test shortestPath() and allShortestPaths() functions
-- COVERS:  Shortest path syntax, BFS path finding, path filtering
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 15: Shortest Path Functions ===' as test_section;

-- =======================================================================
-- SETUP: Create a graph with multiple paths between nodes
-- =======================================================================
SELECT '=== Setup: Creating test graph with multiple paths ===' as section;

-- Create a graph like:
--       Alice -> Bob -> Carol
--         |               ^
--         +---> Dave -----+
--
-- Alice to Carol has two paths:
-- - Short path: Alice -> Dave -> Carol (length 2)
-- - Long path: Alice -> Bob -> Carol (length 2) - same length but different route
--
-- And another path through Eve:
--       Alice -> Eve -> Frank -> Carol (length 3)

SELECT cypher('CREATE (a:Person {name: "Alice"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol"})') as setup;
SELECT cypher('CREATE (d:Person {name: "Dave"})') as setup;
SELECT cypher('CREATE (e:Person {name: "Eve"})') as setup;
SELECT cypher('CREATE (f:Person {name: "Frank"})') as setup;

-- Create the relationships
-- Direct path 1: Alice -> Bob -> Carol
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)') as setup;
SELECT cypher('MATCH (b:Person {name: "Bob"}), (c:Person {name: "Carol"}) CREATE (b)-[:KNOWS]->(c)') as setup;

-- Direct path 2: Alice -> Dave -> Carol (another shortest path)
SELECT cypher('MATCH (a:Person {name: "Alice"}), (d:Person {name: "Dave"}) CREATE (a)-[:KNOWS]->(d)') as setup;
SELECT cypher('MATCH (d:Person {name: "Dave"}), (c:Person {name: "Carol"}) CREATE (d)-[:KNOWS]->(c)') as setup;

-- Longer path: Alice -> Eve -> Frank -> Carol
SELECT cypher('MATCH (a:Person {name: "Alice"}), (e:Person {name: "Eve"}) CREATE (a)-[:KNOWS]->(e)') as setup;
SELECT cypher('MATCH (e:Person {name: "Eve"}), (f:Person {name: "Frank"}) CREATE (e)-[:KNOWS]->(f)') as setup;
SELECT cypher('MATCH (f:Person {name: "Frank"}), (c:Person {name: "Carol"}) CREATE (f)-[:KNOWS]->(c)') as setup;

SELECT '=== Setup complete: Graph created ===' as section;

-- =======================================================================
-- Test 1: Basic shortestPath() Syntax Parsing
-- =======================================================================
SELECT '=== Test 1: shortestPath() Syntax Parsing ===' as section;
.echo on

-- Test that shortestPath() syntax is recognized
-- Note: 'end' is a reserved keyword, use 'target' instead
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Should return a path from Alice to Carol' as note;

-- =======================================================================
-- Test 2: shortestPath() Returns Minimum Length Path
-- =======================================================================
SELECT '=== Test 2: shortestPath() Returns Minimum Length ===' as section;
.echo on

-- The shortest path should be length 2 (not the longer path through Eve and Frank)
-- Note: length(p) function may not be implemented yet, so test with node names
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Path should be Alice to Carol (length 2)' as note;

-- =======================================================================
-- Test 3: allShortestPaths() Returns All Minimum Length Paths
-- =======================================================================
SELECT '=== Test 3: allShortestPaths() Returns All Minimum Paths ===' as section;
.echo on

-- allShortestPaths should return both 2-hop paths
SELECT cypher('MATCH p = allShortestPaths((a:Person {name: "Alice"})-[*]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Should return 2 rows (both paths: Alice->Bob->Carol AND Alice->Dave->Carol)' as note;

-- =======================================================================
-- Test 4: shortestPath() with Relationship Type Filter
-- =======================================================================
SELECT '=== Test 4: shortestPath() with Relationship Type ===' as section;
.echo on

-- Test shortest path with specific relationship type
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[:KNOWS*]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Shortest path using only KNOWS relationships' as note;

-- =======================================================================
-- Test 5: shortestPath() with Max Hops
-- =======================================================================
SELECT '=== Test 5: shortestPath() with Max Hops ===' as section;
.echo on

-- Test with maximum hop limit that excludes all paths
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*..1]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: No result (no direct 1-hop path from Alice to Carol)' as note;

-- Test with max hops that includes shortest path
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*..2]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Should return a 2-hop path' as note;

-- =======================================================================
-- Test 6: shortestPath() Between Non-Connected Nodes
-- =======================================================================
SELECT '=== Test 6: shortestPath() Between Non-Connected Nodes ===' as section;

-- Create an isolated node
SELECT cypher('CREATE (z:Person {name: "Zoe"})') as setup;

.echo on

-- Test shortest path to isolated node (should return no results)
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*]->(z:Person {name: "Zoe"})) RETURN a.name, z.name') as result;

.echo off
SELECT 'Expected: No result (no path exists to Zoe)' as note;

-- =======================================================================
-- Test 7: shortestPath() with Path Variable Access
-- =======================================================================
SELECT '=== Test 7: shortestPath() with Path Components ===' as section;
.echo on

-- Access individual elements of the shortest path
-- Note: nodes(p) and relationships(p) functions may not be implemented yet
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Start and end nodes of the shortest path' as note;

-- =======================================================================
-- Test 8: Variable-Length with Minimum Hops
-- =======================================================================
SELECT '=== Test 8: shortestPath() with Min Hops ===' as section;
.echo on

-- Test shortest path with minimum hops constraint
SELECT cypher('MATCH p = shortestPath((a:Person {name: "Alice"})-[*2..]->(target:Person {name: "Carol"})) RETURN a.name, target.name') as result;

.echo off
SELECT 'Expected: Should return a path with at least 2 hops' as note;

-- =======================================================================
-- TEARDOWN: Clean up test data
-- =======================================================================
SELECT '=== Teardown: Cleaning up test data ===' as section;

SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

SELECT '=== Shortest Path Tests Complete ===' as test_section;
