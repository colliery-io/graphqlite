-- ========================================================================
-- Test 04: Query Patterns (MATCH, WHERE, Property Access)
-- ========================================================================
-- PURPOSE: Comprehensive testing of MATCH patterns, WHERE clause filtering,
--          property-based queries, and advanced pattern matching
-- COVERS:  Node patterns, label matching, property matching, WHERE clauses,
--          RETURN variations, pattern combinations, edge cases
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 04: Query Patterns ===' as test_section;

-- =======================================================================
-- SETUP: Create diverse test data for pattern matching
-- =======================================================================
SELECT '=== Setup: Creating diverse test data ===' as section;

-- People with various properties
SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, city: "NYC"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25, city: "LA"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", age: 30, city: "NYC"})') as setup;
SELECT cypher('CREATE (d:Person {name: "David", age: 35})') as setup; -- Missing city
SELECT cypher('CREATE (:Person {name: "Eve", age: 28})') as setup; -- No variable

-- Companies
SELECT cypher('CREATE (tc:Company {name: "TechCorp", employees: 100})') as setup;
SELECT cypher('CREATE (ac:Company {name: "AcmeCorp", employees: 50})') as setup;
SELECT cypher('CREATE (:Company {name: "StartupInc"})') as setup; -- No variable, no employees

-- Products
SELECT cypher('CREATE (p1:Product {name: "Widget", price: 19.99})') as setup;
SELECT cypher('CREATE (p2:Product {name: "Gadget", price: 29.99})') as setup;
SELECT cypher('CREATE (:Product {name: "Thing", price: 9.99})') as setup;

-- Nodes without labels
SELECT cypher('CREATE (n1 {type: "unknown", value: "test"})') as setup;
SELECT cypher('CREATE ({type: "anonymous"})') as setup;
SELECT cypher('CREATE (empty)') as setup; -- No label, no properties

-- =======================================================================
-- SECTION 1: Basic MATCH Patterns
-- =======================================================================
SELECT '=== Section 1: Basic MATCH Patterns ===' as section;

SELECT 'Test 1.1 - Match all nodes:' as test_name;
SELECT cypher('MATCH (n) RETURN n LIMIT 5') as result;

SELECT 'Test 1.2 - Match with any variable name:' as test_name;
SELECT cypher('MATCH (anything) RETURN anything LIMIT 3') as result;

SELECT 'Test 1.3 - Match nodes without capturing:' as test_name;
SELECT cypher('MATCH () RETURN 1 LIMIT 1') as result;

SELECT 'Test 1.4 - Match with different variable patterns:' as test_name;
SELECT cypher('MATCH (x), (y), (z) RETURN x.name, y.name, z.name LIMIT 2') as result;

-- =======================================================================
-- SECTION 2: Label-based Matching
-- =======================================================================
SELECT '=== Section 2: Label-based Matching ===' as section;

SELECT 'Test 2.1 - Match by single label (Person):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

SELECT 'Test 2.2 - Match by label (Company):' as test_name;
SELECT cypher('MATCH (c:Company) RETURN c.name') as result;

SELECT 'Test 2.3 - Match by label (Product):' as test_name;
SELECT cypher('MATCH (p:Product) RETURN p.name, p.price') as result;

SELECT 'Test 2.4 - Match non-existent label:' as test_name;
SELECT cypher('MATCH (n:NonExistent) RETURN n') as result;

SELECT 'Test 2.5 - Different variable names with labels:' as test_name;
SELECT cypher('MATCH (person:Person) RETURN person.name LIMIT 3') as result;

SELECT 'Test 2.6 - Case sensitivity in labels:' as test_name;
SELECT cypher('MATCH (n:person) RETURN n') as result; -- lowercase
SELECT cypher('MATCH (n:PERSON) RETURN n') as result; -- uppercase

-- =======================================================================
-- SECTION 3: Property-based Matching
-- =======================================================================
SELECT '=== Section 3: Property-based Matching ===' as section;

SELECT 'Test 3.1 - Match by single property:' as test_name;
SELECT cypher('MATCH (n {name: "Alice"}) RETURN n') as result;

SELECT 'Test 3.2 - Match by integer property:' as test_name;
SELECT cypher('MATCH (n {age: 30}) RETURN n.name') as result;

SELECT 'Test 3.3 - Match by float property:' as test_name;
SELECT cypher('MATCH (n {price: 19.99}) RETURN n.name') as result;

SELECT 'Test 3.4 - Match by multiple properties:' as test_name;
SELECT cypher('MATCH (n {name: "Alice", age: 30}) RETURN n') as result;

SELECT 'Test 3.5 - Match with non-existent property value:' as test_name;
SELECT cypher('MATCH (n {name: "Nobody"}) RETURN n') as result;

SELECT 'Test 3.6 - Match nodes with specific property type:' as test_name;
SELECT cypher('MATCH (n {type: "unknown"}) RETURN n') as result;

SELECT 'Test 3.7 - Match with empty properties:' as test_name;
SELECT cypher('MATCH (n {}) RETURN n LIMIT 3') as result;

-- =======================================================================
-- SECTION 4: Combined Label and Property Matching
-- =======================================================================
SELECT '=== Section 4: Label + Property Matching ===' as section;

SELECT 'Test 4.1 - Label with single property:' as test_name;
SELECT cypher('MATCH (n:Person {name: "Bob"}) RETURN n') as result;

SELECT 'Test 4.2 - Label with multiple properties:' as test_name;
SELECT cypher('MATCH (n:Person {age: 30, city: "NYC"}) RETURN n.name') as result;

SELECT 'Test 4.3 - Label with integer property:' as test_name;
SELECT cypher('MATCH (c:Company {employees: 100}) RETURN c.name') as result;

SELECT 'Test 4.4 - Wrong label, right property:' as test_name;
SELECT cypher('MATCH (n:Company {name: "Alice"}) RETURN n') as result;

SELECT 'Test 4.5 - Right label, wrong property:' as test_name;
SELECT cypher('MATCH (n:Person {employees: 100}) RETURN n') as result;

-- =======================================================================
-- SECTION 5: WHERE Clause Patterns
-- =======================================================================
SELECT '=== Section 5: WHERE Clause Patterns ===' as section;

SELECT 'Test 5.1 - WHERE with equality:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.name = "Alice" RETURN n.name') as result;

SELECT 'Test 5.2 - WHERE with inequality:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.name <> "Alice" RETURN n.name') as result;

SELECT 'Test 5.3 - WHERE with greater than:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 RETURN n.name, n.age') as result;

SELECT 'Test 5.4 - WHERE with less than:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age < 30 RETURN n.name, n.age') as result;

SELECT 'Test 5.5 - WHERE with greater than or equal:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age >= 30 RETURN n.name, n.age') as result;

SELECT 'Test 5.6 - WHERE with less than or equal:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age <= 30 RETURN n.name, n.age') as result;

SELECT 'Test 5.7 - WHERE with AND:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 AND n.city = "NYC" RETURN n.name') as result;

SELECT 'Test 5.8 - WHERE with OR:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age < 28 OR n.city = "LA" RETURN n.name') as result;

SELECT 'Test 5.9 - WHERE with NOT:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE NOT n.age = 30 RETURN n.name') as result;

-- =======================================================================
-- SECTION 6: Property Existence and Null Checks
-- =======================================================================
SELECT '=== Section 6: Property Existence and Null Checks ===' as section;

SELECT 'Test 6.1 - Match nodes with specific property exists:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.city IS NOT NULL RETURN n.name, n.city') as result;

SELECT 'Test 6.2 - Match nodes where property is null:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.city IS NULL RETURN n.name') as result;

SELECT 'Test 6.3 - Property existence using property access:' as test_name;
SELECT cypher('MATCH (n:Company) WHERE n.employees IS NOT NULL RETURN n.name, n.employees') as result;

-- =======================================================================
-- SECTION 7: RETURN Variations
-- =======================================================================
SELECT '=== Section 7: RETURN Variations ===' as section;

SELECT 'Test 7.1 - Return with alias:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n AS person LIMIT 2') as result;

SELECT 'Test 7.2 - Return property:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

SELECT 'Test 7.3 - Return property with alias:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name AS person_name') as result;

SELECT 'Test 7.4 - Return multiple items:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age LIMIT 3') as result;

SELECT 'Test 7.5 - Return literal values:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, "literal", 42 LIMIT 2') as result;

SELECT 'Test 7.6 - Return node and properties:' as test_name;
SELECT cypher('MATCH (n:Product) RETURN n, n.price') as result;

-- =======================================================================
-- SECTION 8: Complex Pattern Combinations
-- =======================================================================
SELECT '=== Section 8: Complex Pattern Combinations ===' as section;

SELECT 'Test 8.1 - Multiple MATCH in single query:' as test_name;
SELECT cypher('MATCH (a:Person), (b:Company) RETURN a.name, b.name LIMIT 3') as result;

SELECT 'Test 8.2 - Nested property access patterns:' as test_name;
SELECT cypher('MATCH (p:Product) WHERE p.price > 15.0 RETURN p.name, p.price') as result;

SELECT 'Test 8.3 - Complex WHERE with multiple conditions:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 AND n.city = "NYC" AND n.name <> "Bob" RETURN n.name') as result;

SELECT 'Test 8.4 - Pattern with property ranges:' as test_name;
SELECT cypher('MATCH (p:Product) WHERE p.price >= 10.0 AND p.price <= 25.0 RETURN p.name, p.price') as result;

-- =======================================================================
-- SECTION 9: Edge Cases and Special Patterns
-- =======================================================================
SELECT '=== Section 9: Edge Cases and Special Patterns ===' as section;

SELECT 'Test 9.1 - MATCH with no results:' as test_name;
SELECT cypher('MATCH (n:NoSuchLabel) RETURN n') as result;

SELECT 'Test 9.2 - Empty property matching:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.nonexistent = "value" RETURN n') as result;

SELECT 'Test 9.3 - Matching nodes with no labels:' as test_name;
-- NOTE: AND operator not fully implemented yet, but NOT label syntax now works
SELECT cypher('MATCH (n) WHERE NOT n:Person RETURN n LIMIT 3') as result;

SELECT 'Test 9.4 - Property comparison edge cases:' as test_name;
SELECT cypher('MATCH (n) WHERE n.age = 0 RETURN n') as result; -- Should match zero values

SELECT 'Test 9.5 - String property edge cases:' as test_name;
SELECT cypher('MATCH (n) WHERE n.name = "" RETURN n') as result; -- Empty string matching

-- =======================================================================
-- VERIFICATION: Query Pattern Analysis
-- =======================================================================
SELECT '=== Verification: Query Pattern Analysis ===' as section;

SELECT 'Total test nodes created:' as test_name, COUNT(*) as count FROM nodes;

SELECT 'Nodes by label distribution:' as test_name;
.mode column
.headers on
SELECT COALESCE(nl.label, '[no label]') as label, COUNT(*) as count 
FROM nodes n
LEFT JOIN node_labels nl ON n.id = nl.node_id
GROUP BY nl.label 
ORDER BY count DESC;

SELECT 'Properties distribution:' as test_name;
SELECT pk.key, COUNT(*) as usage_count
FROM property_keys pk
JOIN (
    SELECT key_id FROM node_props_text
    UNION ALL
    SELECT key_id FROM node_props_int
    UNION ALL
    SELECT key_id FROM node_props_real
    UNION ALL
    SELECT key_id FROM node_props_bool
) props ON pk.id = props.key_id
GROUP BY pk.key
ORDER BY usage_count DESC;

SELECT 'Nodes with multiple properties:' as test_name;
SELECT COUNT(DISTINCT node_id) as nodes_with_multiple_props
FROM (
    SELECT node_id FROM node_props_text
    UNION ALL
    SELECT node_id FROM node_props_int
    UNION ALL
    SELECT node_id FROM node_props_real
    UNION ALL
    SELECT node_id FROM node_props_bool
) 
GROUP BY node_id
HAVING COUNT(*) > 1;

-- Cleanup note
SELECT '=== Query Patterns Test Complete ===' as section;
SELECT 'Database contains diverse test data demonstrating comprehensive query pattern support' as note;