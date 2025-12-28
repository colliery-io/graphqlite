-- ========================================================================
-- Test 21: UNION Queries
-- ========================================================================
-- PURPOSE: Test UNION and UNION ALL query combinations
-- COVERS:  Basic UNION, UNION ALL, column alignment, ordering with UNION
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 21: UNION Queries ===' as test_section;

-- =======================================================================
-- SETUP: Create test data
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, dept: "Engineering"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25, dept: "Marketing"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", age: 35, dept: "Engineering"})') as setup;
SELECT cypher('CREATE (d:Person {name: "David", age: 28, dept: "Sales"})') as setup;

SELECT cypher('CREATE (p1:Product {name: "Widget", category: "Tools"})') as setup;
SELECT cypher('CREATE (p2:Product {name: "Gadget", category: "Electronics"})') as setup;

-- =======================================================================
-- SECTION 1: Basic UNION (removes duplicates)
-- =======================================================================
SELECT '=== Section 1: Basic UNION ===' as section;

SELECT 'Test 1.1 - UNION of two queries with overlap:' as test_name;
SELECT cypher('
    MATCH (p:Person) WHERE p.dept = "Engineering" RETURN p.name AS name
    UNION
    MATCH (p:Person) WHERE p.age > 27 RETURN p.name AS name
') as result;
-- Should return Alice, Carol, David (Alice and Carol appear in both but only once)

SELECT 'Test 1.2 - UNION with no overlap:' as test_name;
SELECT cypher('
    MATCH (p:Person) WHERE p.dept = "Marketing" RETURN p.name AS name
    UNION
    MATCH (p:Person) WHERE p.dept = "Sales" RETURN p.name AS name
') as result;
-- Should return Bob, David

SELECT 'Test 1.3 - UNION with identical results (dedup):' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
    UNION
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
') as result;
-- Should return just Alice once

-- =======================================================================
-- SECTION 2: UNION ALL (keeps duplicates)
-- =======================================================================
SELECT '=== Section 2: UNION ALL ===' as section;

SELECT 'Test 2.1 - UNION ALL with overlap:' as test_name;
SELECT cypher('
    MATCH (p:Person) WHERE p.dept = "Engineering" RETURN p.name AS name
    UNION ALL
    MATCH (p:Person) WHERE p.age > 27 RETURN p.name AS name
') as result;
-- Should return Alice, Carol, Alice, Carol, David (duplicates kept)

SELECT 'Test 2.2 - UNION ALL with identical results:' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
    UNION ALL
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
') as result;
-- Should return Alice twice

-- =======================================================================
-- SECTION 3: UNION with multiple columns
-- =======================================================================
SELECT '=== Section 3: Multi-column UNION ===' as section;

SELECT 'Test 3.1 - UNION with two columns:' as test_name;
SELECT cypher('
    MATCH (p:Person) WHERE p.dept = "Engineering" RETURN p.name AS name, p.age AS value
    UNION
    MATCH (p:Product) RETURN p.name AS name, 0 AS value
') as result;
-- Should combine Person names/ages with Product names

SELECT 'Test 3.2 - UNION mixing node types:' as test_name;
SELECT cypher('
    MATCH (p:Person) RETURN p.name AS item, "person" AS type
    UNION
    MATCH (p:Product) RETURN p.name AS item, "product" AS type
') as result;

-- =======================================================================
-- SECTION 4: Multiple UNIONs
-- =======================================================================
SELECT '=== Section 4: Chained UNIONs ===' as section;

SELECT 'Test 4.1 - Three-way UNION:' as test_name;
SELECT cypher('
    MATCH (p:Person) WHERE p.dept = "Engineering" RETURN p.name AS name
    UNION
    MATCH (p:Person) WHERE p.dept = "Marketing" RETURN p.name AS name
    UNION
    MATCH (p:Person) WHERE p.dept = "Sales" RETURN p.name AS name
') as result;
-- Should return all four people

SELECT 'Test 4.2 - Mixed UNION and UNION ALL:' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
    UNION ALL
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
    UNION
    MATCH (p:Person {name: "Bob"}) RETURN p.name AS name
') as result;

-- =======================================================================
-- SECTION 5: UNION with ORDER BY and LIMIT
-- =======================================================================
SELECT '=== Section 5: UNION with modifiers ===' as section;

SELECT 'Test 5.1 - UNION with final ORDER BY:' as test_name;
SELECT cypher('
    MATCH (p:Person) WHERE p.dept = "Engineering" RETURN p.name AS name
    UNION
    MATCH (p:Person) WHERE p.dept = "Marketing" RETURN p.name AS name
    ORDER BY name
') as result;

SELECT 'Test 5.2 - UNION with LIMIT:' as test_name;
SELECT cypher('
    MATCH (p:Person) RETURN p.name AS name
    UNION
    MATCH (p:Product) RETURN p.name AS name
    LIMIT 3
') as result;

-- =======================================================================
-- SECTION 6: Edge cases
-- =======================================================================
SELECT '=== Section 6: Edge cases ===' as section;

SELECT 'Test 6.1 - UNION with empty first result:' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "NonExistent"}) RETURN p.name AS name
    UNION
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
') as result;
-- Should return just Alice

SELECT 'Test 6.2 - UNION with empty second result:' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "Alice"}) RETURN p.name AS name
    UNION
    MATCH (p:Person {name: "NonExistent"}) RETURN p.name AS name
') as result;
-- Should return just Alice

SELECT 'Test 6.3 - UNION with both empty:' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "X"}) RETURN p.name AS name
    UNION
    MATCH (p:Person {name: "Y"}) RETURN p.name AS name
') as result;
-- Should return empty

SELECT '=== Test 21 Complete ===' as test_section;
