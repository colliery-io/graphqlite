-- Test 10: WHERE clause functionality
.load ../build/graphqlite.so

-- Create comprehensive test data for WHERE clause testing
SELECT '=== Creating Test Data ===';
SELECT cypher('CREATE (n:Person {name: "Alice", age: 25, city: "New York", active: true, salary: 75000.50})');
SELECT cypher('CREATE (n:Person {name: "Bob", age: 28, city: "Boston", active: false, salary: 65000.00})');
SELECT cypher('CREATE (n:Person {name: "Charlie", age: 30, city: "Chicago", active: true, salary: 80000.25})');
SELECT cypher('CREATE (n:Person {name: "Diana", age: 22, city: "Denver", active: true, salary: 55000.75})');
SELECT cypher('CREATE (n:Person {name: "Eve", age: 35, city: "Seattle", active: false, salary: 95000.00})');
SELECT cypher('CREATE (n:Person {name: "Frank", age: 42, city: "Portland", active: true, salary: 85000.50})');
SELECT cypher('CREATE (n:Person {name: "Grace", age: 67, city: "Phoenix", active: false, salary: 45000.00})');
SELECT cypher('CREATE (n:Person {name: "Henry", age: 16, city: "Miami", active: true, salary: 25000.00})');

SELECT cypher('CREATE (n:Product {name: "Widget A", price: 50, rating: 4.2, inStock: true, discontinued: false})');
SELECT cypher('CREATE (n:Product {name: "Widget B", price: 100, rating: 3.8, inStock: false, discontinued: false})');
SELECT cypher('CREATE (n:Product {name: "Widget C", price: 75, rating: 4.5, inStock: true, discontinued: true})');
SELECT cypher('CREATE (n:Product {name: "Gadget X", price: 200, rating: 4.0, inStock: false, discontinued: false})');

-- Verify test data was created
SELECT '=== Test Data Verification ===';
SELECT 'Total nodes:', COUNT(*) FROM nodes;
SELECT 'Person nodes:', COUNT(*) FROM nodes n JOIN node_labels nl ON n.id = nl.node_id WHERE nl.label = 'Person';
SELECT 'Product nodes:', COUNT(*) FROM nodes n JOIN node_labels nl ON n.id = nl.node_id WHERE nl.label = 'Product';

-- ============================================================================
-- WHERE Clause with Equality Operators
-- ============================================================================

SELECT '=== WHERE EQUALITY TESTS ===';

-- Integer equality
SELECT 'Test: age = 25 (should find Alice)';
SELECT cypher('MATCH (n:Person) WHERE n.age = 25 RETURN n');

SELECT 'Test: age = 99 (should find nobody)';
SELECT cypher('MATCH (n:Person) WHERE n.age = 99 RETURN n');

-- String equality
SELECT 'Test: name = "Bob" (should find Bob)';
SELECT cypher('MATCH (n:Person) WHERE n.name = "Bob" RETURN n');

SELECT 'Test: city = "Chicago" (should find Charlie)';
SELECT cypher('MATCH (n:Person) WHERE n.city = "Chicago" RETURN n');

-- Boolean equality
SELECT 'Test: active = true (should find active people)';
SELECT cypher('MATCH (n:Person) WHERE n.active = true RETURN n');

SELECT 'Test: active = false (should find inactive people)';
SELECT cypher('MATCH (n:Person) WHERE n.active = false RETURN n');

-- Float equality
SELECT 'Test: rating = 4.5 (should find Widget C)';
SELECT cypher('MATCH (n:Product) WHERE n.rating = 4.5 RETURN n');

-- ============================================================================
-- WHERE Clause with Comparison Operators
-- ============================================================================

SELECT '=== WHERE COMPARISON TESTS ===';

-- Greater than
SELECT 'Test: age > 30 (should find older people)';
SELECT cypher('MATCH (n:Person) WHERE n.age > 30 RETURN n');

SELECT 'Test: price > 75 (should find expensive products)';
SELECT cypher('MATCH (n:Product) WHERE n.price > 75 RETURN n');

SELECT 'Test: salary > 80000.0 (should find high earners)';
SELECT cypher('MATCH (n:Person) WHERE n.salary > 80000.0 RETURN n');

-- Less than
SELECT 'Test: age < 25 (should find younger people)';
SELECT cypher('MATCH (n:Person) WHERE n.age < 25 RETURN n');

SELECT 'Test: price < 60 (should find cheap products)';
SELECT cypher('MATCH (n:Product) WHERE n.price < 60 RETURN n');

-- Greater than or equal
SELECT 'Test: age >= 30 (should find people 30 and older)';
SELECT cypher('MATCH (n:Person) WHERE n.age >= 30 RETURN n');

SELECT 'Test: rating >= 4.0 (should find high-rated products)';
SELECT cypher('MATCH (n:Product) WHERE n.rating >= 4.0 RETURN n');

-- Less than or equal
SELECT 'Test: age <= 25 (should find people 25 and younger)';
SELECT cypher('MATCH (n:Person) WHERE n.age <= 25 RETURN n');

SELECT 'Test: price <= 100 (should find affordable products)';
SELECT cypher('MATCH (n:Product) WHERE n.price <= 100 RETURN n');

-- Not equal
SELECT 'Test: age <> 25 (should find everyone except Alice)';
SELECT cypher('MATCH (n:Person) WHERE n.age <> 25 RETURN n');

SELECT 'Test: city <> "Boston" (should find everyone except Bob)';
SELECT cypher('MATCH (n:Person) WHERE n.city <> "Boston" RETURN n');

-- ============================================================================
-- WHERE Clause with Different Property Types
-- ============================================================================

SELECT '=== WHERE PROPERTY TYPE TESTS ===';

-- Mixed property types
SELECT 'Test: inStock = true (boolean property)';
SELECT cypher('MATCH (n:Product) WHERE n.inStock = true RETURN n');

SELECT 'Test: discontinued = false (boolean property)';
SELECT cypher('MATCH (n:Product) WHERE n.discontinued = false RETURN n');

-- Float comparisons with precision
SELECT 'Test: salary > 70000.0 (float comparison)';
SELECT cypher('MATCH (n:Person) WHERE n.salary > 70000.0 RETURN n');

SELECT 'Test: rating >= 4.0 (float comparison)';
SELECT cypher('MATCH (n:Product) WHERE n.rating >= 4.0 RETURN n');

-- ============================================================================
-- WHERE Clause Edge Cases and Error Handling
-- ============================================================================

SELECT '=== WHERE EDGE CASES ===';

-- Nonexistent properties
SELECT 'Test: nonexistent property (should find nobody)';
SELECT cypher('MATCH (n:Person) WHERE n.height > 180 RETURN n');

-- Type mismatches
SELECT 'Test: type mismatch - age as string (should find nobody)';
SELECT cypher('MATCH (n:Person) WHERE n.age = "25" RETURN n');

SELECT 'Test: type mismatch - name as number (should find nobody)';
SELECT cypher('MATCH (n:Person) WHERE n.name = 123 RETURN n');

-- Boundary values
SELECT 'Test: age >= 0 (should find everyone with age property)';
SELECT cypher('MATCH (n:Person) WHERE n.age >= 0 RETURN n');

SELECT 'Test: age < 1000 (should find everyone)';
SELECT cypher('MATCH (n:Person) WHERE n.age < 1000 RETURN n');

-- Empty results
SELECT 'Test: impossible condition (should find nobody)';
SELECT cypher('MATCH (n:Person) WHERE n.age > 150 RETURN n');

-- ============================================================================
-- WHERE Clause Data Validation
-- ============================================================================

SELECT '=== WHERE DATA VALIDATION ===';

-- Verify specific individuals are found correctly
SELECT 'Test: Find Alice specifically';
SELECT cypher('MATCH (n:Person) WHERE n.name = "Alice" RETURN n');

SELECT 'Test: Find Charlie specifically';
SELECT cypher('MATCH (n:Person) WHERE n.name = "Charlie" RETURN n');

SELECT 'Test: Find Grace specifically (senior citizen)';
SELECT cypher('MATCH (n:Person) WHERE n.name = "Grace" RETURN n');

SELECT 'Test: Find Henry specifically (minor)';
SELECT cypher('MATCH (n:Person) WHERE n.name = "Henry" RETURN n');

-- Age range validations
SELECT 'Test: Find adults (age >= 18)';
SELECT cypher('MATCH (n:Person) WHERE n.age >= 18 RETURN n');

SELECT 'Test: Find seniors (age >= 65)';
SELECT cypher('MATCH (n:Person) WHERE n.age >= 65 RETURN n');

SELECT 'Test: Find working age (18 <= age < 65) - using >= 18';
SELECT cypher('MATCH (n:Person) WHERE n.age >= 18 RETURN n');

-- Product validations
SELECT 'Test: Find available products';
SELECT cypher('MATCH (n:Product) WHERE n.inStock = true RETURN n');

SELECT 'Test: Find premium products (price > 150)';
SELECT cypher('MATCH (n:Product) WHERE n.price > 150 RETURN n');

SELECT 'Test: Find high-quality products (rating > 4.0)';
SELECT cypher('MATCH (n:Product) WHERE n.rating > 4.0 RETURN n');

-- ============================================================================
-- Database State Verification
-- ============================================================================

SELECT '=== DATABASE STATE VERIFICATION ===';

-- Verify our test data integrity by checking direct database queries
SELECT 'Property key distribution:';
SELECT pk.key, COUNT(*) as usage_count
FROM property_keys pk
LEFT JOIN node_props_text npt ON pk.id = npt.key_id
LEFT JOIN node_props_int npi ON pk.id = npi.key_id  
LEFT JOIN node_props_real npr ON pk.id = npr.key_id
LEFT JOIN node_props_bool npb ON pk.id = npb.key_id
GROUP BY pk.key
ORDER BY pk.key;

SELECT 'Age distribution:';
SELECT npi.value as age, COUNT(*) as count
FROM node_props_int npi
JOIN property_keys pk ON npi.key_id = pk.id
WHERE pk.key = 'age'
GROUP BY npi.value
ORDER BY npi.value;

SELECT 'City distribution:';
SELECT npt.value as city, COUNT(*) as count
FROM node_props_text npt
JOIN property_keys pk ON npt.key_id = pk.id
WHERE pk.key = 'city'
GROUP BY npt.value
ORDER BY npt.value;

SELECT 'Active status distribution:';
SELECT CASE npb.value WHEN 1 THEN 'active' ELSE 'inactive' END as status, COUNT(*) as count
FROM node_props_bool npb
JOIN property_keys pk ON npb.key_id = pk.id
WHERE pk.key = 'active'
GROUP BY npb.value
ORDER BY npb.value;

SELECT '=== WHERE CLAUSE TESTS COMPLETED ===';