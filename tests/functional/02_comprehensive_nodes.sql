-- Test 02b: Comprehensive Node Creation Tests
-- Exhaustive testing of node creation capabilities

.load ./build/graphqlite

SELECT '=== Test 02b: Comprehensive Node Creation ===' as test_section;

-- Test 1: Single node variations
SELECT 'Test 1.1 - Empty node:' as test_name;
SELECT cypher('CREATE ()') as result;

SELECT 'Test 1.2 - Node with variable only:' as test_name;
SELECT cypher('CREATE (n)') as result;

SELECT 'Test 1.3 - Node with label only:' as test_name;
SELECT cypher('CREATE (:Person)') as result;

SELECT 'Test 1.4 - Node with variable and label:' as test_name;
SELECT cypher('CREATE (p:Person)') as result;

SELECT 'Test 1.5 - Node with empty properties:' as test_name;
SELECT cypher('CREATE (n {})') as result;

SELECT 'Test 1.6 - Node with properties only:' as test_name;
SELECT cypher('CREATE ({name: "Anonymous"})') as result;

-- Test 2: Property types
SELECT 'Test 2.1 - String property:' as test_name;
SELECT cypher('CREATE (n {name: "Alice"})') as result;

SELECT 'Test 2.2 - Integer property:' as test_name;
SELECT cypher('CREATE (n {age: 30})') as result;

SELECT 'Test 2.3 - Float property:' as test_name;
SELECT cypher('CREATE (n {score: 95.5})') as result;

SELECT 'Test 2.4 - Boolean properties:' as test_name;
SELECT cypher('CREATE (n {active: true, verified: false})') as result;

SELECT 'Test 2.5 - Null property:' as test_name;
SELECT cypher('CREATE (n {value: null})') as result;

SELECT 'Test 2.6 - Mixed property types:' as test_name;
SELECT cypher('CREATE (n {name: "Bob", age: 25, score: 88.5, active: true})') as result;

-- Test 3: Multiple properties
SELECT 'Test 3.1 - Two properties:' as test_name;
SELECT cypher('CREATE (n {first: "Alice", last: "Smith"})') as result;

SELECT 'Test 3.2 - Many properties:' as test_name;
SELECT cypher('CREATE (n {a: 1, b: 2, c: 3, d: 4, e: 5})') as result;

-- Test 4: Label variations
SELECT 'Test 4.1 - Uppercase label:' as test_name;
SELECT cypher('CREATE (:PERSON)') as result;

SELECT 'Test 4.2 - Mixed case label:' as test_name;
SELECT cypher('CREATE (:PersonEntity)') as result;

SELECT 'Test 4.3 - Label with underscore:' as test_name;
SELECT cypher('CREATE (:person_type)') as result;

SELECT 'Test 4.4 - Label with number:' as test_name;
SELECT cypher('CREATE (:Person2)') as result;

-- Test 5: Variable name variations
SELECT 'Test 5.1 - Single letter variable:' as test_name;
SELECT cypher('CREATE (a)') as result;

SELECT 'Test 5.2 - Long variable name:' as test_name;
SELECT cypher('CREATE (thisIsAVeryLongVariableName)') as result;

SELECT 'Test 5.3 - Variable with underscore:' as test_name;
SELECT cypher('CREATE (my_node)') as result;

SELECT 'Test 5.4 - Variable with number:' as test_name;
SELECT cypher('CREATE (node123)') as result;

-- Test 6: Multiple nodes in single CREATE
SELECT 'Test 6.1 - Two empty nodes:' as test_name;
SELECT cypher('CREATE (), ()') as result;

SELECT 'Test 6.2 - Two nodes with variables:' as test_name;
SELECT cypher('CREATE (a), (b)') as result;

SELECT 'Test 6.3 - Two nodes with labels:' as test_name;
SELECT cypher('CREATE (:Person), (:Company)') as result;

SELECT 'Test 6.4 - Mixed node types:' as test_name;
SELECT cypher('CREATE (p:Person {name: "Alice"}), (:Company {name: "TechCorp"}), (c)') as result;

SELECT 'Test 6.5 - Five nodes:' as test_name;
SELECT cypher('CREATE (a), (b), (c), (d), (e)') as result;

-- Test 7: Complex property values
SELECT 'Test 7.1 - Empty string:' as test_name;
SELECT cypher('CREATE (n {name: ""})') as result;

SELECT 'Test 7.2 - String with spaces:' as test_name;
SELECT cypher('CREATE (n {desc: "This is a description"})') as result;

SELECT 'Test 7.3 - String with special chars:' as test_name;
SELECT cypher('CREATE (n {data: "Special: @#$%"})') as result;

SELECT 'Test 7.4 - Large integer:' as test_name;
SELECT cypher('CREATE (n {big: 1000000})') as result;

SELECT 'Test 7.5 - Negative integer:' as test_name;
SELECT cypher('CREATE (n {neg: -42})') as result;

SELECT 'Test 7.6 - Zero:' as test_name;
SELECT cypher('CREATE (n {zero: 0})') as result;

SELECT 'Test 7.7 - Decimal variations:' as test_name;
SELECT cypher('CREATE (n {pi: 3.14159, small: 0.001, neg: -2.5})') as result;

-- Test 8: Property key variations
SELECT 'Test 8.1 - Uppercase property key:' as test_name;
SELECT cypher('CREATE (n {NAME: "Alice"})') as result;

SELECT 'Test 8.2 - Mixed case property key:' as test_name;
SELECT cypher('CREATE (n {firstName: "Bob"})') as result;

SELECT 'Test 8.3 - Property key with underscore:' as test_name;
SELECT cypher('CREATE (n {first_name: "Carol"})') as result;

SELECT 'Test 8.4 - Property key starting with underscore:' as test_name;
SELECT cypher('CREATE (n {_id: 123})') as result;

-- Test 9: Label and property combinations
SELECT 'Test 9.1 - Same label, different properties:' as test_name;
SELECT cypher('CREATE (:Person {name: "Alice"}), (:Person {name: "Bob"})') as result;

SELECT 'Test 9.2 - Different labels, same property:' as test_name;
SELECT cypher('CREATE (:Person {id: 1}), (:Company {id: 2})') as result;

SELECT 'Test 9.3 - Complex combination:' as test_name;
SELECT cypher('CREATE (p1:Person {name: "Alice", age: 30}), (p2:Person {name: "Bob"}), (:Company {name: "Tech"})') as result;

-- Test 10: Repeated property keys (should use last value)
SELECT 'Test 10.1 - Duplicate property key:' as test_name;
SELECT cypher('CREATE (n {name: "First", name: "Last"})') as result;

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

SELECT 'Property keys used:' as test_name;
SELECT pk.key 
FROM property_keys pk 
ORDER BY pk.key;

SELECT 'Total properties set:' as test_name;
SELECT 
    (SELECT COUNT(*) FROM node_props_text) +
    (SELECT COUNT(*) FROM node_props_int) +
    (SELECT COUNT(*) FROM node_props_real) +
    (SELECT COUNT(*) FROM node_props_bool) as total_properties;

-- Cleanup note
SELECT '=== Test Complete ===' as section;
SELECT 'Note: Database contains test data from all node creation tests' as note;