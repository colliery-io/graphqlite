-- ========================================================================
-- Test 114: UNWIND of a list of node or relationship variables
--           (GQLITE-T-0489)
-- ========================================================================
-- PURPOSE: in `UNWIND [n] AS x`, x was a plain value, so x.v read a
--          top-level key of the node's JSON and gave null. A list of node
--          (or of relationship) variables now makes x that kind of entity.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 114: UNWIND of entity variables ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A114 {id: 1, v: 3})-[:R {w: 5}]->(:B114 {id: 2, v: 4})') as setup;

INSERT INTO _assert SELECT 'property of an unwound node',
    json(cypher('MATCH (a:A114)-->(b) UNWIND [a, b] AS x RETURN x.v AS v, labels(x) AS l ORDER BY v'))
    = json('[{"v":3,"l":["A114"]},{"v":4,"l":["B114"]}]');
INSERT INTO _assert SELECT 'property of an unwound relationship',
    json(cypher('MATCH (:A114)-[r]->() UNWIND [r] AS x RETURN x.w AS w, type(x) AS t'))
    = json('[{"w":5,"t":"R"}]');
INSERT INTO _assert SELECT 'MATCH from an unwound node',
    json(cypher('MATCH (a:A114) UNWIND [a] AS x MATCH (x)-->(y) RETURN y.id AS id'))
    = json('[{"id":2}]');

SELECT 'Test 114 passed: ' || count(*) || ' assertions' AS result FROM _assert;
