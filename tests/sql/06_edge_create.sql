-- Test 06: Edge CREATE operations
.load ../build/graphqlite.so

-- Create nodes first (needed for relationships)
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35})');
SELECT cypher('CREATE (d:Company {name: "TechCorp"})');

-- Test simple edge creation (right direction)
SELECT '=== Simple edge creation (right direction) ===';
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:KNOWS]->(b:Person {name: "Bob"})');
SELECT cypher('CREATE (a:Person {name: "Alice"})-[:WORKS_FOR]->(d:Company {name: "TechCorp"})');

-- Test edge creation with variable
SELECT '=== Edge creation with variable ===';
SELECT cypher('CREATE (a:Person {name: "Bob"})-[r:FRIENDS]->(b:Person {name: "Charlie"})');

-- Test edge creation with properties  
SELECT '=== Edge creation with properties ===';
SELECT cypher('CREATE (a:Person {name: "Alice"})-[r:KNOWS {since: "2020", strength: 5}]->(b:Person {name: "Charlie"})');
SELECT cypher('CREATE (a:Person {name: "Bob"})-[r:WORKS_FOR {role: "Developer", startDate: "2021-01-01"}]->(d:Company {name: "TechCorp"})');

-- Test left direction edge creation
SELECT '=== Left direction edge creation ===';
SELECT cypher('CREATE (a:Person {name: "Charlie"})<-[:MANAGES]-(b:Person {name: "Alice"})');

-- Test edge creation with multiple properties
SELECT '=== Edge creation with multiple properties ===';
SELECT cypher('CREATE (a:Person {name: "Alice"})-[r:RATED {rating: 4.5, comment: "Great product!", verified: true}]->(p:Product {name: "Widget"})');

-- Verify the operations completed (this will show if parsing worked)
SELECT 'Test completed - if you see this, edge parsing is working!';

-- Note: We can't verify edge storage yet since we haven't implemented edge execution logic,
-- but these tests verify that the edge syntax parses correctly without errors