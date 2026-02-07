-- ========================================================================
-- Test 32: Cypher Dialect Parity
-- ========================================================================
-- PURPOSE: Test Neo4j/Memgraph compatibility features:
--          trailing semicolons, backtick-quoted property keys,
--          nested dot access, and bracket chaining
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 32: Cypher Dialect Parity ===' as test_section;

-- =======================================================================
-- SECTION 1: Trailing Semicolons
-- =======================================================================
SELECT '=== Section 1: Trailing Semicolons ===' as section;

SELECT 'Test 1.1 - Simple RETURN with trailing semicolon:' as test_name;
SELECT cypher('RETURN 1;') as result;

SELECT 'Test 1.2 - MATCH with trailing semicolon:' as test_name;
SELECT cypher('MATCH (n) RETURN n LIMIT 1;') as result;

SELECT 'Test 1.3 - EXPLAIN with trailing semicolon:' as test_name;
SELECT cypher('EXPLAIN MATCH (n) RETURN n LIMIT 1;') as result;

SELECT 'Test 1.4 - Query without trailing semicolon still works:' as test_name;
SELECT cypher('RETURN 42') as result;

SELECT '=== Trailing Semicolons Tests Complete ===' as section;
