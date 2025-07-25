-- Test 03b: Comprehensive MATCH Node Patterns
-- Exhaustive testing of node matching capabilities

.load ./build/graphqlite

SELECT '=== Test 03b: Comprehensive MATCH Node Patterns ===' as test_section;

-- Setup: Create diverse test data
SELECT '=== Setup - Creating comprehensive test data ===' as section;

-- People with various properties
SELECT cypher("CREATE (a:Person {name: 'Alice', age: 30, city: 'NYC'})") as setup;
SELECT cypher("CREATE (b:Person {name: 'Bob', age: 25, city: 'LA'})") as setup;
SELECT cypher("CREATE (c:Person {name: 'Carol', age: 30, city: 'NYC'})") as setup;
SELECT cypher("CREATE (d:Person {name: 'David', age: 35})") as setup; -- Missing city
SELECT cypher("CREATE (:Person {name: 'Eve', age: 28})") as setup; -- No variable

-- Companies
SELECT cypher("CREATE (tc:Company {name: 'TechCorp', employees: 100})") as setup;
SELECT cypher("CREATE (ac:Company {name: 'AcmeCorp', employees: 50})") as setup;
SELECT cypher("CREATE (:Company {name: 'StartupInc'})") as setup; -- No variable, no employees

-- Products
SELECT cypher("CREATE (p1:Product {name: 'Widget', price: 19.99})") as setup;
SELECT cypher("CREATE (p2:Product {name: 'Gadget', price: 29.99})") as setup;
SELECT cypher("CREATE (:Product {name: 'Thing', price: 9.99})") as setup;

-- Nodes without labels
SELECT cypher("CREATE (n1 {type: 'unknown', value: 'test'})") as setup;
SELECT cypher("CREATE ({type: 'anonymous'})") as setup;
SELECT cypher("CREATE (empty)") as setup; -- No label, no properties

-- Test 1: Basic MATCH patterns
SELECT '=== Test 1: Basic MATCH Patterns ===' as section;

SELECT 'Test 1.1 - Match all nodes:' as test_name;
SELECT cypher('MATCH (n) RETURN n') as result;

SELECT 'Test 1.2 - Match with any variable name:' as test_name;
SELECT cypher('MATCH (anything) RETURN anything') as result;

SELECT 'Test 1.3 - Match nodes without capturing:' as test_name;
SELECT cypher('MATCH () RETURN 1') as result;

-- Test 2: Label-based matching
SELECT '=== Test 2: Label-based Matching ===' as section;

SELECT 'Test 2.1 - Match by single label:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n') as result;

SELECT 'Test 2.2 - Match by label (Company):' as test_name;
SELECT cypher('MATCH (c:Company) RETURN c') as result;

SELECT 'Test 2.3 - Match by label (Product):' as test_name;
SELECT cypher('MATCH (p:Product) RETURN p') as result;

SELECT 'Test 2.4 - Match non-existent label:' as test_name;
SELECT cypher('MATCH (n:NonExistent) RETURN n') as result;

SELECT 'Test 2.5 - Different variable names with labels:' as test_name;
SELECT cypher('MATCH (person:Person) RETURN person') as result;

-- Test 3: Property-based matching
SELECT '=== Test 3: Property-based Matching ===' as section;

SELECT 'Test 3.1 - Match by single property:' as test_name;
SELECT cypher("MATCH (n {name: 'Alice'}) RETURN n") as result;

SELECT 'Test 3.2 - Match by integer property:' as test_name;
SELECT cypher('MATCH (n {age: 30}) RETURN n') as result;

SELECT 'Test 3.3 - Match by float property:' as test_name;
SELECT cypher('MATCH (n {price: 19.99}) RETURN n') as result;

SELECT 'Test 3.4 - Match by multiple properties:' as test_name;
SELECT cypher("MATCH (n {name: 'Alice', age: 30}) RETURN n") as result;

SELECT 'Test 3.5 - Match with non-existent property value:' as test_name;
SELECT cypher("MATCH (n {name: 'Nobody'}) RETURN n") as result;

SELECT 'Test 3.6 - Match nodes without specific property:' as test_name;
SELECT cypher('MATCH (n {type: "unknown"}) RETURN n') as result;

-- Test 4: Combined label and property matching
SELECT '=== Test 4: Label + Property Matching ===' as section;

SELECT 'Test 4.1 - Label with single property:' as test_name;
SELECT cypher("MATCH (n:Person {name: 'Bob'}) RETURN n") as result;

SELECT 'Test 4.2 - Label with multiple properties:' as test_name;
SELECT cypher("MATCH (n:Person {age: 30, city: 'NYC'}) RETURN n") as result;

SELECT 'Test 4.3 - Label with integer property:' as test_name;
SELECT cypher('MATCH (c:Company {employees: 100}) RETURN c') as result;

SELECT 'Test 4.4 - Wrong label, right property:' as test_name;
SELECT cypher("MATCH (n:Company {name: 'Alice'}) RETURN n") as result;

SELECT 'Test 4.5 - Right label, wrong property:' as test_name;
SELECT cypher('MATCH (n:Person {employees: 100}) RETURN n') as result;

-- Test 5: RETURN variations
SELECT '=== Test 5: RETURN Variations ===' as section;

SELECT 'Test 5.1 - Return with alias:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n AS person') as result;

SELECT 'Test 5.2 - Return property:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

SELECT 'Test 5.3 - Return property with alias:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name AS person_name') as result;

SELECT 'Test 5.4 - Return multiple items:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age') as result;

SELECT 'Test 5.5 - Return literal value:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n, "literal", 42') as result;

-- Test 6: WHERE clause patterns
SELECT '=== Test 6: WHERE Clause Patterns ===' as section;

SELECT 'Test 6.1 - WHERE with equality:' as test_name;
SELECT cypher("MATCH (n:Person) WHERE n.name = 'Alice' RETURN n") as result;

SELECT 'Test 6.2 - WHERE with inequality:' as test_name;
SELECT cypher("MATCH (n:Person) WHERE n.name <> 'Alice' RETURN n") as result;

SELECT 'Test 6.3 - WHERE with comparison:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 RETURN n') as result;

SELECT 'Test 6.4 - WHERE with less than:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age < 30 RETURN n') as result;

SELECT 'Test 6.5 - WHERE with AND:' as test_name;
SELECT cypher("MATCH (n:Person) WHERE n.age > 25 AND n.city = 'NYC' RETURN n") as result;

SELECT 'Test 6.6 - WHERE with OR:' as test_name;
SELECT cypher("MATCH (n:Person) WHERE n.age < 28 OR n.city = 'LA' RETURN n") as result;

-- Test 7: Edge cases and special patterns
SELECT '=== Test 7: Edge Cases ===' as section;

SELECT 'Test 7.1 - Match with empty properties:' as test_name;
SELECT cypher('MATCH (n {}) RETURN n') as result;

SELECT 'Test 7.2 - Multiple MATCH in single query:' as test_name;
SELECT cypher('MATCH (a:Person), (b:Company) RETURN a.name, b.name') as result;

SELECT 'Test 7.3 - MATCH with no results:' as test_name;
SELECT cypher('MATCH (n:NoSuchLabel) RETURN n') as result;

SELECT 'Test 7.4 - Case sensitivity in labels:' as test_name;
SELECT cypher('MATCH (n:person) RETURN n') as result; -- lowercase
SELECT cypher('MATCH (n:PERSON) RETURN n') as result; -- uppercase

-- Test 8: Property existence checks
SELECT '=== Test 8: Property Existence ===' as section;

SELECT 'Test 8.1 - Match nodes with specific property exists:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.city IS NOT NULL RETURN n') as result;

SELECT 'Test 8.2 - Match nodes where property is null:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.city IS NULL RETURN n') as result;

-- Verification queries
SELECT '=== Verification ===' as section;

SELECT 'Total nodes created:' as test_name, COUNT(*) as count FROM nodes;

SELECT 'Nodes by label:' as test_name;
.mode column
.headers on
SELECT nl.label, COUNT(*) as count 
FROM node_labels nl 
GROUP BY nl.label 
ORDER BY nl.label;

SELECT 'Properties distribution:' as test_name;
SELECT pk.key, COUNT(*) as usage_count
FROM property_keys pk
JOIN (
    SELECT key_id FROM node_props_text
    UNION ALL
    SELECT key_id FROM node_props_int
    UNION ALL
    SELECT key_id FROM node_props_real
) props ON pk.id = props.key_id
GROUP BY pk.key
ORDER BY usage_count DESC;

-- Cleanup note
SELECT '=== Test Complete ===' as section;
SELECT 'Note: This test creates 14 nodes with various labels and properties' as note;