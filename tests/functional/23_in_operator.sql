-- ========================================================================
-- Test 23: IN Operator
-- ========================================================================
-- PURPOSE: Test the IN operator for list membership checks
-- COVERS:  IN with literals, strings, integers, empty lists, NOT IN
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 23: IN Operator ===' as test_section;

-- =======================================================================
-- SETUP: Create test data
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, status: "active", level: 1})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25, status: "pending", level: 2})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", age: 35, status: "active", level: 3})') as setup;
SELECT cypher('CREATE (d:Person {name: "David", age: 28, status: "inactive", level: 1})') as setup;
SELECT cypher('CREATE (e:Person {name: "Eve", age: 32, status: "active", level: 2})') as setup;

SELECT cypher('CREATE (c1:City {name: "NYC", country: "USA", population: 8000000})') as setup;
SELECT cypher('CREATE (c2:City {name: "LA", country: "USA", population: 4000000})') as setup;
SELECT cypher('CREATE (c3:City {name: "London", country: "UK", population: 9000000})') as setup;
SELECT cypher('CREATE (c4:City {name: "Paris", country: "France", population: 2000000})') as setup;

-- =======================================================================
-- SECTION 1: IN with string lists
-- =======================================================================
SELECT '=== Section 1: IN with strings ===' as section;

SELECT 'Test 1.1 - Basic IN with string list:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name IN ["Alice", "Bob", "Carol"]
    RETURN p.name ORDER BY p.name
') as result;

SELECT 'Test 1.2 - IN with status values:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.status IN ["active", "pending"]
    RETURN p.name, p.status ORDER BY p.name
') as result;

SELECT 'Test 1.3 - IN with single element list:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name IN ["Alice"]
    RETURN p.name
') as result;

SELECT 'Test 1.4 - IN with no matches:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name IN ["Xavier", "Yolanda", "Zack"]
    RETURN p.name
') as result;

SELECT 'Test 1.5 - IN with countries:' as test_name;
SELECT cypher('
    MATCH (c:City)
    WHERE c.country IN ["USA", "UK"]
    RETURN c.name, c.country ORDER BY c.name
') as result;

-- =======================================================================
-- SECTION 2: IN with integer lists
-- =======================================================================
SELECT '=== Section 2: IN with integers ===' as section;

SELECT 'Test 2.1 - IN with integer list:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.age IN [25, 30, 35]
    RETURN p.name, p.age ORDER BY p.age
') as result;

SELECT 'Test 2.2 - IN with level values:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.level IN [1, 2]
    RETURN p.name, p.level ORDER BY p.name
') as result;

SELECT 'Test 2.3 - IN with large numbers:' as test_name;
SELECT cypher('
    MATCH (c:City)
    WHERE c.population IN [8000000, 9000000]
    RETURN c.name, c.population
') as result;

-- =======================================================================
-- SECTION 3: NOT IN operator
-- =======================================================================
SELECT '=== Section 3: NOT IN ===' as section;

SELECT 'Test 3.1 - NOT IN with strings:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE NOT p.status IN ["inactive"]
    RETURN p.name, p.status ORDER BY p.name
') as result;

SELECT 'Test 3.2 - NOT IN with names:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE NOT p.name IN ["Alice", "Bob"]
    RETURN p.name ORDER BY p.name
') as result;

SELECT 'Test 3.3 - NOT IN with integers:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE NOT p.level IN [3]
    RETURN p.name, p.level ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 4: IN combined with other conditions
-- =======================================================================
SELECT '=== Section 4: Combined conditions ===' as section;

SELECT 'Test 4.1 - IN with AND comparison:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.status IN ["active", "pending"] AND p.age > 28
    RETURN p.name, p.age, p.status ORDER BY p.name
') as result;

SELECT 'Test 4.2 - IN with OR:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name IN ["Alice", "Bob"] OR p.age > 34
    RETURN p.name, p.age ORDER BY p.name
') as result;

SELECT 'Test 4.3 - Multiple IN conditions:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.status IN ["active"] AND p.level IN [1, 2]
    RETURN p.name, p.status, p.level ORDER BY p.name
') as result;

SELECT 'Test 4.4 - IN with string operator:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.status IN ["active", "pending"] AND p.name STARTS WITH "A"
    RETURN p.name, p.status
') as result;

-- =======================================================================
-- SECTION 5: Edge cases
-- =======================================================================
SELECT '=== Section 5: Edge cases ===' as section;

SELECT 'Test 5.1 - Empty list IN:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name IN []
    RETURN p.name
') as result;
-- Should return empty

SELECT 'Test 5.2 - NOT IN empty list:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE NOT p.name IN []
    RETURN p.name ORDER BY p.name
') as result;
-- Should return all

SELECT 'Test 5.3 - IN with mixed case:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name IN ["alice", "Alice"]
    RETURN p.name
') as result;
-- Should return Alice (exact match)

SELECT 'Test 5.4 - IN list with many elements:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.age IN [20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30]
    RETURN p.name, p.age ORDER BY p.age
') as result;

SELECT '=== Test 23 Complete ===' as test_section;
