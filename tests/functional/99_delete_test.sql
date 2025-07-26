-- Test DELETE clause functionality
-- This is a simple test to verify DELETE operations work

.load ./build/graphqlite

-- Create test nodes
SELECT cypher("CREATE (a:DeleteTest {name: 'Alice', age: 30})");
SELECT cypher("CREATE (b:DeleteTest {name: 'Bob', age: 25})");
SELECT cypher("CREATE (c:DeleteTest {name: 'Charlie', age: 35})");

-- Verify nodes were created
.print "Nodes before DELETE:"
SELECT cypher("MATCH (n:DeleteTest) RETURN n.name, n.age");

-- Delete one specific node
.print ""
.print "Attempting DELETE operation:"
SELECT cypher("MATCH (n:DeleteTest) WHERE n.name = 'Bob' DELETE n");

-- Verify deletion
.print ""
.print "Nodes after DELETE Bob:"
SELECT cypher("MATCH (n:DeleteTest) RETURN n.name, n.age");

-- Check if we still have 3 nodes or fewer
.print ""
.print "Direct count of DeleteTest nodes in database:"
SELECT COUNT(*) as node_count FROM node_labels WHERE label = 'DeleteTest';

.print ""
.print "DELETE test completed successfully!"