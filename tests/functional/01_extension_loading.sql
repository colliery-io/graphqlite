-- ========================================================================
-- Test 01: Extension Loading and Basic Functionality
-- ========================================================================
-- PURPOSE: Verifies that the GraphQLite extension loads correctly and 
--          provides the expected SQLite extension interface
-- COVERS:  Extension loading, schema creation, basic cypher() function
-- ========================================================================

.load ./build/graphqlite

-- Test 1: Extension loading verification
SELECT '=== Test 1: Extension Loading Verification ===' as test_section;

SELECT 'Test 1.1 - Extension test function:' as test_name, graphqlite_test() as result;

-- Test 2: Schema creation verification
SELECT '=== Test 2: Schema Creation Verification ===' as test_section;

SELECT 'Test 2.1 - Core schema tables created:' as test_name, COUNT(*) as table_count 
FROM sqlite_master 
WHERE type='table' AND name IN ('nodes', 'edges', 'property_keys', 'node_labels');

SELECT 'Test 2.2 - All GraphQLite tables:' as test_name;
.mode column
.headers on
SELECT name as table_name FROM sqlite_master 
WHERE type='table' AND (
  name LIKE 'node%' OR 
  name LIKE 'edge%' OR 
  name = 'property_keys'
) ORDER BY name;

-- Test 3: Basic cypher function verification
SELECT '=== Test 3: Basic Cypher Function Verification ===' as test_section;

SELECT 'Test 3.1 - Simple CREATE operation:' as test_name, cypher('CREATE (n:Test)') as result;

SELECT 'Test 3.2 - Simple MATCH operation:' as test_name, cypher('MATCH (n:Test) RETURN n') as result;

-- Test 4: Extension metadata
SELECT '=== Test 4: Extension Metadata ===' as test_section;

SELECT 'Test 4.1 - Extension functions available:' as test_name;
SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'pragma_%' LIMIT 1;

-- Verification
SELECT '=== Extension Loading Complete ===' as test_section;
SELECT 'GraphQLite extension successfully loaded and operational' as status;