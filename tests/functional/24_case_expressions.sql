-- ========================================================================
-- Test 24: CASE Expressions
-- ========================================================================
-- PURPOSE: Test CASE WHEN...THEN...ELSE expressions
-- COVERS:  Simple CASE, searched CASE, nested CASE, CASE with NULL
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 24: CASE Expressions ===' as test_section;

-- =======================================================================
-- SETUP: Create test data
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, score: 85, status: "active"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 17, score: 92, status: "pending"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", age: 65, score: 78, status: "active"})') as setup;
SELECT cypher('CREATE (d:Person {name: "David", age: 45, score: 55, status: "inactive"})') as setup;
SELECT cypher('CREATE (e:Person {name: "Eve", age: 25, score: 100, status: "active"})') as setup;
SELECT cypher('CREATE (f:Person {name: "Frank", age: 35})') as setup;

SELECT cypher('CREATE (p1:Product {name: "Widget", price: 19.99, stock: 100})') as setup;
SELECT cypher('CREATE (p2:Product {name: "Gadget", price: 99.99, stock: 5})') as setup;
SELECT cypher('CREATE (p3:Product {name: "Thing", price: 9.99, stock: 0})') as setup;

-- =======================================================================
-- SECTION 1: Basic searched CASE
-- =======================================================================
SELECT '=== Section 1: Searched CASE ===' as section;

SELECT 'Test 1.1 - Age category classification:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           p.age,
           CASE
               WHEN p.age < 18 THEN "minor"
               WHEN p.age < 65 THEN "adult"
               ELSE "senior"
           END AS category
    ORDER BY p.age
') as result;

SELECT 'Test 1.2 - Score grade calculation:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.score IS NOT NULL
    RETURN p.name,
           p.score,
           CASE
               WHEN p.score >= 90 THEN "A"
               WHEN p.score >= 80 THEN "B"
               WHEN p.score >= 70 THEN "C"
               WHEN p.score >= 60 THEN "D"
               ELSE "F"
           END AS grade
    ORDER BY p.score DESC
') as result;

SELECT 'Test 1.3 - Status mapping:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           CASE
               WHEN p.status = "active" THEN "Currently Active"
               WHEN p.status = "pending" THEN "Awaiting Approval"
               WHEN p.status = "inactive" THEN "Deactivated"
               ELSE "Unknown"
           END AS status_text
    ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 2: CASE with ELSE
-- =======================================================================
SELECT '=== Section 2: CASE with ELSE ===' as section;

SELECT 'Test 2.1 - CASE with default value:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           CASE
               WHEN p.age > 50 THEN "Over 50"
               ELSE "50 or under"
           END AS age_group
    ORDER BY p.name
') as result;

SELECT 'Test 2.2 - Boolean result:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           CASE
               WHEN p.status = "active" THEN true
               ELSE false
           END AS is_active
    ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 3: CASE without ELSE (returns NULL)
-- =======================================================================
SELECT '=== Section 3: CASE without ELSE ===' as section;

SELECT 'Test 3.1 - No ELSE clause:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           p.age,
           CASE
               WHEN p.age < 18 THEN "underage"
               WHEN p.age > 60 THEN "retirement"
           END AS special_status
    ORDER BY p.name
') as result;
-- Most will be NULL except Bob (underage) and Carol (retirement)

-- =======================================================================
-- SECTION 4: CASE with property values
-- =======================================================================
SELECT '=== Section 4: CASE returning properties ===' as section;

SELECT 'Test 4.1 - Return different properties based on condition:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           CASE
               WHEN p.score >= 90 THEN p.name
               ELSE "not top performer"
           END AS top_performer
    ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 5: CASE in WHERE clause
-- =======================================================================
SELECT '=== Section 5: CASE in filters ===' as section;

SELECT 'Test 5.1 - Filter by CASE result:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE CASE
              WHEN p.age < 18 THEN "minor"
              WHEN p.age < 65 THEN "adult"
              ELSE "senior"
          END = "adult"
    RETURN p.name, p.age
    ORDER BY p.age
') as result;

-- =======================================================================
-- SECTION 6: CASE with products (stock levels)
-- =======================================================================
SELECT '=== Section 6: Stock level classification ===' as section;

SELECT 'Test 6.1 - Stock availability:' as test_name;
SELECT cypher('
    MATCH (p:Product)
    RETURN p.name,
           p.stock,
           CASE
               WHEN p.stock = 0 THEN "Out of Stock"
               WHEN p.stock < 10 THEN "Low Stock"
               WHEN p.stock < 50 THEN "In Stock"
               ELSE "Well Stocked"
           END AS availability
    ORDER BY p.stock
') as result;

SELECT 'Test 6.2 - Price tier:' as test_name;
SELECT cypher('
    MATCH (p:Product)
    RETURN p.name,
           p.price,
           CASE
               WHEN p.price < 20 THEN "Budget"
               WHEN p.price < 50 THEN "Standard"
               ELSE "Premium"
           END AS tier
    ORDER BY p.price
') as result;

-- =======================================================================
-- SECTION 7: Numeric results from CASE
-- =======================================================================
SELECT '=== Section 7: Numeric CASE results ===' as section;

SELECT 'Test 7.1 - Assign numeric values:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           p.status,
           CASE
               WHEN p.status = "active" THEN 1
               WHEN p.status = "pending" THEN 0
               ELSE -1
           END AS status_code
    ORDER BY p.name
') as result;

SELECT 'Test 7.2 - Calculate discount:' as test_name;
SELECT cypher('
    MATCH (p:Product)
    RETURN p.name,
           p.price,
           CASE
               WHEN p.stock = 0 THEN 0
               WHEN p.stock < 10 THEN 10
               ELSE 20
           END AS discount_percent
    ORDER BY p.name
') as result;

SELECT '=== Test 24 Complete ===' as test_section;
