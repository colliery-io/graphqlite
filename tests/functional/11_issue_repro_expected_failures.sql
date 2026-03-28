-- ========================================================================
-- Test 11: Issue Reproduction Tests
-- ========================================================================
-- PURPOSE: Reproduce confirmed bugs from GitHub issues.
--          Each test should FAIL against current code and PASS once fixed.
-- COVERS:  Issues #34, #36, #37, #39, #40, #41, #42, #43
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 11: Issue Reproduction ===' as test_section;

-- =======================================================================
-- Issue #34: OPTIONAL MATCH ignores label filter and drops null rows
-- https://github.com/colliery-io/graphqlite/issues/34
-- =======================================================================
SELECT '=== Issue #34: OPTIONAL MATCH label filter ===' as section;

SELECT 'Setup #34:' as test_name;
SELECT cypher('CREATE (a:Person34 {id: "alice"})') as result;
SELECT cypher('CREATE (r:Pet34 {id: "rex"})') as result;
SELECT cypher('MATCH (a:Person34 {id: "alice"}), (r:Pet34 {id: "rex"}) CREATE (a)-[:OWNS]->(r)') as result;

SELECT 'Test #34a - Label filter should exclude Pet when asking for Car:' as test_name;
SELECT '  Expected: alice | NULL (no Car nodes exist)' as expected;
SELECT cypher('MATCH (a:Person34 {id: "alice"}) OPTIONAL MATCH (a)-->(r:Car34) RETURN a.id, r.id') as result;
-- BUG: Returns alice | rex (Pet leaks through label filter)

SELECT 'Test #34b - Null rows preserved with WHERE filter:' as test_name;
SELECT '  Expected: alice | NULL (WHERE filters all matches)' as expected;
SELECT cypher('MATCH (a:Person34 {id: "alice"}) OPTIONAL MATCH (a)-->(r:Pet34) WHERE r.name = "nonexistent" RETURN a.id, r.id') as result;
-- BUG: Returns empty result instead of alice | NULL

-- =======================================================================
-- Issue #36: Variables from MERGE unbound in subsequent WITH/SET
-- https://github.com/colliery-io/graphqlite/issues/36
-- =======================================================================
SELECT '=== Issue #36: MERGE variable persistence through WITH ===' as section;

SELECT 'Setup #36:' as test_name;
SELECT cypher('CREATE (c:Company36 {id: "acme"})') as result;

SELECT 'Test #36 - MERGE var should survive WITH into second MATCH+MERGE:' as test_name;
SELECT '  Expected: Relationship created, no error' as expected;
SELECT cypher('MERGE (i:Employee36 {id: "emp-1"}) ON CREATE SET i.hired = 123 SET i.name = "Alice" WITH i MATCH (c:Company36 {id: "acme"}) MERGE (c)-[:EMPLOYS]->(i)') as result;
-- BUG: "Unbound variable in SET: i"

-- =======================================================================
-- Issue #37: UNWIND does not accept parameter references
-- https://github.com/colliery-io/graphqlite/issues/37
-- =======================================================================
SELECT '=== Issue #37: UNWIND with parameters ===' as section;

SELECT 'Test #37a - UNWIND literal list (control, should work):' as test_name;
SELECT cypher('UNWIND [1, 2, 3] AS x RETURN x') as result;

SELECT 'Test #37b - UNWIND $param (should work but fails):' as test_name;
SELECT '  Expected: Three rows with values 1, 2, 3' as expected;
SELECT cypher('UNWIND $items AS item RETURN item', '{"items": [1, 2, 3]}') as result;
-- BUG: "UNWIND requires list literal, property access, variable, or function call"

-- =======================================================================
-- Issue #39: DELETE + RETURN COUNT always returns 0
-- https://github.com/colliery-io/graphqlite/issues/39
-- =======================================================================
SELECT '=== Issue #39: DELETE + RETURN COUNT ===' as section;

SELECT 'Setup #39:' as test_name;
SELECT cypher('CREATE (n:Temp39 {id: "1"})') as result;
SELECT cypher('CREATE (n:Temp39 {id: "2"})') as result;
SELECT cypher('CREATE (n:Temp39 {id: "3"})') as result;

SELECT 'Test #39a - Count before delete (control):' as test_name;
SELECT cypher('MATCH (n:Temp39) RETURN count(n) AS cnt') as result;
-- Expected: 3

SELECT 'Test #39b - DELETE + RETURN COUNT should report 3:' as test_name;
SELECT '  Expected: deleted_count = 3' as expected;
SELECT cypher('MATCH (n:Temp39) DETACH DELETE n RETURN COUNT(n) AS deleted_count') as result;
-- BUG: Returns 0

-- =======================================================================
-- Issue #40: CALL {} subqueries not supported
-- https://github.com/colliery-io/graphqlite/issues/40
-- =======================================================================
SELECT '=== Issue #40: CALL {} subquery parsing ===' as section;

SELECT 'Setup #40:' as test_name;
SELECT cypher('CREATE (a:Node40 {id: "a"})') as result;

-- NOTE: This will raise a parse error. Using a subselect to trap it.
SELECT 'Test #40 - CALL {} should parse without error:' as test_name;
SELECT '  Expected: Parses and executes' as expected;
SELECT cypher('MATCH (a:Node40 {id: "a"}) CALL { WITH a RETURN a.id AS inner_id } RETURN a.id') as result;
-- BUG: "syntax error, unexpected IDENTIFIER, expecting end of file"

-- =======================================================================
-- Issue #41: startNode() and endNode() return integer ID instead of Node
-- https://github.com/colliery-io/graphqlite/issues/41
-- =======================================================================
SELECT '=== Issue #41: startNode()/endNode() return type ===' as section;

SELECT 'Setup #41:' as test_name;
SELECT cypher('CREATE (a:Person41 {id: "alice", name: "Alice"})') as result;
SELECT cypher('CREATE (b:Person41 {id: "bob", name: "Bob"})') as result;
SELECT cypher('MATCH (a:Person41 {id: "alice"}), (b:Person41 {id: "bob"}) CREATE (a)-[:KNOWS]->(b)') as result;

SELECT 'Test #41a - startNode(r).name should return source node name:' as test_name;
SELECT '  Expected: Alice' as expected;
SELECT cypher('MATCH ()-[r:KNOWS]->() RETURN startNode(r).name AS sname') as result;
-- BUG: "Failed to transform RETURN clause"

SELECT 'Test #41b - endNode(r).name should return target node name:' as test_name;
SELECT '  Expected: Bob' as expected;
SELECT cypher('MATCH ()-[r:KNOWS]->() RETURN endNode(r).name AS ename') as result;
-- BUG: "Failed to transform RETURN clause"

-- =======================================================================
-- Issue #42: size(labels(n)) returns string length instead of list length
-- https://github.com/colliery-io/graphqlite/issues/42
-- =======================================================================
SELECT '=== Issue #42: size(labels(n)) ===' as section;

SELECT 'Setup #42:' as test_name;
SELECT cypher('CREATE (a:LabelA42 {id: "a"})') as result;
SELECT cypher('CREATE (b:LabelA42:LabelB42 {id: "b"})') as result;

SELECT 'Test #42a - size(labels(n)) with 1 label should be 1:' as test_name;
SELECT '  Expected: 1' as expected;
SELECT cypher('MATCH (n:LabelA42 {id: "a"}) RETURN size(labels(n)) AS sz') as result;
-- BUG: Returns 10 (string length of '["LabelA42"]')

SELECT 'Test #42b - size(labels(n)) with 2 labels should be 2:' as test_name;
SELECT '  Expected: 2' as expected;
SELECT cypher('MATCH (n {id: "b"}) RETURN size(labels(n)) AS sz') as result;
-- BUG: Returns ~22 (string length of '["LabelA42","LabelB42"]')

SELECT 'Test #42c - size() on literal list (control, should work):' as test_name;
SELECT cypher('RETURN size([1, 2, 3]) AS sz') as result;
-- Expected: 3 (this works correctly)

-- =======================================================================
-- Issue #43: Integer property storage truncates values to 32-bit
-- https://github.com/colliery-io/graphqlite/issues/43
-- =======================================================================
SELECT '=== Issue #43: 64-bit integer storage ===' as section;

SELECT 'Setup #43:' as test_name;
SELECT cypher('CREATE (n:IntTest43 {id: "big"})') as result;

SELECT 'Test #43a - Large integer (9999999999) should round-trip:' as test_name;
SELECT '  Expected: 9999999999' as expected;
SELECT cypher('MATCH (n:IntTest43 {id: "big"}) SET n.val = 9999999999') as result;
SELECT cypher('MATCH (n:IntTest43 {id: "big"}) RETURN n.val') as result;
-- BUG: Returns 1410065407 (truncated to 32-bit)

SELECT 'Test #43b - timestamp() should not be truncated:' as test_name;
SELECT '  Expected: Value > 2147483647 (current epoch millis)' as expected;
SELECT cypher('MATCH (n:IntTest43 {id: "big"}) SET n.ts = timestamp()') as result;
SELECT cypher('MATCH (n:IntTest43 {id: "big"}) RETURN n.ts') as result;
-- BUG: Returns truncated value

SELECT '=== Issue Reproduction Tests Complete ===' as test_section;
