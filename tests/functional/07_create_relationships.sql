-- Test 07: CREATE Relationships
-- Tests relationship creation functionality

-- Load GraphQLite extension
.load ./build/graphqlite

.print "=== Test 07: CREATE Relationships ==="

-- Test 1: Simple relationship creation
.print "=== Test 1: Simple relationship creation ===" ;

.print "Test 1.1 - Create two nodes and a relationship:";
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS]->(b:Person {name: "Bob"})') as result;

.print "Test 1.2 - Verify nodes were created:";
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

.print "Test 1.3 - Verify relationship was created:";
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a.name, b.name') as result;

-- Test 2: Different relationship types
.print "=== Test 2: Different relationship types ===";

.print "Test 2.1 - Create LIKES relationship:";
SELECT cypher('CREATE (c:Person {name: "Charlie"})-[:LIKES]->(d:Person {name: "David"})') as result;

.print "Test 2.2 - Create WORKS_WITH relationship:";
SELECT cypher('CREATE (e:Person {name: "Eve"})-[:WORKS_WITH]->(f:Person {name: "Frank"})') as result;

.print "Test 2.3 - Verify different relationship types:";
SELECT cypher('MATCH (a)-[r]->(b) RETURN a.name, b.name') as result;

-- Test 3: Bidirectional relationships
.print "=== Test 3: Bidirectional relationships ===";

.print "Test 3.1 - Create left-directed relationship:";
SELECT cypher('CREATE (g:Person {name: "Grace"})<-[:FOLLOWS]-(h:Person {name: "Henry"})') as result;

.print "Test 3.2 - Verify bidirectional relationship:";
SELECT cypher('MATCH (a)<-[r:FOLLOWS]-(b) RETURN a.name, b.name') as result;

-- Test 4: Relationship variables
.print "=== Test 4: Relationship variables ===";

.print "Test 4.1 - Create relationship with variable:";
SELECT cypher('CREATE (i:Person {name: "Iris"})-[rel:MENTORS]->(j:Person {name: "Jack"})') as result;

.print "Test 4.2 - Return relationship variable:";
SELECT cypher('MATCH (a:Person)-[r:MENTORS]->(b:Person) RETURN a.name, r, b.name') as result;

-- Verification
.print "=== Verification ===";
.print "Total nodes created:";
SELECT cypher('MATCH (n) RETURN n') as total_nodes;

.print "All relationships:";
SELECT cypher('MATCH (a)-[r]->(b) RETURN a.name, b.name') as all_relationships;