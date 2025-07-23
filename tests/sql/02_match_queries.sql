-- Test 02: MATCH queries
.load ../build/graphqlite.so

-- Create test data
SELECT cypher('CREATE (n:Person {name: "Alice", age: 30})');
SELECT cypher('CREATE (n:Person {name: "Bob", age: 25})');
SELECT cypher('CREATE (n:Company {name: "TechCorp", employees: 500})');
SELECT cypher('CREATE (n:Product {name: "Widget", price: 100, inStock: true})');
SELECT cypher('CREATE (n:Product {name: "Gadget", price: 200, inStock: false})');

-- Test MATCH by label with data validation
SELECT '=== MATCH by label ===';

SELECT 'Test: MATCH Person nodes (should find Alice and Bob)';
SELECT cypher('MATCH (n:Person) RETURN n');
-- Validate we get Alice with age 30
-- Expected: Alice and Bob with their correct properties

SELECT 'Test: MATCH Product nodes (should find Widget and Gadget)';
SELECT cypher('MATCH (n:Product) RETURN n');
-- Validate we get both products with correct prices and stock status
-- Expected: Widget (price=100, inStock=true) and Gadget (price=200, inStock=false)

SELECT 'Test: MATCH Company nodes (should find TechCorp)';
SELECT cypher('MATCH (n:Company) RETURN n');
-- Validate we get TechCorp with 500 employees
-- Expected: TechCorp with employees=500

-- Test MATCH with property filter and data validation
SELECT '=== MATCH with property ===';

SELECT 'Test: Find Alice specifically by name';
SELECT cypher('MATCH (n:Person {name: "Alice"}) RETURN n');
-- Validate: Should return Alice with age=30, not Bob
-- Expected: {"id":X,"labels":["Person"],"properties":{"name":"Alice","age":30}}

SELECT 'Test: Find Product with price 100 (Widget)';
SELECT cypher('MATCH (n:Product {price: 100}) RETURN n');
-- Validate: Should return Widget with price=100 and inStock=true
-- Expected: {"id":X,"labels":["Product"],"properties":{"name":"Widget","price":100,"inStock":true}}

SELECT 'Test: Find in-stock products';
SELECT cypher('MATCH (n:Product {inStock: true}) RETURN n');
-- Validate: Should return Widget only (not Gadget which is out of stock)
-- Expected: Widget with inStock=true, price=100

-- Additional validation tests for data correctness
SELECT '=== Data Correctness Validation ===';

SELECT 'Test: Verify Alice has correct age (30, not 25)';
SELECT cypher('MATCH (n:Person {name: "Alice"}) RETURN n');

SELECT 'Test: Verify Bob has correct age (25, not 30)';
SELECT cypher('MATCH (n:Person {name: "Bob"}) RETURN n');

SELECT 'Test: Verify TechCorp has correct employee count (500)';
SELECT cypher('MATCH (n:Company {name: "TechCorp"}) RETURN n');

SELECT 'Test: Verify Widget is in stock and Gadget is out of stock';
SELECT cypher('MATCH (n:Product) RETURN n');

-- Test edge cases
SELECT '=== Edge Cases ===';

SELECT 'Test: Match nonexistent person (should return empty)';
SELECT cypher('MATCH (n:Person {name: "Nonexistent"}) RETURN n');

SELECT 'Test: Match wrong property value (should return empty)';
SELECT cypher('MATCH (n:Person {age: 999}) RETURN n');

SELECT 'Test: Match wrong label (should return empty)';
SELECT cypher('MATCH (n:Nonexistent) RETURN n');