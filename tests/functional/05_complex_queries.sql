-- Test 05: Complex Query Operations
-- Tests complex patterns, multiple hops, and advanced features
-- TEMPORARILY DISABLED - focusing on node operations first

/*
.load ./build/graphqlite

SELECT '=== Test 05: Complex Query Operations ===' as test_section;

-- Setup: Create a more complex graph
SELECT 'Setup - Creating complex graph structure:' as test_name;
SELECT cypher('CREATE (alice:Person {name: "Alice", age: 30})') as result;
SELECT cypher('CREATE (bob:Person {name: "Bob", age: 25})') as result;
SELECT cypher('CREATE (charlie:Person {name: "Charlie", age: 35})') as result;
SELECT cypher('CREATE (techcorp:Company {name: "TechCorp", founded: 2010})') as result;
SELECT cypher('CREATE (startup:Company {name: "StartupInc", founded: 2020})') as result;

-- Create relationships
SELECT cypher('CREATE (alice)-[:WORKS_FOR {since: "2022"}]->(techcorp)') as result;
SELECT cypher('CREATE (bob)-[:WORKS_FOR {since: "2023"}]->(startup)') as result;
SELECT cypher('CREATE (alice)-[:KNOWS {since: "2020"}]->(bob)') as result;
SELECT cypher('CREATE (alice)-[:KNOWS]->(charlie)') as result;
SELECT cypher('CREATE (charlie)-[:OWNS]->(startup)') as result;

-- Test 1: Multi-hop relationship
SELECT 'Test 1 - Multi-hop relationship:' as test_name;
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person)-[:WORKS_FOR]->(c:Company) RETURN a.name, b.name, c.name') as result;

-- Test 2: Complex pattern with properties
SELECT 'Test 2 - Complex pattern with properties:' as test_name;
SELECT cypher('MATCH (p:Person)-[:WORKS_FOR]->(c:Company) WHERE c.founded > 2015 RETURN p.name, c.name') as result;

-- Test 3: Variable length paths (if supported)
SELECT 'Test 3 - Variable patterns:' as test_name;
SELECT cypher('MATCH (a:Person {name: "Alice"})-[:KNOWS*1..2]->(connected) RETURN connected.name') as result;

-- Test 4: Count aggregation
SELECT 'Test 4 - Count aggregation:' as test_name;
SELECT cypher('MATCH (p:Person) RETURN COUNT(p) as person_count') as result;

-- Test 5: Multiple relationship types in pattern
SELECT 'Test 5 - Multiple relationship types:' as test_name;
SELECT cypher('MATCH (owner:Person)-[:OWNS]->(company:Company)<-[:WORKS_FOR]-(employee:Person) RETURN owner.name, company.name, employee.name') as result;

-- Test 6: Optional matches
SELECT 'Test 6 - Pattern with conditions:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.age > 30 MATCH (p)-[:WORKS_FOR]->(c:Company) RETURN p.name, c.name') as result;

-- Verification: Complex graph verification
SELECT 'Verification - Total nodes by label:' as test_name;
.mode column
.headers on
SELECT label, COUNT(*) as count FROM node_labels GROUP BY label ORDER BY label;

SELECT 'Verification - Relationship summary:' as test_name;
SELECT type, COUNT(*) as count FROM edges GROUP BY type ORDER BY count DESC;
*/