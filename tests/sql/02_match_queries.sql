-- Test 02: MATCH queries
.load ./build/graphqlite.so

-- Create test data
SELECT cypher('CREATE (n:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (n:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (n:Company {name: "TechCorp", employees: 500})');
SELECT cypher('CREATE (n:Product {name: "Widget", price: 100, inStock: true})');
SELECT cypher('CREATE (n:Product {name: "Gadget", price: 200, inStock: false})');

-- Test MATCH by label
SELECT '=== MATCH by label ===';
SELECT cypher('MATCH (n:Person) RETURN n');
SELECT cypher('MATCH (n:Product) RETURN n');
SELECT cypher('MATCH (n:Company) RETURN n');

-- Test MATCH with property filter
SELECT '=== MATCH with property ===';
SELECT cypher('MATCH (n:Person {name: "Alice"}) RETURN n');
SELECT cypher('MATCH (n:Product {price: 100}) RETURN n');
SELECT cypher('MATCH (n:Product {inStock: true}) RETURN n');