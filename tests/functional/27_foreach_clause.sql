-- ========================================================================
-- Test 27: FOREACH Clause
-- ========================================================================
-- PURPOSE: Test FOREACH iteration construct
-- COVERS:  FOREACH with CREATE, list literals
-- NOTE:    FOREACH currently only supports list literals, not expressions
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 27: FOREACH Clause ===' as test_section;

-- =======================================================================
-- SECTION 1: FOREACH with CREATE (string list)
-- =======================================================================
SELECT '=== Section 1: FOREACH CREATE strings ===' as section;

SELECT 'Test 1.1 - Create nodes from string list:' as test_name;
SELECT cypher('
    FOREACH (name IN ["Alice", "Bob", "Carol"] |
        CREATE (:ForeachPerson {name: name})
    )
') as result;

SELECT 'Test 1.2 - Verify created nodes:' as test_name;
SELECT cypher('MATCH (p:ForeachPerson) RETURN p.name ORDER BY p.name') as result;

-- =======================================================================
-- SECTION 2: FOREACH with CREATE (integer list)
-- =======================================================================
SELECT '=== Section 2: FOREACH CREATE integers ===' as section;

SELECT 'Test 2.1 - Create nodes from integer list:' as test_name;
SELECT cypher('
    FOREACH (i IN [1, 2, 3, 4, 5] |
        CREATE (:Counter {value: i})
    )
') as result;

SELECT 'Test 2.2 - Verify counter nodes:' as test_name;
SELECT cypher('MATCH (c:Counter) RETURN c.value ORDER BY c.value') as result;

-- =======================================================================
-- SECTION 3: FOREACH with multiple properties
-- =======================================================================
SELECT '=== Section 3: FOREACH multiple properties ===' as section;

SELECT 'Test 3.1 - Create with variable in multiple places:' as test_name;
SELECT cypher('
    FOREACH (x IN ["red", "green", "blue"] |
        CREATE (:Color {name: x, code: x})
    )
') as result;

SELECT 'Test 3.2 - Verify color nodes:' as test_name;
SELECT cypher('MATCH (c:Color) RETURN c.name, c.code ORDER BY c.name') as result;

-- =======================================================================
-- SECTION 4: FOREACH edge cases
-- =======================================================================
SELECT '=== Section 4: Edge cases ===' as section;

SELECT 'Test 4.1 - Empty list FOREACH:' as test_name;
SELECT cypher('
    FOREACH (x IN [] |
        CREATE (:Empty {value: x})
    )
') as result;

SELECT 'Test 4.2 - Verify no empty nodes created:' as test_name;
SELECT cypher('MATCH (e:Empty) RETURN count(e) AS count') as result;

SELECT 'Test 4.3 - One element list:' as test_name;
SELECT cypher('
    FOREACH (x IN ["only"] |
        CREATE (:OneItem {name: x})
    )
') as result;

SELECT 'Test 4.4 - Verify one node:' as test_name;
SELECT cypher('MATCH (s:OneItem) RETURN s.name') as result;

SELECT 'Test 4.5 - Large list:' as test_name;
SELECT cypher('
    FOREACH (n IN ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"] |
        CREATE (:Letter {char: n})
    )
') as result;

SELECT 'Test 4.6 - Verify all letters created:' as test_name;
SELECT cypher('MATCH (l:Letter) RETURN count(l) AS count') as result;

SELECT '=== Test 27 Complete ===' as test_section;
