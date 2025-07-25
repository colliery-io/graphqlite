-- ========================================================================
-- Test 02: Node Operations (CREATE, MATCH, Properties)
-- ========================================================================
-- PURPOSE: Comprehensive testing of node creation, property handling,
--          and basic node matching operations
-- COVERS:  Node creation patterns, property types, label variations,
--          variable naming, multiple nodes, complex properties
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 02: Node Operations ===' as test_section;

-- =======================================================================
-- SECTION 1: Basic Node Creation Patterns
-- =======================================================================
SELECT '=== Section 1: Basic Node Creation Patterns ===' as section;

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

-- =======================================================================
-- SECTION 2: Property Types and Values
-- =======================================================================
SELECT '=== Section 2: Property Types and Values ===' as section;

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

-- =======================================================================
-- SECTION 3: Special Property Values
-- =======================================================================
SELECT '=== Section 3: Special Property Values ===' as section;

SELECT 'Test 3.1 - Empty string:' as test_name;
SELECT cypher('CREATE (n {name: ""})') as result;

SELECT 'Test 3.2 - String with spaces:' as test_name;
SELECT cypher('CREATE (n {desc: "This is a description"})') as result;

SELECT 'Test 3.3 - String with special chars:' as test_name;
SELECT cypher('CREATE (n {data: "Special: @#$%"})') as result;

SELECT 'Test 3.4 - Large integer:' as test_name;
SELECT cypher('CREATE (n {big: 1000000})') as result;

SELECT 'Test 3.5 - Negative integer:' as test_name;
SELECT cypher('CREATE (n {neg: -42})') as result;

SELECT 'Test 3.6 - Zero:' as test_name;
SELECT cypher('CREATE (n {zero: 0})') as result;

SELECT 'Test 3.7 - Decimal variations:' as test_name;
SELECT cypher('CREATE (n {pi: 3.14159, small: 0.001, neg: -2.5})') as result;

-- =======================================================================
-- SECTION 4: Label and Variable Variations
-- =======================================================================
SELECT '=== Section 4: Label and Variable Variations ===' as section;

SELECT 'Test 4.1 - Uppercase label:' as test_name;
SELECT cypher('CREATE (:PERSON)') as result;

SELECT 'Test 4.2 - Mixed case label:' as test_name;
SELECT cypher('CREATE (:PersonEntity)') as result;

SELECT 'Test 4.3 - Label with underscore:' as test_name;
SELECT cypher('CREATE (:person_type)') as result;

SELECT 'Test 4.4 - Label with number:' as test_name;
SELECT cypher('CREATE (:Person2)') as result;

SELECT 'Test 4.5 - Single letter variable:' as test_name;
SELECT cypher('CREATE (a)') as result;

SELECT 'Test 4.6 - Long variable name:' as test_name;
SELECT cypher('CREATE (thisIsAVeryLongVariableName)') as result;

SELECT 'Test 4.7 - Variable with underscore:' as test_name;
SELECT cypher('CREATE (my_node)') as result;

SELECT 'Test 4.8 - Variable with number:' as test_name;
SELECT cypher('CREATE (node123)') as result;

-- =======================================================================
-- SECTION 5: Multiple Node Creation
-- =======================================================================
SELECT '=== Section 5: Multiple Node Creation ===' as section;

SELECT 'Test 5.1 - Two empty nodes:' as test_name;
SELECT cypher('CREATE (), ()') as result;

SELECT 'Test 5.2 - Two nodes with variables:' as test_name;
SELECT cypher('CREATE (a), (b)') as result;

SELECT 'Test 5.3 - Two nodes with labels:' as test_name;
SELECT cypher('CREATE (:Person), (:Company)') as result;

SELECT 'Test 5.4 - Mixed node types:' as test_name;
SELECT cypher('CREATE (p:Person {name: "Alice"}), (:Company {name: "TechCorp"}), (c)') as result;

SELECT 'Test 5.5 - Five nodes:' as test_name;
SELECT cypher('CREATE (a), (b), (c), (d), (e)') as result;

-- =======================================================================
-- SECTION 6: Property Key Variations
-- =======================================================================
SELECT '=== Section 6: Property Key Variations ===' as section;

SELECT 'Test 6.1 - Uppercase property key:' as test_name;
SELECT cypher('CREATE (n {NAME: "Alice"})') as result;

SELECT 'Test 6.2 - Mixed case property key:' as test_name;
SELECT cypher('CREATE (n {firstName: "Bob"})') as result;

SELECT 'Test 6.3 - Property key with underscore:' as test_name;
SELECT cypher('CREATE (n {first_name: "Carol"})') as result;

SELECT 'Test 6.4 - Property key starting with underscore:' as test_name;
SELECT cypher('CREATE (n {_id: 123})') as result;

-- =======================================================================
-- SECTION 7: Advanced Property Patterns
-- =======================================================================
SELECT '=== Section 7: Advanced Property Patterns ===' as section;

SELECT 'Test 7.1 - Multiple properties:' as test_name;
SELECT cypher('CREATE (n {first: "Alice", last: "Smith"})') as result;

SELECT 'Test 7.2 - Many properties:' as test_name;
SELECT cypher('CREATE (n {a: 1, b: 2, c: 3, d: 4, e: 5})') as result;

SELECT 'Test 7.3 - Same label, different properties:' as test_name;
SELECT cypher('CREATE (:Person {name: "Alice"}), (:Person {name: "Bob"})') as result;

SELECT 'Test 7.4 - Different labels, same property:' as test_name;
SELECT cypher('CREATE (:Person {id: 1}), (:Company {id: 2})') as result;

SELECT 'Test 7.5 - Complex combination:' as test_name;
SELECT cypher('CREATE (p1:Person {name: "Alice", age: 30}), (p2:Person {name: "Bob"}), (:Company {name: "Tech"})') as result;

SELECT 'Test 7.6 - Duplicate property key (should use last value):' as test_name;
SELECT cypher('CREATE (n {name: "First", name: "Last"})') as result;

-- =======================================================================
-- VERIFICATION: Database State Analysis
-- =======================================================================
SELECT '=== Verification: Database State Analysis ===' as section;

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

SELECT 'Properties by type:' as test_name;
SELECT 'text' as type, COUNT(*) as count FROM node_props_text
UNION ALL
SELECT 'integer' as type, COUNT(*) as count FROM node_props_int
UNION ALL
SELECT 'real' as type, COUNT(*) as count FROM node_props_real
UNION ALL
SELECT 'boolean' as type, COUNT(*) as count FROM node_props_bool
ORDER BY count DESC;

-- Cleanup note
SELECT '=== Node Operations Test Complete ===' as section;
SELECT 'Database contains comprehensive node creation test data' as note;