-- Test 01: Basic Extension Loading and Schema
-- Tests that the extension loads and creates the expected schema

.load ./build/graphqlite

-- Test extension loading
SELECT 'Extension loading test:' as test_name, graphqlite_test() as result;

-- Test schema creation 
SELECT 'Schema tables created:' as test_name, COUNT(*) as table_count 
FROM sqlite_master 
WHERE type='table' AND name IN ('nodes', 'edges', 'property_keys', 'node_labels');

-- List all GraphQLite tables
SELECT 'All GraphQLite tables:' as test_name;
.mode column
.headers on
SELECT name as table_name FROM sqlite_master 
WHERE type='table' AND (
  name LIKE 'node%' OR 
  name LIKE 'edge%' OR 
  name = 'property_keys'
) ORDER BY name;

-- Test basic cypher function
SELECT 'Basic cypher function test:' as test_name, cypher('CREATE (n:Test)') as result;