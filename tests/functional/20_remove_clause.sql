-- Test REMOVE clause functionality
-- This tests property and label removal operations

.load ./build/graphqlite

-- ============================================
-- Test 1: REMOVE node property
-- ============================================
.print "=== Test 1: REMOVE node property ==="

-- Create test node with multiple properties
SELECT cypher('CREATE (n:RemoveTest1 {name: "Alice", age: 30, city: "NYC"})');

.print "Before REMOVE:"
SELECT cypher('MATCH (n:RemoveTest1) RETURN n.name, n.age, n.city');

-- Remove the age property
SELECT cypher('MATCH (n:RemoveTest1) REMOVE n.age');

.print ""
.print "After REMOVE n.age:"
SELECT cypher('MATCH (n:RemoveTest1) RETURN n.name, n.age, n.city');

-- ============================================
-- Test 2: REMOVE multiple properties
-- ============================================
.print ""
.print "=== Test 2: REMOVE multiple properties ==="

SELECT cypher('CREATE (n:RemoveTest2 {a: 1, b: 2, c: 3, d: 4})');

.print "Before REMOVE:"
SELECT cypher('MATCH (n:RemoveTest2) RETURN n.a, n.b, n.c, n.d');

-- Remove multiple properties in one query
SELECT cypher('MATCH (n:RemoveTest2) REMOVE n.a, n.b');

.print ""
.print "After REMOVE n.a, n.b:"
SELECT cypher('MATCH (n:RemoveTest2) RETURN n.a, n.b, n.c, n.d');

-- ============================================
-- Test 3: REMOVE label
-- ============================================
.print ""
.print "=== Test 3: REMOVE label ==="

SELECT cypher('CREATE (n:Person:Employee:Manager {name: "Bob"})');

.print "Before REMOVE label:"
SELECT cypher('MATCH (n:Person {name: "Bob"}) RETURN n.name, labels(n)');

-- Remove the Manager label
SELECT cypher('MATCH (n:Person {name: "Bob"}) REMOVE n:Manager');

.print ""
.print "After REMOVE n:Manager:"
SELECT cypher('MATCH (n:Person {name: "Bob"}) RETURN n.name, labels(n)');

-- Remove the Employee label too
SELECT cypher('MATCH (n:Person {name: "Bob"}) REMOVE n:Employee');

.print ""
.print "After REMOVE n:Employee:"
SELECT cypher('MATCH (n:Person {name: "Bob"}) RETURN n.name, labels(n)');

-- ============================================
-- Test 4: REMOVE edge property
-- ============================================
.print ""
.print "=== Test 4: REMOVE edge property ==="

SELECT cypher('CREATE (a:RemoveEdgeTest {name: "A"})-[r:KNOWS {since: 2020, strength: 0.9}]->(b:RemoveEdgeTest {name: "B"})');

.print "Before REMOVE edge property:"
SELECT cypher('MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) RETURN a.name, r.since, r.strength, b.name');

-- Remove edge property
SELECT cypher('MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) REMOVE r.since');

.print ""
.print "After REMOVE r.since:"
SELECT cypher('MATCH (a:RemoveEdgeTest)-[r:KNOWS]->(b:RemoveEdgeTest) RETURN a.name, r.since, r.strength, b.name');

-- ============================================
-- Test 5: REMOVE with WHERE clause
-- ============================================
.print ""
.print "=== Test 5: REMOVE with WHERE clause ==="

SELECT cypher('CREATE (a:RemoveWhereTest {name: "Alice", age: 30, status: "active"})');
SELECT cypher('CREATE (b:RemoveWhereTest {name: "Bob", age: 25, status: "active"})');
SELECT cypher('CREATE (c:RemoveWhereTest {name: "Charlie", age: 35, status: "active"})');

.print "Before REMOVE with WHERE:"
SELECT cypher('MATCH (n:RemoveWhereTest) RETURN n.name, n.age, n.status ORDER BY n.name');

-- Remove status only from nodes where age > 28
SELECT cypher('MATCH (n:RemoveWhereTest) WHERE n.age > 28 REMOVE n.status');

.print ""
.print "After REMOVE n.status WHERE n.age > 28:"
SELECT cypher('MATCH (n:RemoveWhereTest) RETURN n.name, n.age, n.status ORDER BY n.name');

-- ============================================
-- Test 6: REMOVE non-existent property (should not error)
-- ============================================
.print ""
.print "=== Test 6: REMOVE non-existent property ==="

SELECT cypher('CREATE (n:RemoveNonExist {name: "Test"})');

-- This should succeed without error
SELECT cypher('MATCH (n:RemoveNonExist) REMOVE n.nonexistent_property');

.print "REMOVE non-existent property completed without error"

-- ============================================
-- Test 7: Combined property and label REMOVE
-- ============================================
.print ""
.print "=== Test 7: Combined property and label REMOVE ==="

SELECT cypher('CREATE (n:TestNode:TempLabel {prop1: "value1", prop2: "value2"})');

.print "Before combined REMOVE:"
SELECT cypher('MATCH (n:TestNode) RETURN n.prop1, n.prop2, labels(n)');

-- Remove both property and label
SELECT cypher('MATCH (n:TestNode) REMOVE n.prop1, n:TempLabel');

.print ""
.print "After REMOVE n.prop1, n:TempLabel:"
SELECT cypher('MATCH (n:TestNode) RETURN n.prop1, n.prop2, labels(n)');

.print ""
.print "REMOVE clause tests completed successfully!"
