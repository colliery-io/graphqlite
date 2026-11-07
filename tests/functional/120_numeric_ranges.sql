-- ========================================================================
-- Test 120: float-to-integer and date ranges (GQLITE-T-0499, GQLITE-T-0503)
-- ========================================================================
-- PURPOSE: toInteger() of NaN, Infinity or a value outside the integer
--          range, a duration component that is infinite or too large, and
--          a date with a huge year were undefined behaviour in C (found by
--          UBSan and the fuzzer). They now give an error (toInteger,
--          duration) or null (date), and ordinary values are unchanged.
-- NOTE:    The error cases are in bindings/python/tests/test_new_functions.py
--          (an error stops `sqlite3 -bail`). Assertions are hard: _assert
--          has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 120: numeric ranges ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'toInteger truncates in range',
    json(cypher('RETURN toInteger(2.9) AS a, toInteger(-2.9) AS b, toInteger(''7.5'') AS c, toInteger(-9223372036854775808.0) AS d'))
    = json('[{"a":2,"b":-2,"c":7,"d":-9223372036854775808}]');
INSERT INTO _assert SELECT 'durations in range are unchanged',
    json_extract(cypher('RETURN duration({weeks: 2, days: 1.5, hours: 3}) AS d'), '$[0].d._iso8601') = 'P15DT15H'
    AND json_extract(cypher('RETURN duration(''P1.5Y2DT3H'') AS d'), '$[0].d._iso8601') = 'P1Y6M2DT3H';
INSERT INTO _assert SELECT 'a date with a year of more than 9 digits is null',
    json(cypher('RETURN date(''+999999999-01-01'') AS a, date(''+1000000000000000000000000000-01-01'') AS b'))
    = json('[{"a":"+999999999-01-01","b":null}]');

SELECT 'Test 120 passed: ' || count(*) || ' assertions' AS result FROM _assert;
