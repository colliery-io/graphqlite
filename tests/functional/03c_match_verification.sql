-- Test 03c: MATCH Result Verification
-- Tests to verify MATCH queries return correct results

.load ./build/graphqlite

SELECT '=== Test 03c: MATCH Result Verification ===' as test_section;

-- Clean database for predictable results
DELETE FROM edges;
DELETE FROM node_labels;
DELETE FROM node_props_text;
DELETE FROM node_props_int; 
DELETE FROM node_props_real;
DELETE FROM node_props_bool;
DELETE FROM nodes;
DELETE FROM property_keys;

-- Create specific test data
SELECT '=== Creating specific test nodes ===' as section;
SELECT cypher("CREATE (:Person {name: 'Alice', age: 30})") as setup;
SELECT cypher("CREATE (:Person {name: 'Bob', age: 25})") as setup;
SELECT cypher("CREATE (:Company {name: 'TechCorp'})") as setup;

-- Test actual vs expected results
SELECT '=== Verifying MATCH Results ===' as section;

-- First check what's actually in the database
SELECT 'Database contents:' as info;
.mode column
.headers on

SELECT 'Nodes table:' as table_name;
SELECT * FROM nodes;

SELECT 'Node labels:' as table_name;
SELECT * FROM node_labels;

SELECT 'Property keys:' as table_name;
SELECT * FROM property_keys;

SELECT 'Text properties:' as table_name;
SELECT n.id as node_id, pk.key, np.value
FROM node_props_text np
JOIN nodes n ON np.node_id = n.id
JOIN property_keys pk ON np.key_id = pk.id;

SELECT 'Integer properties:' as table_name;
SELECT n.id as node_id, pk.key, np.value
FROM node_props_int np
JOIN nodes n ON np.node_id = n.id
JOIN property_keys pk ON np.key_id = pk.id;

-- Now test MATCH queries
.headers off
SELECT '=== Testing MATCH Queries ===' as section;

SELECT 'Test 1: MATCH (n) RETURN n - Should return 3 nodes' as test;
SELECT cypher('MATCH (n) RETURN n') as result;

SELECT 'Test 2: MATCH (n:Person) RETURN n - Should return 2 nodes' as test;
SELECT cypher('MATCH (n:Person) RETURN n') as result;

SELECT 'Test 3: MATCH (n:Company) RETURN n - Should return 1 node' as test;
SELECT cypher('MATCH (n:Company) RETURN n') as result;

SELECT 'Test 4: MATCH (n {name: "Alice"}) RETURN n - Should return 1 node' as test;
SELECT cypher("MATCH (n {name: 'Alice'}) RETURN n") as result;

SELECT 'Test 5: MATCH (n:Person {age: 25}) RETURN n - Should return Bob' as test;
SELECT cypher('MATCH (n:Person {age: 25}) RETURN n') as result;

-- Test what properties are actually returned
SELECT 'Test 6: MATCH (n:Person) RETURN n.name - Should return Alice, Bob' as test;
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

SELECT 'Test 7: MATCH (n:Person) RETURN n.name, n.age - Should return names and ages' as test;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age') as result;

-- Manual query to show expected results
SELECT '=== Expected Results (via SQL) ===' as section;

SELECT 'All Person nodes via SQL:' as query;
.headers on
SELECT n.id, nl.label, 
       GROUP_CONCAT(pk.key || '=' || 
                    COALESCE(npt.value, CAST(npi.value as TEXT), CAST(npr.value as TEXT), CAST(npb.value as TEXT)), 
                    ', ') as properties
FROM nodes n
JOIN node_labels nl ON n.id = nl.node_id
LEFT JOIN node_props_text npt ON n.id = npt.node_id
LEFT JOIN node_props_int npi ON n.id = npi.node_id AND npt.node_id IS NULL
LEFT JOIN node_props_real npr ON n.id = npr.node_id AND npt.node_id IS NULL AND npi.node_id IS NULL
LEFT JOIN node_props_bool npb ON n.id = npb.node_id AND npt.node_id IS NULL AND npi.node_id IS NULL AND npr.node_id IS NULL
LEFT JOIN property_keys pk ON pk.id = COALESCE(npt.key_id, npi.key_id, npr.key_id, npb.key_id)
WHERE nl.label = 'Person'
GROUP BY n.id, nl.label;