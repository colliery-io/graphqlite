-- ========================================================================
-- Test 25: COALESCE and NULL Handling
-- ========================================================================
-- PURPOSE: Test COALESCE function and IS NULL / IS NOT NULL predicates
-- COVERS:  COALESCE with multiple args, NULL checks, default values
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 25: COALESCE and NULL Handling ===' as test_section;

-- =======================================================================
-- SETUP: Create test data with missing properties
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", email: "alice@example.com", phone: "555-0001", nickname: "Ali"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", email: "bob@example.com"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", phone: "555-0003"})') as setup;
SELECT cypher('CREATE (d:Person {name: "David"})') as setup;
SELECT cypher('CREATE (e:Person {name: "Eve", email: "eve@example.com", nickname: "Evie"})') as setup;

SELECT cypher('CREATE (c1:Config {key: "timeout", value: "30", default_value: "60"})') as setup;
SELECT cypher('CREATE (c2:Config {key: "retries", default_value: "3"})') as setup;
SELECT cypher('CREATE (c3:Config {key: "debug", value: "true"})') as setup;

-- =======================================================================
-- SECTION 1: Basic COALESCE
-- =======================================================================
SELECT '=== Section 1: Basic COALESCE ===' as section;

SELECT 'Test 1.1 - COALESCE with two arguments:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name, COALESCE(p.email, "no email") AS contact
    ORDER BY p.name
') as result;

SELECT 'Test 1.2 - COALESCE with three arguments:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name, COALESCE(p.nickname, p.email, "unknown") AS display
    ORDER BY p.name
') as result;

SELECT 'Test 1.3 - COALESCE all present (returns first):' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "Alice"})
    RETURN COALESCE(p.name, p.email, "default") AS result
') as result;

SELECT 'Test 1.4 - COALESCE all NULL (returns last):' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "David"})
    RETURN COALESCE(p.email, p.phone, p.nickname, "no contact") AS result
') as result;

-- =======================================================================
-- SECTION 2: COALESCE for defaults
-- =======================================================================
SELECT '=== Section 2: Default values ===' as section;

SELECT 'Test 2.1 - Config value with default:' as test_name;
SELECT cypher('
    MATCH (c:Config)
    RETURN c.key, COALESCE(c.value, c.default_value) AS effective_value
    ORDER BY c.key
') as result;

SELECT 'Test 2.2 - Contact preference chain:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           COALESCE(p.phone, p.email, "no contact method") AS preferred_contact
    ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 3: IS NULL predicate
-- =======================================================================
SELECT '=== Section 3: IS NULL ===' as section;

SELECT 'Test 3.1 - Find nodes with missing email:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NULL
    RETURN p.name
    ORDER BY p.name
') as result;

SELECT 'Test 3.2 - Find nodes with missing phone:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.phone IS NULL
    RETURN p.name
    ORDER BY p.name
') as result;

SELECT 'Test 3.3 - Find nodes with any contact missing:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NULL OR p.phone IS NULL
    RETURN p.name, p.email, p.phone
    ORDER BY p.name
') as result;

SELECT 'Test 3.4 - Multiple NULL checks:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NULL AND p.phone IS NULL
    RETURN p.name AS no_contact
') as result;

-- =======================================================================
-- SECTION 4: IS NOT NULL predicate
-- =======================================================================
SELECT '=== Section 4: IS NOT NULL ===' as section;

SELECT 'Test 4.1 - Find nodes with email:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NOT NULL
    RETURN p.name, p.email
    ORDER BY p.name
') as result;

SELECT 'Test 4.2 - Find nodes with both contacts:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NOT NULL AND p.phone IS NOT NULL
    RETURN p.name, p.email, p.phone
') as result;

SELECT 'Test 4.3 - Find nodes with nickname:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.nickname IS NOT NULL
    RETURN p.name, p.nickname
    ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 5: Combining COALESCE with conditions
-- =======================================================================
SELECT '=== Section 5: Combined usage ===' as section;

SELECT 'Test 5.1 - COALESCE in WHERE:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE COALESCE(p.nickname, p.name) STARTS WITH "A"
    RETURN p.name, p.nickname
') as result;

SELECT 'Test 5.2 - Filter then COALESCE:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NOT NULL
    RETURN p.name, COALESCE(p.nickname, p.name) AS display_name
    ORDER BY p.name
') as result;

SELECT 'Test 5.3 - COALESCE with CASE:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    RETURN p.name,
           CASE
               WHEN p.email IS NOT NULL AND p.phone IS NOT NULL THEN "full contact"
               WHEN p.email IS NOT NULL OR p.phone IS NOT NULL THEN "partial contact"
               ELSE "no contact"
           END AS contact_status,
           COALESCE(p.email, p.phone, "N/A") AS primary_contact
    ORDER BY p.name
') as result;

-- =======================================================================
-- SECTION 6: Edge cases
-- =======================================================================
SELECT '=== Section 6: Edge cases ===' as section;

SELECT 'Test 6.1 - COALESCE with fallback:' as test_name;
SELECT cypher('
    MATCH (p:Person {name: "Alice"})
    RETURN COALESCE(p.email, "no email") AS result
') as result;

SELECT 'Test 6.2 - Empty string vs NULL:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email IS NOT NULL
    RETURN p.name, p.email
    ORDER BY p.name
') as result;

SELECT '=== Test 25 Complete ===' as test_section;
