-- ========================================================================
-- Test 121: integer arithmetic at the range edges (GQLITE-T-0500)
-- ========================================================================
-- PURPOSE: integer +, -, *, / and unary minus that overflow gave a float
--          (two literals) or a wrapped value (anything else), and SET or
--          CREATE stored null. They are an error now (the error cases are
--          in bindings/python/tests/test_new_functions.py, as an error stops
--          `sqlite3 -bail`); values at the edges stay exact.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 121: integer overflow ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'literals at the edges',
    json(cypher('RETURN 9223372036854775806 + 1 AS a, -9223372036854775807 - 1 AS b, 4611686018427387903 * 2 AS c, -9223372036854775808 / 1 AS d'))
    = json('[{"a":9223372036854775807,"b":-9223372036854775808,"c":9223372036854775806,"d":-9223372036854775808}]');
INSERT INTO _assert SELECT 'values at the edges',
    json(cypher('UNWIND [9223372036854775806] AS x RETURN x + 1 AS a, -x AS b, x * 1 AS c, x / -1 AS d'))
    = json('[{"a":9223372036854775807,"b":-9223372036854775806,"c":9223372036854775806,"d":-9223372036854775806}]');
INSERT INTO _assert SELECT 'a float operand stays a float',
    json(cypher('RETURN 9223372036854775807 + 1.0 AS f')) = json('[{"f":9.2233720368547758e+18}]');
INSERT INTO _assert SELECT 'time arithmetic wraps by the day',
    json(cypher('RETURN time(''23:30'') + duration({hours: 25}) AS a, time(''01:00'') - duration({hours: 3}) AS b'))
    = json('[{"a":"00:30+00:00","b":"22:00+00:00"}]');

SELECT 'Test 121 passed: ' || count(*) || ' assertions' AS result FROM _assert;
