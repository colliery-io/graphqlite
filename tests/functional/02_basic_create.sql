-- Test 02: Basic CREATE Operations
-- Tests basic node creation with labels and properties

.load ./build/graphqlite

-- Test simple node creation
SELECT '=== Test 02: Basic CREATE Operations ===' as test_section;

-- Test 1: Simple node creation
SELECT 'Test 1 - Simple node:' as test_name;
SELECT cypher('CREATE (n)') as result;

-- Test 2: Node with label
SELECT 'Test 2 - Node with label:' as test_name;
SELECT cypher('CREATE (n:Person)') as result;

-- Test 3: Node with properties
SELECT 'Test 3 - Node with properties:' as test_name;
SELECT cypher("CREATE (n {name: 'Alice', age: 30})") as result;

-- Test 4: Node with label and properties
SELECT 'Test 4 - Node with label and properties:' as test_name;
SELECT cypher("CREATE (n:Person {name: 'Bob', age: 25})") as result;

-- Test 5: Multiple nodes
SELECT 'Test 5 - Multiple nodes:' as test_name;
SELECT cypher("CREATE (a:Person {name: 'Charlie'}), (b:Person {name: 'Diana'})") as result;

-- Verify data was created (check node count)
SELECT 'Verification - Total nodes created:' as test_name, COUNT(*) as node_count FROM nodes;