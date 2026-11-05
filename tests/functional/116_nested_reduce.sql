-- ========================================================================
-- Test 116: reduce() inside reduce() (GQLITE-T-0491, GQLITE-T-0493)
-- ========================================================================
-- PURPOSE: a reduce() whose step is a string reduce() gave a number
--          ("21" became 21) next to another column; an inner reduce()
--          that read the outer variable gave null, because both levels
--          read the same unaliased json_each.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 116: nested reduce ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A116 {id: 2, l: [1, 2]})') as setup;

INSERT INTO _assert SELECT 'string reduce() in a reduce() step stays a string',
    json(cypher('MATCH (a:A116) RETURN a.id AS k, reduce(acc = null, z IN [1] | reduce(acc = '''', x IN [a.id, 1] | acc + toString(x))) AS x'))
    = json('[{"k":2,"x":"21"}]');
INSERT INTO _assert SELECT 'inner reduce() reads the outer variable',
    json(cypher('RETURN reduce(acc = 0, z IN [1, 2] | acc + reduce(s = 0, y IN [z] | s + y)) AS m'))
    = json('[{"m":3}]');
INSERT INTO _assert SELECT 'inner reduce() starts from the outer accumulator',
    json(cypher('RETURN reduce(acc = 0, z IN [1, 2] | reduce(s = acc, y IN [z] | s + y)) AS n'))
    = json('[{"n":3}]');
INSERT INTO _assert SELECT 'inner reduce() over the outer list element',
    json(cypher('RETURN reduce(a = [], x IN [[1], [2, 3]] | a + reduce(b = [], y IN x | b + [y * 10])) AS f'))
    = json('[{"f":[10,20,30]}]');
INSERT INTO _assert SELECT 'nested reduce() over a property list',
    json(cypher('MATCH (a:A116) RETURN a.id AS k, reduce(t = 0, z IN a.l | t + reduce(u = 0, w IN [z, a.id] | u + w)) AS n'))
    = json('[{"k":2,"n":7}]');

SELECT 'Test 116 passed: ' || count(*) || ' assertions' AS result FROM _assert;
