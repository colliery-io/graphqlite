-- ========================================================================
-- Test 89: a named path through WITH * (GQLITE-T-0471)
-- ========================================================================
-- PURPOSE: WITH * carried each visible variable as `<alias>.id`; a path has
--          no table alias, so the SQL had `.id` on nothing (near ".":
--          syntax error). WITH * now carries a path as `WITH p` does: its
--          path value, re-registered as a path. Found by the metamorphic
--          harness (relation named_path).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 89: named path through WITH * ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A89 {n: 1})-[:R89]->(:B89 {n: 2}), (:A89 {n: 3})') as setup;

INSERT INTO _assert SELECT 'WITH * keeps the path',
    json(cypher('MATCH p = (a:A89)-[:R89]->(b) WITH * RETURN length(p) AS l, a.n AS a, b.n AS b'))
    = json('[{"l":1,"a":1,"b":2}]');
INSERT INTO _assert SELECT 'OPTIONAL path through WITH *',
    json(cypher('MATCH (a:A89) OPTIONAL MATCH p = (a)-[:R89]->() WITH * RETURN a.n AS a, length(p) AS l ORDER BY a'))
    = json('[{"a":1,"l":1},{"a":3,"l":null}]');
INSERT INTO _assert SELECT 'the same path value as without WITH',
    cypher('MATCH p = (a:A89)-[:R89]->(b) WITH * RETURN p AS path')
    = cypher('MATCH p = (a:A89)-[:R89]->(b) RETURN p AS path');
INSERT INTO _assert SELECT 'nodes() after two WITH *',
    json(cypher('MATCH p = (a:A89)-[:R89]->(b) WITH * WHERE length(p) = 1 WITH * RETURN [x IN nodes(p) | x.n] AS ns'))
    = json('[{"ns":[1,2]}]');

SELECT 'Test 89 passed: ' || count(*) || ' assertions' AS result FROM _assert;
