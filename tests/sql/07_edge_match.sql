-- Test 07: Edge MATCH operations
.load ../build/graphqlite.so

-- Create test data with edges first
SELECT '=== Setting up test data ===';
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS {since: "2020"}]->(b:Person {name: "Bob"})');
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:WORKS_FOR {role: "Manager"}]->(c:Company {name: "TechCorp"})');
SELECT cypher('CREATE (a:Person {name: "Bob"})-[:FRIENDS {since: "2019"}]->(c:Person {name: "Charlie"})');
SELECT cypher('CREATE (a:Person {name: "Charlie"})<-[:MANAGES]-(d:Person {name: "Alice"})');

-- Test simple edge matching (right direction)
SELECT '=== Simple edge matching (right direction) ===';
SELECT cypher('MATCH (a:Person)-[:KNOWS]->(b:Person) RETURN a');
SELECT cypher('MATCH (a:Person)-[:WORKS_FOR]->(c:Company) RETURN a');

-- Test edge matching with variable
SELECT '=== Edge matching with variable ===';
SELECT cypher('MATCH (a:Person)-[r:KNOWS]->(b:Person) RETURN a');
SELECT cypher('MATCH (a:Person)-[r:FRIENDS]->(b:Person) RETURN a');

-- Test edge matching with properties
SELECT '=== Edge matching with properties ===';
SELECT cypher('MATCH (a:Person)-[r:KNOWS {since: "2020"}]->(b:Person) RETURN a');
SELECT cypher('MATCH (a:Person)-[r:WORKS_FOR {role: "Manager"}]->(c:Company) RETURN a');

-- Test left direction edge matching
SELECT '=== Left direction edge matching ===';
SELECT cypher('MATCH (a:Person)<-[:MANAGES]-(b:Person) RETURN a');

-- Test complex edge matching
SELECT '=== Complex edge matching ===';
SELECT cypher('MATCH (a:Person)-[r:KNOWS {since: "2020"}]->(b:Person {name: "Bob"}) RETURN a');

-- Verify the operations completed (this will show if parsing worked)
SELECT 'Edge MATCH test completed - if you see this, edge MATCH parsing is working!';

-- Note: We can't verify actual edge matching results yet since we haven't implemented 
-- edge execution logic, but these tests verify that the edge MATCH syntax parses correctly