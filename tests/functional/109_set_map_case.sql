-- ========================================================================
-- Test 109: SET n = map that reads n, and SET values with label tests
--           (GQLITE-T-0479)
-- ========================================================================
-- PURPOSE: `SET n = {id: n.id}` cleared n and then read n.id, and bulk SET
--          dropped any map value that was not a literal: n lost id. A SET
--          value with a label test (`CASE WHEN n:A ...`, `n:A`) was refused
--          ("SET value must be a literal ...").
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 109: SET = map and label tests ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A109 {id: 1, v: 5, s: ''x''}), (:B109 {id: 2, v: 6})') as setup;

SELECT cypher('MATCH (n:A109) SET n = {id: n.id, q: n.v + 1, l: [n.s]}') as w1;
INSERT INTO _assert SELECT 'SET = map reads n before it clears n',
    json(cypher('MATCH (n:A109) RETURN properties(n) AS p'))
    = json('[{"p":{"id":1,"q":6,"l":["x"]}}]');

SELECT cypher('MATCH (n) WHERE n:A109 OR n:B109 SET n.k = CASE WHEN n:A109 THEN ''a'' ELSE ''o'' END, n.t = n:B109') as w2;
INSERT INTO _assert SELECT 'label tests in SET values',
    json(cypher('MATCH (n) WHERE n:A109 OR n:B109 RETURN n.id AS id, n.k AS k, n.t AS t ORDER BY id'))
    = json('[{"id":1,"k":"a","t":false},{"id":2,"k":"o","t":true}]');

SELECT 'Test 109 passed: ' || count(*) || ' assertions' AS result FROM _assert;
