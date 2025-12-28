-- ========================================================================
-- Test 28: Variable-Length Relationship Patterns
-- ========================================================================
-- PURPOSE: Test variable-length path patterns with *min..max syntax
-- COVERS:  Exact length, bounded range, unbounded, with relationship types
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 28: Variable-Length Paths ===' as test_section;

-- =======================================================================
-- SETUP: Create a chain/tree graph
-- =======================================================================
SELECT '=== Setup: Creating chain and tree structure ===' as section;

-- Linear chain: A -> B -> C -> D -> E
SELECT cypher('CREATE (a:Node {name: "A", level: 0})') as setup;
SELECT cypher('CREATE (b:Node {name: "B", level: 1})') as setup;
SELECT cypher('CREATE (c:Node {name: "C", level: 2})') as setup;
SELECT cypher('CREATE (d:Node {name: "D", level: 3})') as setup;
SELECT cypher('CREATE (e:Node {name: "E", level: 4})') as setup;

SELECT cypher('MATCH (a:Node {name: "A"}), (b:Node {name: "B"}) CREATE (a)-[:NEXT]->(b)') as setup;
SELECT cypher('MATCH (b:Node {name: "B"}), (c:Node {name: "C"}) CREATE (b)-[:NEXT]->(c)') as setup;
SELECT cypher('MATCH (c:Node {name: "C"}), (d:Node {name: "D"}) CREATE (c)-[:NEXT]->(d)') as setup;
SELECT cypher('MATCH (d:Node {name: "D"}), (e:Node {name: "E"}) CREATE (d)-[:NEXT]->(e)') as setup;

-- Tree structure: Root -> Child1, Child2; Child1 -> Grandchild
SELECT cypher('CREATE (root:Tree {name: "Root"})') as setup;
SELECT cypher('CREATE (c1:Tree {name: "Child1"})') as setup;
SELECT cypher('CREATE (c2:Tree {name: "Child2"})') as setup;
SELECT cypher('CREATE (gc:Tree {name: "Grandchild"})') as setup;

SELECT cypher('MATCH (r:Tree {name: "Root"}), (c:Tree {name: "Child1"}) CREATE (r)-[:PARENT_OF]->(c)') as setup;
SELECT cypher('MATCH (r:Tree {name: "Root"}), (c:Tree {name: "Child2"}) CREATE (r)-[:PARENT_OF]->(c)') as setup;
SELECT cypher('MATCH (c:Tree {name: "Child1"}), (gc:Tree {name: "Grandchild"}) CREATE (c)-[:PARENT_OF]->(gc)') as setup;

-- =======================================================================
-- SECTION 1: Exact length paths
-- =======================================================================
SELECT '=== Section 1: Exact length ===' as section;

SELECT 'Test 1.1 - Exactly 1 hop:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*1]->(target)
    RETURN target.name AS reached
') as result;

SELECT 'Test 1.2 - Exactly 2 hops:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*2]->(target)
    RETURN target.name AS reached
') as result;

SELECT 'Test 1.3 - Exactly 3 hops:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*3]->(target)
    RETURN target.name AS reached
') as result;

SELECT 'Test 1.4 - Exactly 4 hops:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*4]->(target)
    RETURN target.name AS reached
') as result;

-- =======================================================================
-- SECTION 2: Bounded range
-- =======================================================================
SELECT '=== Section 2: Bounded range *min..max ===' as section;

SELECT 'Test 2.1 - One to two hops:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*1..2]->(target)
    RETURN target.name AS reached ORDER BY target.name
') as result;

SELECT 'Test 2.2 - One to three hops:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*1..3]->(target)
    RETURN target.name AS reached ORDER BY target.name
') as result;

SELECT 'Test 2.3 - Two to four hops:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*2..4]->(target)
    RETURN target.name AS reached ORDER BY target.name
') as result;

SELECT 'Test 2.4 - Zero to one hops (includes self):' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*0..1]->(target)
    RETURN target.name AS reached ORDER BY target.name
') as result;

-- =======================================================================
-- SECTION 3: With relationship type
-- =======================================================================
SELECT '=== Section 3: Typed variable-length ===' as section;

SELECT 'Test 3.1 - NEXT type only:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[:NEXT*1..3]->(target)
    RETURN target.name ORDER BY target.name
') as result;

SELECT 'Test 3.2 - PARENT_OF in tree:' as test_name;
SELECT cypher('
    MATCH (r:Tree {name: "Root"})-[:PARENT_OF*1..2]->(descendant)
    RETURN descendant.name ORDER BY descendant.name
') as result;

SELECT 'Test 3.3 - All descendants:' as test_name;
SELECT cypher('
    MATCH (r:Tree {name: "Root"})-[:PARENT_OF*]->(descendant)
    RETURN descendant.name ORDER BY descendant.name
') as result;

-- =======================================================================
-- SECTION 4: Direction variations
-- =======================================================================
SELECT '=== Section 4: Direction ===' as section;

SELECT 'Test 4.1 - Reverse direction:' as test_name;
SELECT cypher('
    MATCH (e:Node {name: "E"})<-[*1..2]-(source)
    RETURN source.name ORDER BY source.name
') as result;

SELECT 'Test 4.2 - Any direction:' as test_name;
SELECT cypher('
    MATCH (c:Node {name: "C"})-[*1]-(neighbor)
    RETURN neighbor.name ORDER BY neighbor.name
') as result;

-- =======================================================================
-- SECTION 5: With path variable
-- =======================================================================
SELECT '=== Section 5: Path capture ===' as section;

SELECT 'Test 5.1 - Capture path:' as test_name;
SELECT cypher('
    MATCH path = (a:Node {name: "A"})-[*2]->(target)
    RETURN target.name, length(path) AS path_length
') as result;

SELECT 'Test 5.2 - Multiple paths of different lengths:' as test_name;
SELECT cypher('
    MATCH path = (a:Node {name: "A"})-[*1..3]->(target)
    RETURN target.name, length(path) AS hops ORDER BY hops, target.name
') as result;

-- =======================================================================
-- SECTION 6: Edge cases
-- =======================================================================
SELECT '=== Section 6: Edge cases ===' as section;

SELECT 'Test 6.1 - Path too long (no result):' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*10]->(target)
    RETURN target.name
') as result;

SELECT 'Test 6.2 - Min equals max:' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*2..2]->(target)
    RETURN target.name
') as result;

SELECT 'Test 6.3 - Zero length (self):' as test_name;
SELECT cypher('
    MATCH (a:Node {name: "A"})-[*0]->(target)
    RETURN target.name
') as result;

SELECT 'Test 6.4 - From middle of chain:' as test_name;
SELECT cypher('
    MATCH (c:Node {name: "C"})-[*1..2]->(target)
    RETURN target.name ORDER BY target.name
') as result;

SELECT '=== Test 28 Complete ===' as test_section;
