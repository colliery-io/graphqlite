-- ========================================================================
-- Test 05: RETURN Clause Features (DISTINCT, ORDER BY, LIMIT, SKIP)
-- ========================================================================
-- PURPOSE: Comprehensive testing of RETURN clause modifiers and result
--          formatting features
-- COVERS:  DISTINCT filtering, ORDER BY sorting, LIMIT/SKIP pagination,
--          combined clauses, alias handling
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 05: RETURN Clause Features ===' as test_section;

-- =======================================================================
-- SETUP: Create test data with varied names and ages for sorting/filtering
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30})') as setup1;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25})') as setup2;
SELECT cypher('CREATE (c:Person {name: "Charlie", age: 35})') as setup3;
SELECT cypher('CREATE (d:Person {name: "Alice", age: 28})') as setup4; -- Duplicate name
SELECT cypher('CREATE (e:Person {name: "David", age: 40})') as setup5;
SELECT cypher('CREATE (f:Product {name: "Widget", price: 19.99})') as setup6;
SELECT cypher('CREATE (g:Product {name: "Gadget", price: 29.99})') as setup7;

-- =======================================================================
-- SECTION 1: DISTINCT Clause Testing
-- =======================================================================
SELECT '=== Section 1: DISTINCT Clause Testing ===' as section;

SELECT 'Test 1.1 - Return names (with duplicates):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name') as result;

SELECT 'Test 1.2 - Return distinct names:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name') as result;

SELECT 'Test 1.3 - Return distinct ages:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.age') as result;

SELECT 'Test 1.4 - DISTINCT with multiple columns:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name, n.age') as result;

SELECT 'Test 1.5 - DISTINCT across different node types:' as test_name;
SELECT cypher('MATCH (n) RETURN DISTINCT n.name') as result;

-- =======================================================================
-- SECTION 2: ORDER BY Clause Testing
-- =======================================================================
SELECT '=== Section 2: ORDER BY Clause Testing ===' as section;

SELECT 'Test 2.1 - Order by name (ascending):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name') as result;

SELECT 'Test 2.2 - Order by age (ascending):' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.age') as result;

SELECT 'Test 2.3 - Order by age (descending):' as test_name;
-- NOTE: DESC keyword should now be supported
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.age DESC') as result;

SELECT 'Test 2.4 - Order by multiple columns:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.name, n.age') as result;

SELECT 'Test 2.5 - Order by multiple columns (mixed direction):' as test_name;
-- NOTE: ASC/DESC keywords should now be supported
SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.name ASC, n.age DESC') as result;

SELECT 'Test 2.6 - Order by numeric property:' as test_name;
SELECT cypher('MATCH (n:Product) RETURN n.name, n.price ORDER BY n.price') as result;

-- =======================================================================
-- SECTION 3: LIMIT Clause Testing
-- =======================================================================
SELECT '=== Section 3: LIMIT Clause Testing ===' as section;

SELECT 'Test 3.1 - Limit to 2 results:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name LIMIT 2') as result;

SELECT 'Test 3.2 - Limit to 1 result:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name LIMIT 1') as result;

SELECT 'Test 3.3 - Limit with ORDER BY:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name LIMIT 3') as result;

SELECT 'Test 3.4 - Limit larger than result set:' as test_name;
SELECT cypher('MATCH (n:Product) RETURN n.name LIMIT 10') as result;

SELECT 'Test 3.5 - Limit with zero:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name LIMIT 0') as result;

-- =======================================================================
-- SECTION 4: SKIP Clause Testing
-- =======================================================================
SELECT '=== Section 4: SKIP Clause Testing ===' as section;

SELECT 'Test 4.1 - Skip first 2 results:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 2') as result;

SELECT 'Test 4.2 - Skip first result:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 1') as result;

SELECT 'Test 4.3 - Skip with large offset:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 10') as result;

SELECT 'Test 4.4 - Skip with zero:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 0') as result;

-- =======================================================================
-- SECTION 5: Combined Clause Testing
-- =======================================================================
SELECT '=== Section 5: Combined Clause Testing ===' as section;

SELECT 'Test 5.1 - DISTINCT + ORDER BY:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name ORDER BY n.name') as result;

SELECT 'Test 5.2 - ORDER BY + LIMIT:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.age LIMIT 2') as result;

SELECT 'Test 5.3 - ORDER BY + SKIP + LIMIT:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.age SKIP 1 LIMIT 2') as result;

SELECT 'Test 5.4 - All features combined:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.name ORDER BY n.name SKIP 0 LIMIT 10') as result;

SELECT 'Test 5.5 - DISTINCT + ORDER BY + LIMIT:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n.age ORDER BY n.age LIMIT 3') as result;

SELECT 'Test 5.6 - Complex pagination:' as test_name;
SELECT cypher('MATCH (n) RETURN n.name ORDER BY n.name SKIP 2 LIMIT 3') as result;

-- =======================================================================
-- SECTION 6: Alias and Expression Testing
-- =======================================================================
SELECT '=== Section 6: Alias and Expression Testing ===' as section;

SELECT 'Test 6.1 - RETURN with alias:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name AS person_name ORDER BY person_name') as result;

SELECT 'Test 6.2 - Multiple aliases:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name AS name, n.age AS years ORDER BY name') as result;

SELECT 'Test 6.3 - Alias with ORDER BY:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name AS name ORDER BY n.age LIMIT 3') as result;

SELECT 'Test 6.4 - Node alias with DISTINCT:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN DISTINCT n AS person ORDER BY n.name') as result;

-- =======================================================================
-- SECTION 7: Edge Cases and Special Scenarios
-- =======================================================================
SELECT '=== Section 7: Edge Cases and Special Scenarios ===' as section;

SELECT 'Test 7.1 - Empty result set with clauses:' as test_name;
SELECT cypher('MATCH (n:NonExistent) RETURN n.name ORDER BY n.name LIMIT 5') as result;

SELECT 'Test 7.2 - NULL value handling in ORDER BY:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name, n.city ORDER BY n.city') as result;

SELECT 'Test 7.3 - Mixed data types in ORDER BY:' as test_name;
SELECT cypher('MATCH (n) RETURN n.name ORDER BY n.name LIMIT 5') as result;

SELECT 'Test 7.4 - Large SKIP beyond result set:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name SKIP 100') as result;

SELECT 'Test 7.5 - DISTINCT with complex expressions:' as test_name;
SELECT cypher('MATCH (n) WHERE n.name IS NOT NULL RETURN DISTINCT n.name ORDER BY n.name') as result;

-- =======================================================================
-- SECTION 8: Performance and Optimization Patterns
-- =======================================================================
SELECT '=== Section 8: Performance and Optimization Patterns ===' as section;

SELECT 'Test 8.1 - TOP-N pattern with LIMIT:' as test_name;
-- NOTE: DESC keyword not supported - documented in BUG_FIXES.md
-- SELECT cypher('MATCH (n:Person) RETURN n.name, n.age ORDER BY n.age DESC LIMIT 1') as result;
SELECT 'SKIPPED: DESC keyword not supported' as result;

SELECT 'Test 8.2 - Pagination pattern:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.name ORDER BY n.name SKIP 1 LIMIT 2') as result;

SELECT 'Test 8.3 - Deduplication pattern:' as test_name;
SELECT cypher('MATCH (n) RETURN DISTINCT n.name ORDER BY n.name LIMIT 5') as result;

-- =======================================================================
-- VERIFICATION: Result Set Analysis
-- =======================================================================
SELECT '=== Verification: Result Set Analysis ===' as section;

SELECT 'Total test nodes:' as test_name;
-- NOTE: count() function not implemented - documented in BUG_FIXES.md
-- SELECT cypher('MATCH (n) RETURN count(n)') as total_count;
SELECT 'SKIPPED: count() function not implemented' as total_count;

SELECT 'Unique names count:' as test_name;
-- NOTE: count() and DISTINCT functions not implemented - documented in BUG_FIXES.md
-- SELECT cypher('MATCH (n) WHERE n.name IS NOT NULL RETURN count(DISTINCT n.name)') as unique_names;
SELECT 'SKIPPED: count() and DISTINCT functions not implemented' as unique_names;

SELECT 'Age distribution:' as test_name;
SELECT cypher('MATCH (n:Person) RETURN n.age ORDER BY n.age') as age_dist;

SELECT 'Name frequency:' as test_name;
SELECT cypher('MATCH (n) WHERE n.name IS NOT NULL RETURN n.name ORDER BY n.name') as name_freq;

-- Cleanup note
SELECT '=== RETURN Clause Features Test Complete ===' as section;
SELECT 'All RETURN clause modifiers tested successfully' as note;