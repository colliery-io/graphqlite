-- ========================================================================
-- Test 30: Built-in Functions (String, Math, List, Type)
-- ========================================================================
-- PURPOSE: Comprehensive test of built-in Cypher functions
-- COVERS:  String functions, math functions, list functions, type conversion
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 30: Built-in Functions ===' as test_section;

-- =======================================================================
-- SETUP: Create test data
-- =======================================================================
SELECT '=== Setup: Creating test data ===' as section;

SELECT cypher('CREATE (a:Person {name: "Alice Smith", age: 30, score: 85.5})') as setup;
SELECT cypher('CREATE (b:Person {name: "bob jones", age: 25, score: 92.3})') as setup;
SELECT cypher('CREATE (c:Person {name: "  Carol  ", age: 35, score: -15.7})') as setup;

-- =======================================================================
-- SECTION 1: String Functions
-- =======================================================================
SELECT '=== Section 1: String Functions ===' as section;

-- Case conversion
SELECT 'Test 1.1 - toUpper():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, toUpper(p.name) AS upper ORDER BY p.name') as result;

SELECT 'Test 1.2 - toLower():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, toLower(p.name) AS lower ORDER BY p.name') as result;

-- Trimming
SELECT 'Test 1.3 - trim():' as test_name;
SELECT cypher('MATCH (p:Person {name: "  Carol  "}) RETURN trim(p.name) AS trimmed') as result;

SELECT 'Test 1.4 - ltrim():' as test_name;
SELECT cypher('MATCH (p:Person {name: "  Carol  "}) RETURN ltrim(p.name) AS left_trimmed') as result;

SELECT 'Test 1.5 - rtrim():' as test_name;
SELECT cypher('MATCH (p:Person {name: "  Carol  "}) RETURN rtrim(p.name) AS right_trimmed') as result;

-- Substring operations
SELECT 'Test 1.6 - substring() from start:' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN substring(p.name, 0, 5) AS first_name') as result;

SELECT 'Test 1.7 - substring() from middle:' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN substring(p.name, 6) AS last_name') as result;

SELECT 'Test 1.8 - left():' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN left(p.name, 5) AS first_five') as result;

SELECT 'Test 1.9 - right():' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN right(p.name, 5) AS last_five') as result;

-- String manipulation
SELECT 'Test 1.10 - replace():' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN replace(p.name, "Smith", "Jones") AS replaced') as result;

SELECT 'Test 1.11 - reverse():' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN reverse(p.name) AS reversed') as result;

-- String info
SELECT 'Test 1.12 - size() on string:' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN size(p.name) AS length') as result;

-- =======================================================================
-- SECTION 2: Math Functions
-- =======================================================================
SELECT '=== Section 2: Math Functions ===' as section;

SELECT 'Test 2.1 - abs():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.score, abs(p.score) AS abs_score ORDER BY p.name') as result;

SELECT 'Test 2.2 - ceil():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.score, ceil(p.score) AS ceiling ORDER BY p.name') as result;

SELECT 'Test 2.3 - floor():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.score, floor(p.score) AS floored ORDER BY p.name') as result;

SELECT 'Test 2.4 - round():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.score, round(p.score) AS rounded ORDER BY p.name') as result;

SELECT 'Test 2.5 - sqrt():' as test_name;
SELECT cypher('RETURN sqrt(16) AS sqrt_16, sqrt(2) AS sqrt_2') as result;

SELECT 'Test 2.6 - sign():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.score, sign(p.score) AS sign_val ORDER BY p.name') as result;

SELECT 'Test 2.7 - Arithmetic in expressions:' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, p.age * 2 AS doubled_age, p.age + 10 AS plus_ten ORDER BY p.name') as result;

-- =======================================================================
-- SECTION 3: List Functions
-- =======================================================================
SELECT '=== Section 3: List Functions ===' as section;

SELECT 'Test 3.1 - size() on list:' as test_name;
SELECT cypher('RETURN size([1, 2, 3, 4, 5]) AS list_size') as result;

SELECT 'Test 3.2 - head():' as test_name;
SELECT cypher('RETURN head([1, 2, 3, 4, 5]) AS first') as result;

SELECT 'Test 3.3 - tail():' as test_name;
SELECT cypher('RETURN tail([1, 2, 3, 4, 5]) AS rest') as result;

SELECT 'Test 3.4 - last():' as test_name;
SELECT cypher('RETURN last([1, 2, 3, 4, 5]) AS last_elem') as result;

SELECT 'Test 3.5 - range():' as test_name;
SELECT cypher('RETURN range(1, 5) AS one_to_five') as result;

SELECT 'Test 3.6 - range() with step:' as test_name;
SELECT cypher('RETURN range(0, 10, 2) AS evens') as result;

SELECT 'Test 3.7 - List indexing:' as test_name;
SELECT cypher('RETURN [10, 20, 30, 40, 50][0] AS first, [10, 20, 30, 40, 50][2] AS third') as result;

SELECT 'Test 3.8 - List indexing with strings:' as test_name;
SELECT cypher('RETURN ["a", "b", "c"][1] AS middle') as result;

SELECT 'Test 3.9 - Negative indexing (last element):' as test_name;
SELECT cypher('RETURN [10, 20, 30, 40, 50][-1] AS last') as result;

SELECT 'Test 3.10 - Negative indexing (second to last):' as test_name;
SELECT cypher('RETURN [10, 20, 30, 40, 50][-2] AS second_last') as result;

SELECT 'Test 3.11 - Negative indexing equals positive:' as test_name;
SELECT cypher('RETURN [10, 20, 30][-3] AS first_neg, [10, 20, 30][0] AS first_pos') as result;

-- =======================================================================
-- SECTION 4: Type Conversion Functions
-- =======================================================================
SELECT '=== Section 4: Type Conversion ===' as section;

SELECT 'Test 4.1 - toString() from integer:' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, toString(p.age) AS age_str ORDER BY p.name') as result;

SELECT 'Test 4.2 - toString() from float:' as test_name;
SELECT cypher('MATCH (p:Person) RETURN p.name, toString(p.score) AS score_str ORDER BY p.name') as result;

SELECT 'Test 4.3 - toInteger() from string:' as test_name;
SELECT cypher('RETURN toInteger("42") AS int_val') as result;

SELECT 'Test 4.4 - toInteger() from float:' as test_name;
SELECT cypher('RETURN toInteger(3.7) AS truncated') as result;

SELECT 'Test 4.5 - toFloat() from string:' as test_name;
SELECT cypher('RETURN toFloat("3.14") AS float_val') as result;

SELECT 'Test 4.6 - toFloat() from integer:' as test_name;
SELECT cypher('RETURN toFloat(42) AS float_val') as result;

SELECT 'Test 4.7 - toBoolean():' as test_name;
SELECT cypher('RETURN toBoolean("true") AS t, toBoolean("false") AS f') as result;

-- =======================================================================
-- SECTION 5: Aggregation Functions
-- =======================================================================
SELECT '=== Section 5: Aggregation Functions ===' as section;

SELECT 'Test 5.1 - count():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN count(p) AS total') as result;

SELECT 'Test 5.2 - sum():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN sum(p.age) AS total_age') as result;

SELECT 'Test 5.3 - avg():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN avg(p.age) AS avg_age') as result;

SELECT 'Test 5.4 - min():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN min(p.age) AS youngest') as result;

SELECT 'Test 5.5 - max():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN max(p.age) AS oldest') as result;

SELECT 'Test 5.6 - collect():' as test_name;
SELECT cypher('MATCH (p:Person) RETURN collect(p.name) AS all_names') as result;

-- =======================================================================
-- SECTION 6: Combined function usage
-- =======================================================================
SELECT '=== Section 6: Combined usage ===' as section;

SELECT 'Test 6.1 - Nested functions:' as test_name;
SELECT cypher('MATCH (p:Person) RETURN toUpper(trim(p.name)) AS cleaned ORDER BY p.name') as result;

SELECT 'Test 6.2 - Math with aggregation:' as test_name;
SELECT cypher('MATCH (p:Person) RETURN round(avg(p.score)) AS avg_score') as result;

SELECT 'Test 6.3 - String concat with +' as test_name;
SELECT cypher('MATCH (p:Person {name: "Alice Smith"}) RETURN p.name + " (age: " + toString(p.age) + ")" AS display') as result;

SELECT '=== Test 30 Complete ===' as test_section;
