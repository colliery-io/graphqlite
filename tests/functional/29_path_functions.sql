-- ========================================================================
-- Test 29: Path Functions
-- ========================================================================
-- PURPOSE: Test path-related functions: length(), nodes(), relationships()
-- COVERS:  Path capture, extracting components
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 29: Path Functions ===' as test_section;

-- =======================================================================
-- SETUP: Create connected graph
-- =======================================================================
SELECT '=== Setup: Creating connected graph ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", age: 35})') as setup;
SELECT cypher('CREATE (d:Person {name: "David", age: 28})') as setup;

SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS {since: 2020}]->(b)
') as setup;
SELECT cypher('
    MATCH (b:Person {name: "Bob"}), (c:Person {name: "Carol"})
    CREATE (b)-[:KNOWS {since: 2021}]->(c)
') as setup;
SELECT cypher('
    MATCH (c:Person {name: "Carol"}), (d:Person {name: "David"})
    CREATE (c)-[:KNOWS {since: 2022}]->(d)
') as setup;

-- =======================================================================
-- SECTION 1: length() function
-- =======================================================================
SELECT '=== Section 1: length() function ===' as section;

SELECT 'Test 1.1 - Length of single-hop path:' as test_name;
SELECT cypher('
    MATCH path = (a:Person {name: "Alice"})-[:KNOWS]->(b)
    RETURN a.name AS from_person, b.name AS to_person, length(path) AS hops
') as result;

SELECT 'Test 1.2 - Length of two-hop path:' as test_name;
SELECT cypher('
    MATCH path = (a:Person {name: "Alice"})-[:KNOWS*2]->(c)
    RETURN a.name AS from_person, c.name AS to_person, length(path) AS hops
') as result;

SELECT 'Test 1.3 - Variable path lengths:' as test_name;
SELECT cypher('
    MATCH path = (a:Person {name: "Alice"})-[:KNOWS*1..3]->(target)
    RETURN target.name, length(path) AS distance
    ORDER BY distance, target.name
') as result;

-- =======================================================================
-- SECTION 2: nodes() function (single hop)
-- =======================================================================
SELECT '=== Section 2: nodes() function ===' as section;

SELECT 'Test 2.1 - Nodes in single-hop path:' as test_name;
SELECT cypher('
    MATCH path = (a:Person {name: "Alice"})-[:KNOWS]->(b)
    RETURN nodes(path) AS path_nodes
') as result;

-- =======================================================================
-- SECTION 3: relationships() function (single hop)
-- =======================================================================
SELECT '=== Section 3: relationships() function ===' as section;

SELECT 'Test 3.1 - Relationships in single-hop path:' as test_name;
SELECT cypher('
    MATCH path = (a:Person {name: "Alice"})-[:KNOWS]->(b)
    RETURN relationships(path) AS path_rels
') as result;

-- =======================================================================
-- SECTION 4: Path patterns without capture
-- =======================================================================
SELECT '=== Section 4: Pattern matching ===' as section;

SELECT 'Test 4.1 - Find all reachable nodes:' as test_name;
SELECT cypher('
    MATCH (a:Person {name: "Alice"})-[:KNOWS*]->(reachable)
    RETURN DISTINCT reachable.name ORDER BY reachable.name
') as result;

SELECT 'Test 4.2 - Filter by path length:' as test_name;
SELECT cypher('
    MATCH path = (a:Person {name: "Alice"})-[:KNOWS*]->(target)
    WHERE length(path) >= 2
    RETURN target.name, length(path) AS distance
    ORDER BY distance
') as result;

-- =======================================================================
-- SECTION 5: Edge cases
-- =======================================================================
SELECT '=== Section 5: Edge cases ===' as section;

SELECT 'Test 5.1 - Path from disconnected start:' as test_name;
SELECT cypher('CREATE (lonely:Person {name: "Lonely"})') as setup;
SELECT cypher('
    MATCH path = (l:Person {name: "Lonely"})-[:KNOWS]->(anyone)
    RETURN length(path)
') as result;

SELECT '=== Test 29 Complete ===' as test_section;
