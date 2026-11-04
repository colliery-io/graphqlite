-- ========================================================================
-- Test 112: lists and maps inside lists keep their values (GQLITE-T-0483)
-- ========================================================================
-- PURPOSE: UNWIND wrote a map or list element as a JSON literal, so values
--          that read a variable became null (`UNWIND [{a: n.v}]` gave
--          {a: null}). reverse() of a list returned text without the JSON
--          subtype, so inside a list, a map or collect() it was the string
--          "[2,1]".
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 112: nested values ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:N112 {v: 1, s: ''x'', l: [1, 2]})') as setup;

INSERT INTO _assert SELECT 'UNWIND of a map that reads a variable',
    json(cypher('MATCH (n:N112) UNWIND [{a: n.v, b: [n.s]}, {a: 2}] AS x RETURN x.a AS a, x.b AS b'))
    = json('[{"a":1,"b":["x"]},{"a":2,"b":null}]');
INSERT INTO _assert SELECT 'UNWIND of a list that reads a variable',
    json(cypher('MATCH (n:N112) UNWIND [[n.v, 1]] AS x RETURN x'))
    = json('[{"x":[1,1]}]');
INSERT INTO _assert SELECT 'reverse() of a list inside a list and a map',
    json(cypher('MATCH (n:N112) RETURN [reverse(n.l)] AS x, {y: reverse(n.l)} AS m'))
    = json('[{"x":[[2,1]],"m":{"y":[2,1]}}]');
INSERT INTO _assert SELECT 'reverse() of a list through collect()',
    json(cypher('WITH reverse([1, 2]) AS y WITH collect(y) AS c RETURN c[0] AS x'))
    = json('[{"x":[2,1]}]');

SELECT 'Test 112 passed: ' || count(*) || ' assertions' AS result FROM _assert;
