-- Test DELETE for relationships/edges
-- This tests deleting specific relationships while keeping nodes

.load ./build/graphqlite

-- Create test nodes  
SELECT cypher('CREATE (a:RelTest {name: "Alice"})');
SELECT cypher('CREATE (b:RelTest {name: "Bob"})');

-- Create a relationship between them
SELECT cypher('CREATE (a:RelTest {name: "Alice"})-[:KNOWS]->(b:RelTest {name: "Bob"})');

-- Verify relationship exists
.print "Checking relationships before DELETE:"
SELECT COUNT(*) as relationship_count FROM edges;

-- Test relationship DELETE
.print ""
.print "Testing relationship DELETE:"
SELECT cypher('MATCH (a)-[r:KNOWS]->(b) DELETE r');

-- Verify relationship deleted but nodes remain
.print ""
.print "Checking relationships after DELETE:"
SELECT COUNT(*) as relationship_count FROM edges;

.print ""
.print "Checking nodes still exist:"
SELECT cypher('MATCH (n:RelTest) RETURN n.name');

.print ""
.print "Relationship DELETE test completed!"