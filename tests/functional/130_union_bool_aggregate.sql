-- ========================================================================
-- Test 130: max() / min() of a boolean in UNION and DISTINCT
--           (GQLITE-T-0521)
-- ========================================================================
-- PURPOSE: max() or min() of a boolean was integer 0 / 1, while every
--          other boolean in RETURN is the text of _gql_bool_str, so UNION
--          kept `false` twice (found by the metamorphic harness, seed
--          515000). Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 130: UNION of boolean aggregates ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'UNION of false and max(false)',
    json(cypher('RETURN false AS c UNION RETURN max(1 = 2) AS c')) = json('[{"c":false}]');
INSERT INTO _assert SELECT 'UNION of true and min(true)',
    json(cypher('RETURN true AS c UNION RETURN min(1 = 1) AS c')) = json('[{"c":true}]');
INSERT INTO _assert SELECT 'max and min of booleans are booleans',
    json(cypher('UNWIND [1, 2] AS x RETURN max(x = 2) AS a, min(x = 2) AS b')) = json('[{"a":true,"b":false}]');
INSERT INTO _assert SELECT 'grouped',
    json(cypher('UNWIND [1, 2] AS x RETURN x % 2 AS k, max(x > 1) AS m ORDER BY k'))
        = json('[{"k":0,"m":true},{"k":1,"m":false}]');

SELECT 'Test 130 passed: ' || count(*) || ' assertions' AS result FROM _assert;
