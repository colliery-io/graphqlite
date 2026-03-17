-- ========================================================================
-- Test 100: Bare Undirected Match Syntax (--, -->, <--)
-- ========================================================================
-- PURPOSE: Test bare relationship patterns without brackets
-- COVERS:  --, -->, <-- syntax and undirected match both directions
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 100: Bare Undirected Match Syntax ===' as test_section;

-- Setup: create a small graph
SELECT cypher('CREATE (a:Person {id: "alice", name: "Alice"})') as setup;
SELECT cypher('CREATE (b:Person {id: "bob", name: "Bob"})') as setup;
SELECT cypher('CREATE (c:Person {id: "carol", name: "Carol"})') as setup;
SELECT cypher('MATCH (a {id: "alice"}), (b {id: "bob"}) CREATE (a)-[:KNOWS]->(b)') as setup;
SELECT cypher('MATCH (c {id: "carol"}), (a {id: "alice"}) CREATE (c)-[:FOLLOWS]->(a)') as setup;

-- =======================================================================
-- SECTION 1: Bare -- (undirected, both directions)
-- =======================================================================
SELECT '=== Section 1: Bare undirected -- ===' as section;

SELECT 'Test 1.1 - Bare -- returns both directions:' as test_name;
SELECT cypher('MATCH (a {id: "alice"})--(b) RETURN b.id AS neighbor ORDER BY b.id') as result;
-- Expect: bob (outgoing KNOWS) and carol (incoming FOLLOWS)

-- =======================================================================
-- SECTION 2: Bare --> (outgoing only)
-- =======================================================================
SELECT '=== Section 2: Bare outgoing --> ===' as section;

SELECT 'Test 2.1 - Bare --> returns only outgoing:' as test_name;
SELECT cypher('MATCH (a {id: "alice"})-->(b) RETURN b.id AS neighbor') as result;
-- Expect: bob only

-- =======================================================================
-- SECTION 3: Bare <-- (incoming only)
-- =======================================================================
SELECT '=== Section 3: Bare incoming <-- ===' as section;

SELECT 'Test 3.1 - Bare <-- returns only incoming:' as test_name;
SELECT cypher('MATCH (a {id: "alice"})<--(b) RETURN b.id AS neighbor') as result;
-- Expect: carol only

-- =======================================================================
-- SECTION 4: Bracket undirected -[]- (both directions)
-- =======================================================================
SELECT '=== Section 4: Bracket undirected ===' as section;

SELECT 'Test 4.1 - Bracket -[]- returns both directions:' as test_name;
SELECT cypher('MATCH (a {id: "alice"})-[]-(b) RETURN b.id AS neighbor ORDER BY b.id') as result;
-- Expect: bob and carol

-- =======================================================================
-- SECTION 5: Typed undirected -[:TYPE]- (both directions)
-- =======================================================================
SELECT '=== Section 5: Typed undirected ===' as section;

SELECT 'Test 5.1 - Typed -[:KNOWS]- returns both directions for KNOWS:' as test_name;
SELECT cypher('MATCH (a {id: "alice"})-[:KNOWS]-(b) RETURN b.id AS neighbor') as result;
-- Expect: bob only (only KNOWS edges, both directions)

SELECT 'Test 5.2 - Typed -[:FOLLOWS]- returns both directions for FOLLOWS:' as test_name;
SELECT cypher('MATCH (a {id: "alice"})-[:FOLLOWS]-(b) RETURN b.id AS neighbor') as result;
-- Expect: carol only (only FOLLOWS edges, both directions)

-- =======================================================================
-- SECTION 6: Minus expression still works
-- =======================================================================
SELECT '=== Section 6: Minus expression regression ===' as section;

SELECT 'Test 6.1 - Subtraction still works:' as test_name;
SELECT cypher('RETURN 10-3 AS result') as result;
-- Expect: 7

SELECT 'Test 6.2 - Negative numbers work:' as test_name;
SELECT cypher('RETURN -5 AS result') as result;
-- Expect: -5

SELECT '=== Test 100 Complete ===' as test_section;
