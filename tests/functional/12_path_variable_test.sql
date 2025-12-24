-- ========================================================================
-- Test 12: Path Variable Assignment
-- ========================================================================
-- PURPOSE: Test path variable assignment syntax (path = (a)-[r]->(b))
-- COVERS:  Path variable parsing, path data types, path value construction
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 12: Path Variable Assignment ===' as test_section;

-- =======================================================================
-- SETUP: Create test data for path variable tests
-- =======================================================================
SELECT '=== Setup: Creating test data for path variables ===' as section;

-- Create nodes for path testing
SELECT cypher('CREATE (a:Person {name: "Alice"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol"})') as setup;

-- Create relationships
SELECT cypher('MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"}) CREATE (a)-[:KNOWS]->(b)') as setup;
SELECT cypher('MATCH (b:Person {name: "Bob"}), (c:Person {name: "Carol"}) CREATE (b)-[:LIKES]->(c)') as setup;

-- =======================================================================
-- Test 1: Basic Path Variable Assignment
-- =======================================================================
SELECT '=== Test 1: Basic Path Variable Assignment ===' as section;
.echo on

-- Test simple path variable assignment
SELECT cypher('MATCH path = (a:Person)-[r:KNOWS]->(b:Person) RETURN path') as result;

.echo off
SELECT 'Expected: Path variable should contain vertex-edge-vertex sequence' as note;

-- =======================================================================
-- Test 2: Path Variable with Node Variables
-- =======================================================================
SELECT '=== Test 2: Path Variable with Node Variables ===' as section;
.echo on

-- Test path variable with individual node access
SELECT cypher('MATCH path = (a:Person)-[r:KNOWS]->(b:Person) RETURN path, a.name, b.name') as result;

.echo off
SELECT 'Expected: Path variable plus individual node properties' as note;

-- =======================================================================
-- Test 3: Multiple Path Variables
-- =======================================================================
SELECT '=== Test 3: Multiple Path Variables ===' as section;
.echo on

-- Test multiple path patterns with variables
SELECT cypher('MATCH path1 = (a:Person)-[r1:KNOWS]->(b:Person), path2 = (b)-[r2:LIKES]->(c:Person) RETURN path1, path2') as result;

.echo off
SELECT 'Expected: Two separate path variables' as note;

-- =======================================================================
-- Test 4: Path Variable in WHERE Clause
-- =======================================================================
SELECT '=== Test 4: Path Variable in WHERE Clause ===' as section;
.echo on

-- Test path variable used in WHERE filtering
SELECT cypher('MATCH path = (a:Person)-[r]->(b:Person) WHERE a.name = "Alice" RETURN path') as result;

.echo off
SELECT 'Expected: Filtered path starting from Alice' as note;

-- =======================================================================
-- Test 5: Longer Path Variables  
-- =======================================================================
SELECT '=== Test 5: Longer Path Variables ===' as section;
.echo on

-- Test path with multiple hops
SELECT cypher('MATCH path = (a:Person)-[r1:KNOWS]->(b:Person)-[r2:LIKES]->(c:Person) RETURN path') as result;

.echo off
SELECT 'Expected: Three-node path with two relationships' as note;

-- =======================================================================
-- Test 6: Path Variable Error Cases
-- =======================================================================
SELECT '=== Test 6: Path Variable Error Cases ===' as section;
.echo on

-- Test duplicate path variable name (should fail or warning)
SELECT cypher('MATCH path = (a:Person)-[]->(b:Person), path = (c:Person)-[]->(d:Person) RETURN path') as result;

.echo off
SELECT 'Expected: Error or warning for duplicate path variable' as note;

-- =======================================================================
-- Test 7: Path Variable with Properties
-- =======================================================================
SELECT '=== Test 7: Path Variable with Properties ===' as section;
.echo on

-- Test path variable with property constraints
SELECT cypher('MATCH path = (a:Person {name: "Alice"})-[r:KNOWS]->(b:Person) RETURN path') as result;

.echo off
SELECT 'Expected: Path with property-constrained nodes' as note;

-- =======================================================================
-- TEARDOWN: Clean up test data
-- =======================================================================
SELECT '=== Teardown: Cleaning up test data ===' as section;

-- Delete all test data
SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup;

SELECT '=== Path Variable Tests Complete ===' as test_section;