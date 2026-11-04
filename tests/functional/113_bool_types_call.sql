-- ========================================================================
-- Test 113: booleans and strings through comprehensions and CALL { }
--           (GQLITE-T-0482, GQLITE-T-0487)
-- ========================================================================
-- PURPOSE: a boolean property in a comprehension value became "true", and
--          CALL { } outputs kept only their SQLite storage type (a boolean
--          came back as "true" or 1, a string that looks like JSON as a
--          list). CALL over no outer rows failed with "x is not defined",
--          and CALL { WITH * } imported no variable.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 113: booleans and CALL outputs ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A113 {b: true, v: 1})-[:R]->(:B113 {b: false})') as setup;

INSERT INTO _assert SELECT 'booleans in comprehension values',
    json(cypher('MATCH (n:A113) RETURN [x IN [1] | n.b] AS a, [(n)-->(m) | m.b] AS b'))
    = json('[{"a":[true],"b":[false]}]');
INSERT INTO _assert SELECT 'CALL outputs keep their types',
    json(cypher('MATCH (n:A113) CALL { WITH n RETURN n.b AS x, n.v > 0 AS z, ''[1]'' AS s } RETURN x, z, s'))
    = json('[{"x":true,"z":true,"s":"[1]"}]');
INSERT INTO _assert SELECT 'RETURN after CALL keeps types',
    json(cypher('MATCH (n:A113) CALL { WITH n RETURN n.v AS v } RETURN n.b AS k, n.v > 0 AS g'))
    = json('[{"k":true,"g":true}]');
INSERT INTO _assert SELECT 'CALL over no rows gives no rows',
    json(cypher('MATCH (n:Missing113) CALL { WITH n RETURN n.v AS x } RETURN x')) = json('[]');
INSERT INTO _assert SELECT 'CALL { WITH * } imports every variable',
    json(cypher('MATCH (a:A113)-->(b) CALL { WITH * RETURN a.v AS x } RETURN x'))
    = json('[{"x":1}]');
INSERT INTO _assert SELECT 'reduce() with a string function step gives a string',
    json(cypher('MATCH (a:A113) RETURN a.v AS k, reduce(acc = null, z IN [1] | toString(a.v)) AS x, reduce(acc = null, z IN [1] | toString(a.b)) AS y'))
    = json('[{"k":1,"x":"1","y":"true"}]');

SELECT 'Test 113 passed: ' || count(*) || ' assertions' AS result FROM _assert;
