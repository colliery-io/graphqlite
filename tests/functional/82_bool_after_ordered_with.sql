-- ========================================================================
-- Test 82: value types after WITH ... ORDER BY (GQLITE-T-0466)
-- ========================================================================
-- PURPOSE: the ORDER BY of a WITH is carried onto the RETURN's SELECT, and
--          SQLite's sorter drops value subtypes. The RETURN wrote its type
--          tags only for its own ORDER BY, so booleans came back as the
--          strings "true"/"false". It now counts the carried ORDER BY too
--          (GQLITE-A-0007). Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 82: value types after an ordered WITH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A82 {id: 1, b: false, l: [1, 2]}), (:A82 {id: 2, b: true, l: [3]})') as setup;

INSERT INTO _assert SELECT 'WITH * ORDER BY LIMIT',
    json(cypher('MATCH (a:A82) WITH * ORDER BY a.id LIMIT 2 RETURN a.b AS b'))
    = json('[{"b":false},{"b":true}]');
INSERT INTO _assert SELECT 'WITH a ORDER BY',
    json(cypher('MATCH (a:A82) WITH a ORDER BY a.id DESC RETURN a.b AS b, a.l AS l'))
    = json('[{"b":true,"l":[3]},{"b":false,"l":[1,2]}]');
INSERT INTO _assert SELECT 'WITH a ORDER BY SKIP',
    json(cypher('MATCH (a:A82) WITH a ORDER BY a.id SKIP 1 RETURN a.b AS b'))
    = json('[{"b":true}]');
INSERT INTO _assert SELECT 'map projection',
    json(cypher('MATCH (a:A82) WITH a ORDER BY a.id RETURN a {.b} AS m'))
    = json('[{"m":{"b":false}},{"m":{"b":true}}]');

SELECT 'Test 82 passed: ' || count(*) || ' assertions' AS result FROM _assert;
