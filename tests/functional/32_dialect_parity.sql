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

-- =======================================================================
-- SECTION 2: Backtick-Quoted Property Keys
-- =======================================================================
SELECT '=== Section 2: Backtick-Quoted Property Keys ===' as section;

SELECT 'Test 2.1 - Create node with backtick-quoted key:' as test_name;
SELECT cypher('CREATE (n:BqTest {`special-key`: "hello", `with spaces`: 42, normal: "world"})') as result;

SELECT 'Test 2.2 - Read backtick-quoted property:' as test_name;
SELECT cypher('MATCH (n:BqTest) RETURN n.`special-key`') as result;

SELECT 'Test 2.3 - Read backtick property with spaces:' as test_name;
SELECT cypher('MATCH (n:BqTest) RETURN n.`with spaces`') as result;

SELECT 'Test 2.4 - Mix backtick and normal property access:' as test_name;
SELECT cypher('MATCH (n:BqTest) RETURN n.`special-key`, n.normal') as result;

SELECT 'Test 2.5 - Backtick property in WHERE clause:' as test_name;
SELECT cypher('MATCH (n:BqTest) WHERE n.`special-key` = "hello" RETURN n.normal') as result;

-- =======================================================================
-- SECTION 3: Nested Dot Access (JSON property drilling)
-- =======================================================================
SELECT '=== Section 3: Nested Dot Access ===' as section;

SELECT 'Test 3.1 - Create node with JSON-valued property:' as test_name;
SELECT cypher('CREATE (n:JsonTest {metadata: ''{"name": "Alice", "address": {"city": "NYC", "zip": "10001"}}''})') as result;

SELECT 'Test 3.2 - Single-level nested dot access:' as test_name;
SELECT cypher('MATCH (n:JsonTest) RETURN n.metadata.name') as result;

SELECT 'Test 3.3 - Two-level nested dot access:' as test_name;
SELECT cypher('MATCH (n:JsonTest) RETURN n.metadata.address.city') as result;

SELECT 'Test 3.4 - Nested dot in WHERE clause:' as test_name;
SELECT cypher('MATCH (n:JsonTest) WHERE n.metadata.name = "Alice" RETURN n.metadata.address.zip') as result;

-- =======================================================================
-- SECTION 4: Bracket Chaining
-- =======================================================================
SELECT '=== Section 4: Bracket Chaining ===' as section;

SELECT 'Test 4.1 - String-key bracket access (same as dot):' as test_name;
SELECT cypher('MATCH (n:BqTest) RETURN n[''normal'']') as result;

SELECT 'Test 4.2 - Bracket access matches dot access:' as test_name;
SELECT cypher('MATCH (n:BqTest) RETURN n[''special-key'']') as result;

SELECT 'Test 4.3 - Chained bracket access on JSON property:' as test_name;
SELECT cypher('MATCH (n:JsonTest) RETURN n[''metadata''][''name'']') as result;

-- =======================================================================
-- SECTION 5: Mixed Access Patterns
-- =======================================================================
SELECT '=== Section 5: Mixed Access Patterns ===' as section;

SELECT 'Test 5.1 - Bracket then dot on JSON:' as test_name;
SELECT cypher('MATCH (n:JsonTest) RETURN n[''metadata''].name') as result;

SELECT 'Test 5.2 - List subscript still works:' as test_name;
SELECT cypher('RETURN [10, 20, 30][1]') as result;

SELECT 'Test 5.3 - Existing property access unchanged:' as test_name;
SELECT cypher('CREATE (p:Person {name: "Bob", age: 25})')  as setup;
SELECT cypher('MATCH (p:Person {name: "Bob"}) RETURN p.name, p.age') as result;

SELECT 'Test 5.4 - end keyword as variable still works:' as test_name;
SELECT cypher('CREATE (end:EndTest {name: "EndNode"})') as setup;
SELECT cypher('MATCH (end:EndTest) RETURN end.name') as result;

SELECT '=== Cypher Dialect Parity Tests Complete ===' as section;
