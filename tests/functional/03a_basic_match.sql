-- Test 03: Basic MATCH Operations
-- Tests basic node querying and pattern matching

.load ./build/graphqlite

-- Setup: Create some test data
SELECT '=== Test 03: Basic MATCH Operations ===' as test_section;

SELECT 'Setup - Creating test data:' as test_name;
SELECT cypher("CREATE (a:Person {name: 'Alice', age: 30})") as result;
SELECT cypher("CREATE (b:Person {name: 'Bob', age: 25})") as result;
SELECT cypher("CREATE (c:Company {name: 'TechCorp'})") as result;

-- Test 1: Match all nodes
SELECT 'Test 1 - Match all nodes:' as test_name;
SELECT cypher('MATCH (n) RETURN n') as result;

-- Test 2: Match nodes by label
SELECT 'Test 2 - Match nodes by label:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n') as result;

-- Test 3: Match nodes by property
SELECT 'Test 3 - Match nodes by property:' as test_name;
SELECT cypher("MATCH (n {name: 'Alice'}) RETURN n") as result;

-- Test 4: Match nodes by label and property
SELECT 'Test 4 - Match nodes by label and property:' as test_name;
SELECT cypher('MATCH (n:Person {age: 25}) RETURN n') as result;

-- Test 5: Match with WHERE clause
SELECT 'Test 5 - Match with WHERE clause:' as test_name;
SELECT cypher('MATCH (n:Person) WHERE n.age > 25 RETURN n') as result;

-- Verification: Check that our test data exists
SELECT 'Verification - Person nodes:' as test_name, COUNT(*) as person_count 
FROM nodes n JOIN node_labels nl ON n.id = nl.node_id 
WHERE nl.label = 'Person';