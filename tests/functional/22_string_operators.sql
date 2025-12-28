-- ========================================================================
-- Test 22: String Operators in WHERE Clauses
-- ========================================================================
-- PURPOSE: Test string matching operators STARTS WITH, ENDS WITH, CONTAINS
-- COVERS:  All string operators, case sensitivity, edge cases, combinations
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 22: String Operators ===' as test_section;

-- =======================================================================
-- SETUP: Create test data with varied string patterns
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice Anderson", email: "alice@example.com"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob Brown", email: "bob@example.org"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol Chen", email: "carol.chen@example.com"})') as setup;
SELECT cypher('CREATE (d:Person {name: "David Davis", email: "david@test.com"})') as setup;
SELECT cypher('CREATE (e:Person {name: "alice smith", email: "asmith@EXAMPLE.COM"})') as setup;

SELECT cypher('CREATE (f1:File {name: "report.pdf", path: "/docs/reports/report.pdf"})') as setup;
SELECT cypher('CREATE (f2:File {name: "data.csv", path: "/data/exports/data.csv"})') as setup;
SELECT cypher('CREATE (f3:File {name: "backup.tar.gz", path: "/backups/backup.tar.gz"})') as setup;

-- =======================================================================
-- SECTION 1: STARTS WITH operator
-- =======================================================================
SELECT '=== Section 1: STARTS WITH ===' as section;

SELECT 'Test 1.1 - Basic STARTS WITH match:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH "Alice" RETURN p.name') as result;

SELECT 'Test 1.2 - STARTS WITH single character:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH "A" RETURN p.name ORDER BY p.name') as result;

SELECT 'Test 1.3 - STARTS WITH no match:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH "Zoe" RETURN p.name') as result;

SELECT 'Test 1.4 - STARTS WITH case sensitivity:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH "alice" RETURN p.name') as result;
-- Should only match "alice smith" not "Alice Anderson"

SELECT 'Test 1.5 - STARTS WITH on email domain:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email STARTS WITH "alice" RETURN p.name, p.email') as result;

SELECT 'Test 1.6 - STARTS WITH with path:' as test_name;
SELECT cypher('MATCH (f:File) WHERE f.path STARTS WITH "/docs" RETURN f.name') as result;

-- =======================================================================
-- SECTION 2: ENDS WITH operator
-- =======================================================================
SELECT '=== Section 2: ENDS WITH ===' as section;

SELECT 'Test 2.1 - Basic ENDS WITH match:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email ENDS WITH ".com" RETURN p.name, p.email ORDER BY p.name') as result;

SELECT 'Test 2.2 - ENDS WITH .org:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email ENDS WITH ".org" RETURN p.name, p.email') as result;

SELECT 'Test 2.3 - ENDS WITH no match:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email ENDS WITH ".net" RETURN p.name') as result;

SELECT 'Test 2.4 - ENDS WITH on name:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name ENDS WITH "son" RETURN p.name') as result;

SELECT 'Test 2.5 - ENDS WITH file extensions:' as test_name;
SELECT cypher('MATCH (f:File) WHERE f.name ENDS WITH ".pdf" RETURN f.name') as result;

SELECT 'Test 2.6 - ENDS WITH compound extension:' as test_name;
SELECT cypher('MATCH (f:File) WHERE f.name ENDS WITH ".tar.gz" RETURN f.name') as result;

SELECT 'Test 2.7 - ENDS WITH case sensitivity:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email ENDS WITH ".COM" RETURN p.name, p.email') as result;

-- =======================================================================
-- SECTION 3: CONTAINS operator
-- =======================================================================
SELECT '=== Section 3: CONTAINS ===' as section;

SELECT 'Test 3.1 - Basic CONTAINS match:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS "Brown" RETURN p.name') as result;

SELECT 'Test 3.2 - CONTAINS substring:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email CONTAINS "example" RETURN p.name, p.email ORDER BY p.name') as result;

SELECT 'Test 3.3 - CONTAINS no match:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS "xyz" RETURN p.name') as result;

SELECT 'Test 3.4 - CONTAINS single character:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email CONTAINS "@" RETURN p.name') as result;

SELECT 'Test 3.5 - CONTAINS at start (like STARTS WITH):' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS "Alice" RETURN p.name') as result;

SELECT 'Test 3.6 - CONTAINS at end (like ENDS WITH):' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS "Chen" RETURN p.name') as result;

SELECT 'Test 3.7 - CONTAINS in path:' as test_name;
SELECT cypher('MATCH (f:File) WHERE f.path CONTAINS "exports" RETURN f.name, f.path') as result;

-- =======================================================================
-- SECTION 4: Combining string operators
-- =======================================================================
SELECT '=== Section 4: Combined operators ===' as section;

SELECT 'Test 4.1 - STARTS WITH AND ENDS WITH:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.email STARTS WITH "alice" AND p.email ENDS WITH ".com"
    RETURN p.name, p.email
') as result;

SELECT 'Test 4.2 - STARTS WITH OR STARTS WITH:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE p.name STARTS WITH "A" OR p.name STARTS WITH "B"
    RETURN p.name ORDER BY p.name
') as result;

SELECT 'Test 4.3 - CONTAINS AND comparison:' as test_name;
SELECT cypher('
    MATCH (f:File)
    WHERE f.path CONTAINS "data" AND f.name ENDS WITH ".csv"
    RETURN f.name, f.path
') as result;

SELECT 'Test 4.4 - NOT STARTS WITH:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE NOT p.name STARTS WITH "A"
    RETURN p.name ORDER BY p.name
') as result;

SELECT 'Test 4.5 - NOT CONTAINS:' as test_name;
SELECT cypher('
    MATCH (p:Person)
    WHERE NOT p.email CONTAINS "example"
    RETURN p.name, p.email
') as result;

-- =======================================================================
-- SECTION 5: Edge cases
-- =======================================================================
SELECT '=== Section 5: Edge cases ===' as section;

SELECT 'Test 5.1 - Empty string STARTS WITH:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH "" RETURN p.name LIMIT 3') as result;

SELECT 'Test 5.2 - Empty string CONTAINS:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS "" RETURN p.name LIMIT 3') as result;

SELECT 'Test 5.3 - Full string match with STARTS WITH:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH "Bob Brown" RETURN p.name') as result;

SELECT 'Test 5.4 - Space in search string:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS " " RETURN p.name ORDER BY p.name') as result;

SELECT 'Test 5.5 - Special character dot:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.email CONTAINS "." RETURN p.name, p.email LIMIT 3') as result;

SELECT '=== Test 22 Complete ===' as test_section;
