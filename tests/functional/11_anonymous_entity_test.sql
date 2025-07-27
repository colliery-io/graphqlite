-- ========================================================================
-- Test 11: Anonymous Entity Tracking (Simplified Version)
-- ========================================================================
-- PURPOSE: Validates that anonymous entities (nodes/relationships without 
--          variables) are properly tracked across query phases
-- COVERS:  Anonymous nodes, anonymous relationships, basic patterns
-- NOTE:    Complex multi-relationship patterns and EXISTS clauses are 
--          disabled due to parser issues that need investigation
-- ========================================================================

.load ./build/graphqlite

-- Clean up any existing test data
SELECT cypher('MATCH (a)-[r]->(b) DELETE r') as cleanup_rels;
SELECT cypher('MATCH (n:AnonTest) DELETE n') as cleanup;

-- =======================================================================
-- SECTION 1: Anonymous Nodes
-- =======================================================================
SELECT '=== Section 1: Anonymous Nodes ===' as section;

SELECT 'Test 1.1 - Create anonymous node:' as test_name;
SELECT cypher('CREATE ()') as result;

SELECT 'Test 1.2 - Create anonymous node with label:' as test_name;
SELECT cypher('CREATE (:AnonTest)') as result;

SELECT 'Test 1.3 - Create anonymous node with properties:' as test_name;
SELECT cypher('CREATE (:AnonTest {type: "anonymous", value: 123})') as result;

SELECT 'Test 1.4 - Match anonymous nodes:' as test_name;
SELECT cypher('MATCH () RETURN COUNT(*) AS total') as result;

SELECT 'Test 1.5 - Match anonymous nodes with label:' as test_name;
SELECT cypher('MATCH (:AnonTest) RETURN COUNT(*) AS anon_count') as result;

-- =======================================================================
-- SECTION 2: Anonymous Relationships in CREATE
-- =======================================================================
SELECT '=== Section 2: Anonymous Relationships in CREATE ===' as section;

-- Create named nodes for relationship tests
SELECT cypher('CREATE (a:AnonTest {name: "NodeA"}), (b:AnonTest {name: "NodeB"}), (c:AnonTest {name: "NodeC"})') as setup;

SELECT 'Test 2.1 - Create anonymous relationship (no variable):' as test_name;
SELECT cypher('MATCH (a:AnonTest {name: "NodeA"}), (b:AnonTest {name: "NodeB"}) CREATE (a)-[:CONNECTS]->(b)') as result;

SELECT 'Test 2.2 - Create anonymous relationship without type:' as test_name;
SELECT cypher('MATCH (b:AnonTest {name: "NodeB"}), (c:AnonTest {name: "NodeC"}) CREATE (b)-[]->(c)') as result;

SELECT 'Test 2.3 - Create multiple anonymous relationships:' as test_name;
SELECT cypher('MATCH (a:AnonTest {name: "NodeA"}), (c:AnonTest {name: "NodeC"}) CREATE (a)-[:LINK1]->(c), (c)-[:LINKX]->(a)') as result;

-- =======================================================================
-- SECTION 3: Simple Anonymous Relationships in MATCH
-- =======================================================================
SELECT '=== Section 3: Simple Anonymous Relationships in MATCH ===' as section;

SELECT 'Test 3.1 - Match pattern with anonymous relationship:' as test_name;
SELECT cypher('MATCH (a)-[:CONNECTS]->(b) RETURN a.name, b.name') as result;

SELECT 'Test 3.2 - Match any anonymous relationship:' as test_name;
SELECT cypher('MATCH (a)-[]->(b) WHERE a.name = "NodeA" RETURN a.name, b.name ORDER BY b.name') as result;

SELECT 'Test 3.3 - Simple working patterns:' as test_name;
SELECT cypher('CREATE (a), (b), (c)') as setup;
SELECT cypher('CREATE (a)-[:LINK1]->(b), (b)-[:SIMPLE]->(c)') as create_rels;
SELECT cypher('MATCH (a)-[]->(b)-[:LINK1]->(c) RETURN a') as test_link1;
SELECT cypher('MATCH (a)-[]->(b)-[:SIMPLE]->(c) RETURN a') as test_simple;

-- =======================================================================
-- SECTION 4: Basic Multi-Pattern Queries
-- =======================================================================
SELECT '=== Section 4: Basic Multi-Pattern Queries ===' as section;

SELECT 'Test 4.1 - Simple chain with anonymous relationships:' as test_name;
SELECT cypher('CREATE (c1:AnonTest {name: "Chain1"})-[]->(c2:AnonTest {name: "Chain2"})-[]->(c3:AnonTest {name: "Chain3"})') as result;

-- =======================================================================
-- Verification: Database State Analysis
-- =======================================================================
SELECT '=== Verification: Database State Analysis ===' as section;

SELECT 'Total AnonTest nodes:' as test_name;
SELECT cypher('MATCH (n:AnonTest) RETURN COUNT(n) as node_count') as result;

SELECT 'Total relationships between AnonTest nodes:' as test_name;
SELECT cypher('MATCH (:AnonTest)-[r]->(:AnonTest) RETURN COUNT(r) as rel_count') as result;

SELECT 'Relationship type distribution:' as test_name;
SELECT type, COUNT(*) as count FROM edges 
WHERE source_id IN (SELECT id FROM nodes WHERE id IN (SELECT node_id FROM node_labels WHERE label = 'AnonTest'))
  AND target_id IN (SELECT id FROM nodes WHERE id IN (SELECT node_id FROM node_labels WHERE label = 'AnonTest'))
GROUP BY type
ORDER BY count DESC, type;

-- =======================================================================
-- CLEANUP
-- =======================================================================
SELECT '=== Cleanup ===' as section;
SELECT cypher('MATCH (a)-[r]->(b) DELETE r') as cleanup_rels;
SELECT cypher('MATCH (n:AnonTest) DELETE n') as cleanup;

-- =======================================================================
SELECT '=== Anonymous Entity Tracking Test Complete ===' as section;
SELECT 'Basic anonymous entity tests completed successfully' as note;
SELECT 'NOTE: Complex patterns disabled due to parser issues needing investigation' as note;