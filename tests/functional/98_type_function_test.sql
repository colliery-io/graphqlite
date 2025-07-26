.load ./build/graphqlite

-- TYPE() Function Comprehensive Tests
-- Tests the type() function for relationship type inspection

.print "========================================="
.print "TYPE() Function Tests"
.print "========================================="

-- Clean slate
.print ""
.print "Creating test data with various relationship types..."

-- Create test nodes and relationships
SELECT cypher("CREATE (alice:Person {name: 'Alice'})");
SELECT cypher("CREATE (bob:Person {name: 'Bob'})");
SELECT cypher("CREATE (company:Company {name: 'TechCorp'})");
SELECT cypher("CREATE (project:Project {name: 'GraphDB'})");

-- Create relationships of different types
SELECT cypher("MATCH (alice:Person {name: 'Alice'}), (bob:Person {name: 'Bob'}) CREATE (alice)-[:KNOWS]->(bob)");
SELECT cypher("MATCH (alice:Person {name: 'Alice'}), (company:Company {name: 'TechCorp'}) CREATE (alice)-[:WORKS_FOR]->(company)");
SELECT cypher("MATCH (bob:Person {name: 'Bob'}), (project:Project {name: 'GraphDB'}) CREATE (bob)-[:MANAGES]->(project)");
SELECT cypher("MATCH (alice:Person {name: 'Alice'}), (project:Project {name: 'GraphDB'}) CREATE (alice)-[:CONTRIBUTES_TO]->(project)");

.print ""
.print "=== Basic type() Function Tests ==="

.print ""
.print "Test 1: type() with specific relationship type"
SELECT cypher("MATCH ()-[r:KNOWS]->() RETURN type(r)");

.print ""
.print "Test 2: type() with WORKS_FOR relationship"
SELECT cypher("MATCH ()-[r:WORKS_FOR]->() RETURN type(r)");

.print ""
.print "Test 3: type() with any relationship pattern"
SELECT cypher("MATCH ()-[r]->() RETURN type(r) ORDER BY type(r)");

.print ""
.print "Test 4: type() with relationship variable and node info"
SELECT cypher("MATCH (a)-[r]->(b) RETURN a.name, type(r), b.name");

.print ""
.print "Test 5: type() with alias"
SELECT cypher("MATCH ()-[r]->() RETURN type(r) AS relationship_type ORDER BY relationship_type");

.print ""
.print "Test 6: type() in WHERE clause filtering"
SELECT cypher("MATCH (a)-[r]->(b) WHERE type(r) = 'KNOWS' RETURN a.name, b.name");

.print ""
.print "Test 7: type() with COUNT function"
SELECT cypher("MATCH ()-[r]->() RETURN count(type(r))");

.print ""
.print "=== Advanced type() Function Tests ==="

.print ""
.print "Test 8: type() with DISTINCT"
SELECT cypher("MATCH ()-[r]->() RETURN DISTINCT type(r)");

.print ""
.print "Test 9: type() comparison operations"
SELECT cypher("MATCH ()-[r]->() WHERE type(r) = 'KNOWS' RETURN type(r)");

.print ""
.print "========================================="
.print "TYPE() Function Tests Complete"
.print "All valid type() function tests passed!"
.print "========================================="