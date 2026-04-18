-- ========================================================================
-- Test 39: Issue Regression Tests (Previously Expected Failures)
-- ========================================================================
-- PURPOSE: Tests for issues that were previously failing and are now fixed.
--          These were promoted from 11_issue_repro_expected_failures.sql
--          after the underlying bugs were resolved.
-- COVERS:  Issues #34a, #36, #37, #39, #40, #41, #42, #43, #49, #50, #51
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 39: Issue Regression Tests ===' as test_section;

-- =======================================================================
-- Issue #34a: OPTIONAL MATCH label filter (fixed)
-- https://github.com/colliery-io/graphqlite/issues/34
-- =======================================================================
SELECT '=== Issue #34a: OPTIONAL MATCH label filter ===' as section;

SELECT cypher('CREATE (a:Person34r {id: "alice"})') as result;
SELECT cypher('CREATE (r:Pet34r {id: "rex"})') as result;
SELECT cypher('MATCH (a:Person34r {id: "alice"}), (r:Pet34r {id: "rex"}) CREATE (a)-[:OWNS34r]->(r)') as result;

SELECT 'Test #34a - Label filter should exclude Pet when asking for Car:' as test_name;
SELECT '  Expected: alice | NULL (no Car nodes exist)' as expected;
SELECT cypher('MATCH (a:Person34r {id: "alice"}) OPTIONAL MATCH (a)-[:OWNS34r]->(r:Car34r) RETURN a.id, r.id') as result;
-- Was BUG: Returns alice | rex (Pet leaks through label filter)
-- Now fixed: Returns alice | null

SELECT 'Test #34b - WHERE filter preserves null row:' as test_name;
SELECT '  Expected: alice | NULL (WHERE filters all matches but row preserved)' as expected;
SELECT cypher('MATCH (a:Person34r {id: "alice"}) OPTIONAL MATCH (a)-[:OWNS34r]->(r:Pet34r) WHERE r.name = "nonexistent" RETURN a.id, r.id') as result;
-- Was BUG: Returns empty result instead of alice | NULL

-- =======================================================================
-- Issue #36: Variables from MERGE unbound in subsequent WITH/SET (fixed)
-- https://github.com/colliery-io/graphqlite/issues/36
-- =======================================================================
SELECT '=== Issue #36: MERGE variable persistence through WITH ===' as section;

SELECT cypher('CREATE (c:Company36r {id: "acme"})') as result;

SELECT 'Test #36 - MERGE var should survive WITH into second MATCH+MERGE:' as test_name;
SELECT cypher('MERGE (i:Employee36r {id: "emp-1"}) ON CREATE SET i.hired = 123 SET i.name = "Alice" WITH i MATCH (c:Company36r {id: "acme"}) MERGE (c)-[:EMPLOYS36r]->(i)') as result;
-- Was BUG: "Unbound variable in SET: i"

SELECT 'Verify relationship created:' as test_name;
SELECT cypher('MATCH (c:Company36r)-[:EMPLOYS36r]->(i:Employee36r) RETURN c.id, i.id, i.name') as result;

-- =======================================================================
-- Issue #37: UNWIND does not accept parameter references (fixed)
-- https://github.com/colliery-io/graphqlite/issues/37
-- =======================================================================
SELECT '=== Issue #37: UNWIND with parameters ===' as section;

SELECT 'Test #37a - UNWIND literal list (control):' as test_name;
SELECT cypher('UNWIND [1, 2, 3] AS x RETURN x') as result;

SELECT 'Test #37b - UNWIND $param:' as test_name;
SELECT cypher('UNWIND $items AS item RETURN item', '{"items": [1, 2, 3]}') as result;
-- Was BUG: "UNWIND requires list literal, property access, variable, or function call"

-- =======================================================================
-- Issue #39: DELETE + RETURN COUNT always returns 0 (fixed)
-- https://github.com/colliery-io/graphqlite/issues/39
-- =======================================================================
SELECT '=== Issue #39: DELETE + RETURN COUNT ===' as section;

SELECT cypher('CREATE (n:Temp39r {id: "1"})') as result;
SELECT cypher('CREATE (n:Temp39r {id: "2"})') as result;
SELECT cypher('CREATE (n:Temp39r {id: "3"})') as result;

SELECT 'Test #39a - Count before delete:' as test_name;
SELECT cypher('MATCH (n:Temp39r) RETURN count(n) AS cnt') as result;

SELECT 'Test #39b - DELETE + RETURN COUNT should report 3:' as test_name;
SELECT cypher('MATCH (n:Temp39r) DETACH DELETE n RETURN COUNT(n) AS deleted_count') as result;
-- Was BUG: Returns 0

-- =======================================================================
-- Issue #40: CALL {} subqueries not supported (fixed)
-- https://github.com/colliery-io/graphqlite/issues/40
-- =======================================================================
SELECT '=== Issue #40: CALL {} subquery ===' as section;

SELECT cypher('CREATE (a:Node40r {id: "a"})') as result;

SELECT 'Test #40 - CALL {} should parse and execute:' as test_name;
SELECT cypher('MATCH (a:Node40r {id: "a"}) CALL { WITH a RETURN a.id AS inner_id } RETURN a.id') as result;
-- Was BUG: "syntax error, unexpected IDENTIFIER, expecting end of file"

-- =======================================================================
-- Issue #41: startNode() and endNode() return type (fixed)
-- https://github.com/colliery-io/graphqlite/issues/41
-- =======================================================================
SELECT '=== Issue #41: startNode()/endNode() property access ===' as section;

SELECT cypher('CREATE (a:Person41r {id: "alice", name: "Alice"})') as result;
SELECT cypher('CREATE (b:Person41r {id: "bob", name: "Bob"})') as result;
SELECT cypher('MATCH (a:Person41r {id: "alice"}), (b:Person41r {id: "bob"}) CREATE (a)-[:KNOWS41r]->(b)') as result;

SELECT 'Test #41a - startNode(r).name:' as test_name;
SELECT cypher('MATCH ()-[r:KNOWS41r]->() RETURN startNode(r).name AS sname') as result;
-- Was BUG: "Failed to transform RETURN clause"

SELECT 'Test #41b - endNode(r).name:' as test_name;
SELECT cypher('MATCH ()-[r:KNOWS41r]->() RETURN endNode(r).name AS ename') as result;

-- =======================================================================
-- Issue #42: size(labels(n)) returns string length instead of list length (fixed)
-- https://github.com/colliery-io/graphqlite/issues/42
-- =======================================================================
SELECT '=== Issue #42: size(labels(n)) ===' as section;

SELECT cypher('CREATE (a:LabelA42r {id: "a"})') as result;
SELECT cypher('CREATE (b:LabelA42r:LabelB42r {id: "b"})') as result;

SELECT 'Test #42a - size(labels(n)) with 1 label should be 1:' as test_name;
SELECT cypher('MATCH (n:LabelA42r {id: "a"}) RETURN size(labels(n)) AS sz') as result;
-- Was BUG: Returns 12 (string length)

SELECT 'Test #42b - size(labels(n)) with 2 labels should be 2:' as test_name;
SELECT cypher('MATCH (n {id: "b"}) RETURN size(labels(n)) AS sz') as result;

-- =======================================================================
-- Issue #43: Integer property storage truncates to 32-bit (fixed)
-- https://github.com/colliery-io/graphqlite/issues/43
-- =======================================================================
SELECT '=== Issue #43: 64-bit integer storage ===' as section;

SELECT cypher('CREATE (n:IntTest43r {id: "big"})') as result;

SELECT 'Test #43a - Large integer round-trip:' as test_name;
SELECT cypher('MATCH (n:IntTest43r {id: "big"}) SET n.val = 9999999999') as result;
SELECT cypher('MATCH (n:IntTest43r {id: "big"}) RETURN n.val') as result;
-- Was BUG: Returns 1410065407 (truncated)

SELECT 'Test #43b - timestamp() not truncated:' as test_name;
SELECT cypher('MATCH (n:IntTest43r {id: "big"}) SET n.ts = timestamp()') as result;
SELECT cypher('MATCH (n:IntTest43r {id: "big"}) RETURN n.ts > 2147483647 AS is_64bit') as result;

-- =======================================================================
-- Issue #49: UNWIND $param write paths (fixed in this PR)
-- https://github.com/colliery-io/graphqlite/issues/49
-- =======================================================================
SELECT '=== Issue #49: UNWIND $param write paths ===' as section;

SELECT 'Test #49a - UNWIND $param + CREATE + SET:' as test_name;
SELECT cypher('UNWIND $items AS item CREATE (n:Node49r) SET n.id = item.id, n.name = item.name', '{"items": [{"id": "a", "name": "Alpha"}, {"id": "b", "name": "Beta"}]}') as result;
SELECT cypher('MATCH (n:Node49r) RETURN n.id, n.name ORDER BY n.id') as result;

SELECT 'Test #49b - UNWIND $param + MERGE:' as test_name;
SELECT cypher('UNWIND $items AS item MERGE (n:Node49mr {id: item.id})', '{"items": [{"id": "x"}, {"id": "y"}]}') as result;
SELECT cypher('MATCH (n:Node49mr) RETURN n.id ORDER BY n.id') as result;

SELECT 'Test #49c - UNWIND literal + SET:' as test_name;
SELECT cypher('UNWIND ["a", "b"] AS item CREATE (n:Node49sr) SET n.id = item') as result;
SELECT cypher('MATCH (n:Node49sr) RETURN n.id ORDER BY n.id') as result;

-- =======================================================================
-- Issue #50: startNode(r)/endNode(r) alias collision (fixed in this PR)
-- https://github.com/colliery-io/graphqlite/issues/50
-- =======================================================================
SELECT '=== Issue #50: startNode/endNode alias collision ===' as section;

SELECT cypher('CREATE (a:Person50r {name: "Alice"})-[:KNOWS50r]->(b:Person50r {name: "Bob"})') as result;

SELECT 'Test #50 - Both in same RETURN with distinct columns:' as test_name;
SELECT cypher('MATCH ()-[r:KNOWS50r]->() RETURN startNode(r).name, endNode(r).name') as result;
-- Was BUG: [{"name":"Alice","name":"Bob"}] (duplicate key)

-- =======================================================================
-- Issue #51: CALL subquery MERGE scoping (fixed in this PR)
-- https://github.com/colliery-io/graphqlite/issues/51
-- =======================================================================
SELECT '=== Issue #51: CALL subquery MERGE scoping ===' as section;

SELECT cypher('CREATE (c:Co51r {id: "acme"})') as result;
SELECT cypher('CREATE (d:Dep51r {id: "eng"})') as result;

SELECT 'Test #51 - CALL MERGE links to correct target:' as test_name;
SELECT cypher('MATCH (c:Co51r {id: "acme"}) CALL { With c MATCH (d:Dep51r {id: "eng"}) MERGE (c)-[:HAS51r]->(d) }') as result;
SELECT cypher('MATCH (a)-[:HAS51r]->(b) RETURN a.id, labels(a) AS al, b.id, labels(b) AS bl') as result;
-- Was BUG: b.id=acme (self-loop) instead of b.id=eng

-- =======================================================================
-- Issue #61.2: $param values in CREATE relationship inline properties
-- https://github.com/colliery-io/graphqlite/issues/61  (GQLITE-T-0186)
-- =======================================================================
SELECT '=== Issue #61.2: $param in rel inline props ===' as section;

SELECT cypher('CREATE (a:N61r2 {node_id:"a"})') as result;
SELECT cypher('CREATE (b:N61r2 {node_id:"b"})') as result;

SELECT 'Test #61.2a - CREATE rel with $param text/int properties:' as test_name;
SELECT cypher('MATCH (a:N61r2 {node_id:"a"}) MATCH (b:N61r2 {node_id:"b"})
               CREATE (a)-[:REL61r2 {prop1: $p1, prop2: $p2}]->(b)',
              '{"p1":"world","p2":99}') as result;
SELECT cypher('MATCH ()-[r:REL61r2]->() RETURN r.prop1, r.prop2') as result;
-- Expected: [{"r.prop1":"world","r.prop2":99}]   (was: both NULL)

SELECT 'Test #61.2b - CREATE rel with $param real/bool properties:' as test_name;
SELECT cypher('MATCH (a:N61r2 {node_id:"a"}) MATCH (b:N61r2 {node_id:"b"})
               CREATE (a)-[:REL61r2b {weight: $w, active: $act}]->(b)',
              '{"w":3.14,"act":true}') as result;
SELECT cypher('MATCH ()-[r:REL61r2b]->() RETURN r.weight, r.active') as result;

-- =======================================================================
-- Issue #61.3: ON CREATE SET with $param on relationship variable
-- https://github.com/colliery-io/graphqlite/issues/61  (GQLITE-T-0187)
-- =======================================================================
SELECT '=== Issue #61.3: ON CREATE SET $param on rel var ===' as section;

SELECT cypher('CREATE (a:N61r3 {node_id:"a"})') as result;
SELECT cypher('CREATE (b:N61r3 {node_id:"b"})') as result;

SELECT 'Test #61.3 - MERGE rel with inline $param + ON CREATE SET $param:' as test_name;
SELECT cypher('MATCH (a:N61r3 {node_id:"a"}) MATCH (b:N61r3 {node_id:"b"})
               MERGE (a)-[r:CALLS61r3 {edge_id: $eid}]->(b)
               ON CREATE SET r.file = $file, r.line = $line',
              '{"eid":"e1","file":"test.py","line":42}') as result;
SELECT cypher('MATCH ()-[r:CALLS61r3]->() RETURN r.edge_id, r.file, r.line') as result;
-- Expected: [{"r.edge_id":"e1","r.file":"test.py","r.line":42}]  (was: all NULL)

SELECT '=== Issue Regression Tests Complete ===' as test_section;
