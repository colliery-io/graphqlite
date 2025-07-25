-- Test 04: Relationship Operations
-- Tests relationship creation and querying
-- TEMPORARILY DISABLED - focusing on node operations first

/*
.load ./build/graphqlite

SELECT '=== Test 04: Relationship Operations ===' as test_section;

-- Setup: Create nodes for relationship testing
SELECT 'Setup - Creating nodes for relationships:' as test_name;
SELECT cypher('CREATE (a:Person {name: "Alice"})') as result;
SELECT cypher('CREATE (b:Person {name: "Bob"})') as result;
SELECT cypher('CREATE (c:Company {name: "TechCorp"})') as result;

-- Test 1: Simple relationship creation
SELECT 'Test 1 - Simple relationship:' as test_name;
SELECT cypher('CREATE (a:Person {name: "Person1"})-[:KNOWS]->(b:Person {name: "Person2"})') as result;

-- Test 2: Relationship with properties
SELECT 'Test 2 - Relationship with properties:' as test_name;
SELECT cypher('CREATE (a:Person {name: "Manager"})-[:MANAGES {since: "2023"}]->(b:Person {name: "Employee"})') as result;

-- Test 3: Multiple relationship types
SELECT 'Test 3 - Multiple relationship types:' as test_name;
SELECT cypher('CREATE (a:Person {name: "Alice2"})-[:WORKS_FOR]->(c:Company {name: "Corp1"})') as result;
SELECT cypher('CREATE (a)-[:LIVES_IN]->(l:Location {name: "NYC"})') as result;

-- Test 4: Match relationships
SELECT 'Test 4 - Match simple relationship:' as test_name;
SELECT cypher('MATCH (a)-[:KNOWS]->(b) RETURN a, b') as result;

-- Test 5: Match relationship by type
SELECT 'Test 5 - Match by relationship type:' as test_name;
SELECT cypher('MATCH (a)-[:WORKS_FOR]->(b) RETURN a, b') as result;

-- Test 6: Bidirectional relationship matching
SELECT 'Test 6 - Bidirectional relationship:' as test_name;
SELECT cypher('MATCH (a)-[:KNOWS]-(b) RETURN a, b') as result;

-- Verification: Check edges were created
SELECT 'Verification - Total edges:' as test_name, COUNT(*) as edge_count FROM edges;
SELECT 'Verification - Edge types:' as test_name;
.mode column
.headers on
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY type;
*/