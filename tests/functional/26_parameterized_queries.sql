-- ========================================================================
-- Test 26: Parameterized Queries
-- ========================================================================
-- PURPOSE: Test parameter binding with $name syntax
-- COVERS:  String params, integer params, multiple params, injection safety
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 26: Parameterized Queries ===' as test_section;

-- =======================================================================
-- SETUP: Create test data
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice", age: 30, city: "NYC"})') as setup;
SELECT cypher('CREATE (b:Person {name: "Bob", age: 25, city: "LA"})') as setup;
SELECT cypher('CREATE (c:Person {name: "Carol", age: 35, city: "NYC"})') as setup;
SELECT cypher('CREATE (d:Person {name: "David", age: 28, city: "Chicago"})') as setup;

SELECT cypher('
    MATCH (a:Person {name: "Alice"}), (b:Person {name: "Bob"})
    CREATE (a)-[:KNOWS {since: 2020}]->(b)
') as setup;

-- =======================================================================
-- SECTION 1: String parameters
-- =======================================================================
SELECT '=== Section 1: String parameters ===' as section;

SELECT 'Test 1.1 - Match by name parameter:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name, p.age', '{"name": "Alice"}') as result;

SELECT 'Test 1.2 - Match by city parameter:' as test_name;
SELECT cypher('MATCH (p:Person {city: $city}) RETURN p.name ORDER BY p.name', '{"city": "NYC"}') as result;

SELECT 'Test 1.3 - Parameter with no match:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": "NonExistent"}') as result;

SELECT 'Test 1.4 - Parameter in WHERE clause:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.city = $city RETURN p.name ORDER BY p.name', '{"city": "NYC"}') as result;

-- =======================================================================
-- SECTION 2: Integer parameters
-- =======================================================================
SELECT '=== Section 2: Integer parameters ===' as section;

SELECT 'Test 2.1 - Match by age parameter:' as test_name;
SELECT cypher('MATCH (p:Person {age: $age}) RETURN p.name', '{"age": 30}') as result;

SELECT 'Test 2.2 - Comparison with parameter:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.age > $min_age RETURN p.name, p.age ORDER BY p.age', '{"min_age": 27}') as result;

SELECT 'Test 2.3 - Range comparison:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.age >= $min AND p.age <= $max RETURN p.name, p.age ORDER BY p.age', '{"min": 25, "max": 30}') as result;

-- =======================================================================
-- SECTION 3: Multiple parameters
-- =======================================================================
SELECT '=== Section 3: Multiple parameters ===' as section;

SELECT 'Test 3.1 - Two string parameters:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name = $name OR p.city = $city RETURN p.name, p.city ORDER BY p.name', '{"name": "David", "city": "NYC"}') as result;

SELECT 'Test 3.2 - Mixed type parameters:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.city = $city AND p.age > $min_age RETURN p.name, p.age', '{"city": "NYC", "min_age": 25}') as result;

SELECT 'Test 3.3 - Three parameters:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name = $name OR p.age = $age OR p.city = $city RETURN p.name', '{"name": "Alice", "age": 25, "city": "Chicago"}') as result;

-- =======================================================================
-- SECTION 4: Parameters in CREATE
-- =======================================================================
SELECT '=== Section 4: CREATE with parameters ===' as section;

SELECT 'Test 4.1 - Create node with parameters:' as test_name;
SELECT cypher('CREATE (p:Person {name: $name, age: $age, city: $city}) RETURN p.name', '{"name": "TestUser", "age": 40, "city": "Boston"}') as result;

SELECT 'Test 4.2 - Verify created node:' as test_name;
SELECT cypher('MATCH (p:Person {name: "TestUser"}) RETURN p.name, p.age, p.city') as result;

-- =======================================================================
-- SECTION 5: SET with parameters
-- =======================================================================
SELECT '=== Section 5: SET with parameters ===' as section;

SELECT 'Test 5.1 - SET property with parameter:' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice"}) SET p.status = $status RETURN p.name, p.status', '{"status": "active"}') as result;

SELECT 'Test 5.2 - SET multiple properties with parameters:' as test_name;
SELECT cypher('MATCH (p:Person {name: "Bob"}) SET p.status = $status, p.score = $score RETURN p.name, p.status, p.score', '{"status": "pending", "score": 85}') as result;

SELECT 'Test 5.3 - SET with WHERE parameter:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.city = $city SET p.region = $region RETURN p.name, p.region ORDER BY p.name', '{"city": "NYC", "region": "East"}') as result;

SELECT 'Test 5.4 - Verify SET changes persisted:' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice"}) RETURN p.status') as result;

-- =======================================================================
-- SECTION 6: DELETE with parameters
-- =======================================================================
SELECT '=== Section 6: DELETE with parameters ===' as section;

SELECT 'Test 6.1 - Setup for DELETE test:' as test_name;
SELECT cypher('CREATE (t:Temp {name: $name, value: $value})', '{"name": "ToDelete", "value": 999}') as result;

SELECT 'Test 6.2 - DELETE with WHERE parameter:' as test_name;
SELECT cypher('MATCH (t:Temp) WHERE t.name = $name DELETE t', '{"name": "ToDelete"}') as result;

SELECT 'Test 6.3 - Verify DELETE worked:' as test_name;
SELECT cypher('MATCH (t:Temp {name: "ToDelete"}) RETURN count(t) AS remaining') as result;

-- =======================================================================
-- SECTION 7: Relationship parameters
-- =======================================================================
SELECT '=== Section 7: Relationship parameters ===' as section;

SELECT 'Test 7.1 - Create relationship with parameter properties:' as test_name;
SELECT cypher('
    MATCH (a:Person {name: "Carol"}), (b:Person {name: "David"})
    CREATE (a)-[:WORKS_WITH {since: $year, project: $project}]->(b)
', '{"year": 2023, "project": "Alpha"}') as result;

SELECT 'Test 7.2 - Match relationship by parameter:' as test_name;
SELECT cypher('
    MATCH (a)-[r:WORKS_WITH {project: $project}]->(b)
    RETURN a.name, b.name, r.since
', '{"project": "Alpha"}') as result;

SELECT 'Test 7.3 - Match by relationship type parameter in WHERE:' as test_name;
SELECT cypher('
    MATCH (a:Person)-[r]->(b:Person)
    WHERE r.since = $year
    RETURN a.name, type(r) AS rel_type, b.name
    ORDER BY a.name
', '{"year": 2020}') as result;

-- =======================================================================
-- SECTION 8: SQL injection safety
-- =======================================================================
SELECT '=== Section 8: Injection safety ===' as section;

SELECT 'Test 8.1 - Malicious string parameter:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": "Alice\"); DROP TABLE nodes;--"}') as result;
-- Should return empty, not execute injection

SELECT 'Test 8.2 - Quote in parameter:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": "O''Brien"}') as result;

SELECT 'Test 8.3 - Special characters:' as test_name;
SELECT cypher('CREATE (p:Test {text: $text}) RETURN p.text', '{"text": "Line1\nLine2\tTab"}') as result;

-- =======================================================================
-- SECTION 9: Parameters with string operators
-- =======================================================================
SELECT '=== Section 9: String operator parameters ===' as section;

SELECT 'Test 9.1 - STARTS WITH parameter:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name STARTS WITH $prefix RETURN p.name ORDER BY p.name', '{"prefix": "A"}') as result;

SELECT 'Test 9.2 - CONTAINS parameter:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name CONTAINS $substr RETURN p.name ORDER BY p.name', '{"substr": "o"}') as result;

-- =======================================================================
-- SECTION 10: Edge cases
-- =======================================================================
SELECT '=== Section 10: Edge cases ===' as section;

SELECT 'Test 10.1 - Empty string parameter:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": ""}') as result;

SELECT 'Test 10.2 - Parameter not used (extra param):' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": "Alice", "unused": "value"}') as result;

SELECT 'Test 10.3 - Null parameter:' as test_name;
SELECT cypher('MATCH (p:Person) WHERE p.name = $name RETURN p.name', '{"name": null}') as result;

-- =======================================================================
-- SECTION 11: Generic transform path (OPTIONAL MATCH, WITH, UNWIND)
-- =======================================================================
SELECT '=== Section 11: Generic transform path params ===' as section;

SELECT 'Test 11.1 - Params in OPTIONAL MATCH:' as test_name;
SELECT cypher('OPTIONAL MATCH (p:Person) WHERE p.name = $name RETURN p.name', '{"name": "Alice"}') as result;

SELECT 'Test 11.2 - Params in UNWIND+RETURN:' as test_name;
SELECT cypher('UNWIND [1,2,3] AS x RETURN x, $label', '{"label": "test"}') as result;

-- =======================================================================
-- SECTION 12: List and map parameter values
-- =======================================================================
SELECT '=== Section 12: List and map params ===' as section;

SELECT 'Test 12.1 - Extra map param ignored:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": "Alice", "meta": {"key": "val"}}') as result;

SELECT 'Test 12.2 - Extra list param ignored:' as test_name;
SELECT cypher('MATCH (p:Person {name: $name}) RETURN p.name', '{"name": "Alice", "ids": [1, 2, 3]}') as result;

-- =======================================================================
-- SECTION 13: Boolean parameter
-- =======================================================================
SELECT '=== Section 13: Boolean params ===' as section;

SELECT 'Test 13.1 - Create with boolean param:' as test_name;
SELECT cypher('CREATE (f:Flag {active: $a}) RETURN f.active', '{"a": true}') as result;

SELECT 'Test 13.2 - Create with false param:' as test_name;
SELECT cypher('CREATE (f:Flag {active: $a}) RETURN f.active', '{"a": false}') as result;

-- =======================================================================
-- SECTION 14: Float parameter
-- =======================================================================
SELECT '=== Section 14: Float params ===' as section;

SELECT 'Test 14.1 - Create with float param:' as test_name;
SELECT cypher('CREATE (m:Metric {val: $v}) RETURN m.val', '{"v": 3.14}') as result;

SELECT '=== Test 26 Complete ===' as test_section;
