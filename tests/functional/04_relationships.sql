-- Test 04: Relationship Operations
-- Tests relationship creation and querying

.load ./build/graphqlite

.print "=== Test 04: Relationship Operations ==="

-- Setup: Create nodes for relationship testing
.print "Setup - Creating nodes for relationships:";
SELECT cypher('CREATE (a:Person {name: "Alice"})') as result;
SELECT cypher('CREATE (b:Person {name: "Bob"})') as result;
SELECT cypher('CREATE (c:Company {name: "TechCorp"})') as result;

-- Test 1: Simple relationship creation
.print "Test 1 - Simple relationship:";
SELECT cypher('CREATE (a:Person {name: "Person1"})-[:KNOWS]->(b:Person {name: "Person2"})') as result;

-- Test 2: Relationship with properties (will fail for now - properties not implemented)
.print "Test 2 - Relationship with properties (expected to fail):";
-- SELECT cypher('CREATE (a:Person {name: "Manager"})-[:MANAGES {since: "2023"}]->(b:Person {name: "Employee"})') as result;

-- Test 3: Multiple relationship types
.print "Test 3 - Multiple relationship types:";
SELECT cypher('CREATE (a:Person {name: "Alice2"})-[:WORKS_FOR]->(c:Company {name: "Corp1"})') as result;
-- SELECT cypher('CREATE (a)-[:LIVES_IN]->(l:Location {name: "NYC"})') as result;

-- Test 4: Match relationships
.print "Test 4 - Match simple relationship:";
SELECT cypher('MATCH (a)-[:KNOWS]->(b) RETURN a.name, b.name') as result;

-- Test 5: Match relationship by type
.print "Test 5 - Match by relationship type:";
SELECT cypher('MATCH (a)-[:WORKS_FOR]->(b) RETURN a.name, b.name') as result;

-- Test 6: Bidirectional relationship matching
.print "Test 6 - Bidirectional relationship:";
SELECT cypher('MATCH (a)-[:KNOWS]-(b) RETURN a.name, b.name') as result;

-- Verification: Check edges were created
.print "Verification - Total edges:";
SELECT COUNT(*) as edge_count FROM edges;
.print "Verification - Edge types:";
.mode column
.headers on
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY type;