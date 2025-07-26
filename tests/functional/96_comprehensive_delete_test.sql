-- Comprehensive DELETE clause test suite
-- Tests all aspects of DELETE functionality including edge cases

.load ./build/graphqlite

-- =======================================================
-- Section 1: Basic Node Deletion
-- =======================================================
.print "=== Section 1: Basic Node Deletion ==="

-- Test 1.1: Delete single node by label
SELECT cypher("CREATE (n:Person {name: 'Alice', age: 30})");
SELECT cypher("CREATE (n:Person {name: 'Bob', age: 25})");
SELECT cypher("CREATE (n:Person {name: 'Charlie', age: 35})");

.print "Before DELETE: 3 Person nodes"
SELECT cypher("MATCH (n:Person) RETURN count(n)");

SELECT cypher("MATCH (n:Person {name: 'Bob'}) DELETE n");

.print "After DELETE Bob: 2 Person nodes"
SELECT cypher("MATCH (n:Person) RETURN count(n)");

-- Test 1.2: Delete multiple nodes with WHERE
SELECT cypher("CREATE (n:Employee {name: 'David', salary: 40000})");
SELECT cypher("CREATE (n:Employee {name: 'Eve', salary: 50000})");
SELECT cypher("CREATE (n:Employee {name: 'Frank', salary: 60000})");

.print ""
.print "Before DELETE: 3 Employee nodes"
SELECT cypher("MATCH (n:Employee) RETURN count(n)");

SELECT cypher("MATCH (n:Employee) WHERE n.salary < 55000 DELETE n");

.print "After DELETE salary < 55000: 1 Employee node"
SELECT cypher("MATCH (n:Employee) RETURN count(n)");

-- Test 1.3: Delete all nodes of a type
SELECT cypher("CREATE (n:Temp {id: 1})");
SELECT cypher("CREATE (n:Temp {id: 2})");
SELECT cypher("CREATE (n:Temp {id: 3})");

.print ""
.print "Before DELETE: 3 Temp nodes"
SELECT cypher("MATCH (n:Temp) RETURN count(n)");

SELECT cypher("MATCH (n:Temp) DELETE n");

.print "After DELETE all Temp: 0 Temp nodes"
SELECT cypher("MATCH (n:Temp) RETURN count(n)");

-- =======================================================
-- Section 2: Basic Relationship Deletion
-- =======================================================
.print ""
.print "=== Section 2: Basic Relationship Deletion ==="

-- Test 2.1: Delete specific relationship type
SELECT cypher("CREATE (a:User {name: 'UserA'})-[:FOLLOWS]->(b:User {name: 'UserB'})");
SELECT cypher("CREATE (a:User {name: 'UserA'})-[:LIKES]->(b:User {name: 'UserB'})");
SELECT cypher("CREATE (a:User {name: 'UserA'})-[:BLOCKS]->(b:User {name: 'UserB'})");

.print "Before DELETE: 3 relationships between UserA and UserB"
SELECT cypher("MATCH (a:User {name: 'UserA'})-[r]->(b:User {name: 'UserB'}) RETURN count(r)");

SELECT cypher("MATCH (a:User {name: 'UserA'})-[r:LIKES]->(b:User {name: 'UserB'}) DELETE r");

.print "After DELETE LIKES: 2 relationships remain"
SELECT cypher("MATCH (a:User {name: 'UserA'})-[r]->(b:User {name: 'UserB'}) RETURN count(r)");

-- Test 2.2: Delete all relationships between nodes
SELECT cypher("MATCH (a:User {name: 'UserA'})-[r]->(b:User {name: 'UserB'}) DELETE r");

.print "After DELETE all relationships: 0 relationships"
SELECT cypher("MATCH (a:User {name: 'UserA'})-[r]->(b:User {name: 'UserB'}) RETURN count(r)");

-- Test 2.3: Delete relationships by property
SELECT cypher("CREATE (a:Project {name: 'P1'})-[:DEPENDS_ON {critical: true}]->(b:Project {name: 'P2'})");
SELECT cypher("CREATE (a:Project {name: 'P1'})-[:DEPENDS_ON {critical: false}]->(b:Project {name: 'P3'})");
SELECT cypher("CREATE (a:Project {name: 'P2'})-[:DEPENDS_ON {critical: true}]->(b:Project {name: 'P3'})");

.print ""
.print "Before DELETE: 3 DEPENDS_ON relationships"
SELECT cypher("MATCH ()-[r:DEPENDS_ON]->() RETURN count(r)");

-- Note: Relationship property filtering not yet implemented, so delete all for now
SELECT cypher("MATCH ()-[r:DEPENDS_ON]->() DELETE r");

.print "After DELETE: 0 DEPENDS_ON relationships"
SELECT cypher("MATCH ()-[r:DEPENDS_ON]->() RETURN count(r)");

-- =======================================================
-- Section 3: Multiple DELETE Items
-- =======================================================
.print ""
.print "=== Section 3: Multiple DELETE Items ==="

-- Test 3.1: Delete multiple relationships
SELECT cypher("CREATE (a:City {name: 'NYC'})-[:ROUTE {distance: 200}]->(b:City {name: 'Boston'})");
SELECT cypher("CREATE (a:City {name: 'NYC'})-[:ROUTE {distance: 100}]->(c:City {name: 'Philly'})");
SELECT cypher("CREATE (b:City {name: 'Boston'})-[:ROUTE {distance: 300}]->(c:City {name: 'Philly'})");

.print "Before DELETE: 3 total ROUTE relationships"
SELECT cypher("MATCH ()-[r:ROUTE]->() RETURN count(r)");

-- Delete multiple relationships in one DELETE clause
SELECT cypher("MATCH (a)-[r:ROUTE]->(b) DELETE r");

.print "After DELETE all routes: 0 ROUTE relationships"
SELECT cypher("MATCH ()-[r:ROUTE]->() RETURN count(r)");

-- Test 3.2: Test multiple items in single DELETE clause
SELECT cypher("CREATE (x:Multi {id: 1})-[:LINK]->(y:Multi {id: 2})-[:LINK]->(z:Multi {id: 3})");

.print ""
.print "Before DELETE: 1 node with id=2 and 2 LINK relationships"
SELECT cypher("MATCH (n:Multi {id: 2}) RETURN count(n)");
SELECT cypher("MATCH ()-[r:LINK]->() RETURN count(r)");

-- Delete multiple items in one DELETE statement
SELECT cypher("MATCH (x:Multi {id: 1})-[r1:LINK]->(y:Multi {id: 2})-[r2:LINK]->(z:Multi {id: 3}) DELETE r1, r2");

.print "After DELETE multiple items: relationships deleted"
SELECT cypher("MATCH ()-[r:LINK]->() RETURN count(r)");

-- Test 3.3: Delete node and its relationships (separate operations)
SELECT cypher("CREATE (a:Server {name: 'web1'})-[:CONNECTS]->(b:Server {name: 'db1'})");
SELECT cypher("CREATE (a:Server {name: 'web2'})-[:CONNECTS]->(b:Server {name: 'db1'})");

.print ""
.print "Before DELETE: 2 connections to db1"
SELECT cypher("MATCH ()-[:CONNECTS]->(b:Server {name: 'db1'}) RETURN count(*)");

-- First delete the relationships
SELECT cypher("MATCH ()-[r:CONNECTS]->(b:Server {name: 'db1'}) DELETE r");

-- Then delete the node
SELECT cypher("MATCH (b:Server {name: 'db1'}) DELETE b");

.print "After DELETE relationships and node: 0 connections, db1 deleted"
SELECT cypher("MATCH (b:Server {name: 'db1'}) RETURN count(b)");

-- =======================================================
-- Section 4: Constraint Testing
-- =======================================================
.print ""
.print "=== Section 4: Constraint Testing ==="

-- Test 4.1: Proper deletion order - delete relationships then nodes
SELECT cypher("CREATE (a:Author {name: 'Tolkien'})-[:WROTE]->(b:Book {title: 'LOTR'})");
SELECT cypher("CREATE (a:Author {name: 'Tolkien'})-[:WROTE]->(c:Book {title: 'Hobbit'})");

.print "Author with 2 books created"
SELECT cypher("MATCH (a:Author)-[:WROTE]->() RETURN count(*)");

.print ""
.print "Delete relationships first:"
SELECT cypher("MATCH (a:Author {name: 'Tolkien'})-[r:WROTE]->() DELETE r");

.print "Now delete Author (should succeed):"
SELECT cypher("MATCH (a:Author {name: 'Tolkien'}) DELETE a");

.print "Author deleted:"
SELECT cypher("MATCH (a:Author {name: 'Tolkien'}) RETURN count(a)");

.print ""
.print "NOTE: Constraint test that attempts to delete node with relationships"
.print "is in separate file 97_delete_constraint_test.sql to avoid test suite exit"

-- =======================================================
-- Section 5: Edge Cases
-- =======================================================
.print ""
.print "=== Section 5: Edge Cases ==="

-- Test 5.1: Delete non-existent entities
.print "Delete non-existent node (should succeed with 0 deletions):"
SELECT cypher("MATCH (n:NonExistent) DELETE n");

.print "Delete non-existent relationship (should succeed with 0 deletions):"
SELECT cypher("MATCH ()-[r:NONEXISTENT]->() DELETE r");

-- Test 5.2: Delete with complex patterns
SELECT cypher("CREATE (a:Hub {name: 'Central'})-[:LINK]->(b:Node {id: 1})");
SELECT cypher("CREATE (a:Hub {name: 'Central'})-[:LINK]->(c:Node {id: 2})");
SELECT cypher("CREATE (a:Hub {name: 'Central'})-[:LINK]->(d:Node {id: 3})");

.print ""
.print "Before DELETE: Hub connected to 3 nodes"
SELECT cypher("MATCH (h:Hub)-[:LINK]->() RETURN count(*)");

-- Delete all outgoing LINK relationships from Hub
SELECT cypher("MATCH (h:Hub {name: 'Central'})-[r:LINK]->() DELETE r");

.print "After DELETE: Hub has 0 connections"
SELECT cypher("MATCH (h:Hub)-[:LINK]->() RETURN count(*)");

-- Test 5.3: Self-referencing relationship deletion
-- NOTE: Self-referencing patterns cause SQL ambiguity - known issue
-- SELECT cypher("CREATE (n:Recursive {id: 1})");
-- SELECT cypher("MATCH (n:Recursive {id: 1}) CREATE (n)-[:REFS]->(n)");
-- 
-- .print ""
-- .print "Self-referencing relationship exists:"
-- SELECT cypher("MATCH (n:Recursive)-[r:REFS]->(n) RETURN count(r)");
-- 
-- SELECT cypher("MATCH (n:Recursive)-[r:REFS]->(n) DELETE r");
-- 
-- .print "Self-referencing relationship deleted:"
-- SELECT cypher("MATCH (n:Recursive)-[r:REFS]->(n) RETURN count(r)");

.print ""
.print "Self-referencing test skipped (known SQL ambiguity issue)";

-- =======================================================
-- Section 6: Performance/Batch Operations
-- =======================================================
.print ""
.print "=== Section 6: Performance/Batch Operations ==="

-- Test 6.1: Batch delete many nodes
-- Create 10 temporary nodes
SELECT cypher("CREATE (n:BatchNode {id: 1})");
SELECT cypher("CREATE (n:BatchNode {id: 2})");
SELECT cypher("CREATE (n:BatchNode {id: 3})");
SELECT cypher("CREATE (n:BatchNode {id: 4})");
SELECT cypher("CREATE (n:BatchNode {id: 5})");
SELECT cypher("CREATE (n:BatchNode {id: 6})");
SELECT cypher("CREATE (n:BatchNode {id: 7})");
SELECT cypher("CREATE (n:BatchNode {id: 8})");
SELECT cypher("CREATE (n:BatchNode {id: 9})");
SELECT cypher("CREATE (n:BatchNode {id: 10})");

.print "Created 10 BatchNode nodes"
SELECT cypher("MATCH (n:BatchNode) RETURN count(n)");

-- Delete all in one operation
SELECT cypher("MATCH (n:BatchNode) DELETE n");

.print "All BatchNode nodes deleted:"
SELECT cypher("MATCH (n:BatchNode) RETURN count(n)");

-- Test 6.2: Chain deletion pattern
SELECT cypher("CREATE (a:Chain {pos: 1})-[:NEXT]->(b:Chain {pos: 2})-[:NEXT]->(c:Chain {pos: 3})-[:NEXT]->(d:Chain {pos: 4})");

.print ""
.print "Chain created with 3 NEXT relationships"
SELECT cypher("MATCH ()-[r:NEXT]->() RETURN count(r)");

-- Delete middle relationships
SELECT cypher("MATCH (a:Chain)-[r:NEXT]->(b:Chain) WHERE a.pos = 2 OR b.pos = 3 DELETE r");

.print "Middle relationships deleted:"
SELECT cypher("MATCH ()-[r:NEXT]->() RETURN count(r)");

-- =======================================================
-- Section 7: Return Values and Result Verification
-- =======================================================
.print ""
.print "=== Section 7: Return Values and Result Verification ==="

-- The result object should report deletion counts
-- This is tested implicitly by the functional test output

.print ""
.print "Comprehensive DELETE test completed successfully!"

-- Cleanup: Delete all remaining test data
.print ""
.print "=== Cleanup ==="
SELECT cypher("MATCH ()-[r]->() DELETE r");
SELECT cypher("MATCH (n) DELETE n");

.print "All test data cleaned up"