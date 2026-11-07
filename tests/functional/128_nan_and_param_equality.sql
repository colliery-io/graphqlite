-- ========================================================================
-- Test 128: NaN comparisons, NaN through math functions, parameter
--           equality of lists and maps (GQLITE-T-0518)
-- ========================================================================
-- PURPOSE: From the Neo4j acceptance features (NaNAcceptance,
--          TernaryEqualityAcceptance):
--          * an ordered comparison with NaN held in a variable, parameter
--            or property was null; it is false (NOT gives true);
--          * abs(), ceil() and other math functions of NaN gave a number;
--            they give NaN;
--          * $a = $b of two lists or maps compared JSON text; it is the
--            three-valued element equality ([1, 2] = [1, null] is null);
--          * a float literal may have a type suffix (1.0f, 2.5d).
--          isNaN("foo") is a type error (in the Python tests: an error
--          stops sqlite3 -bail). Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 128: NaN and parameter equality ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'NaN in a variable',
    json(cypher('WITH 0.0 / 0.0 AS nan RETURN nan < 0.0 AS a, NOT (nan < 0.0) AS b, nan >= nan AS c, nan < ''x'' AS d'))
        = json('[{"a":false,"b":true,"c":false,"d":null}]');
INSERT INTO _assert SELECT 'NaN in a parameter',
    json(cypher('RETURN NOT (0.0 <= 0.0 / $zero) AS a, NOT (0.0 >= 0.0 / $zero) AS b', '{"zero": 0.0}'))
        = json('[{"a":true,"b":true}]');
SELECT cypher('CREATE ({nan: 0.0 / 0.0})');
INSERT INTO _assert SELECT 'NaN in a property',
    json(cypher('MATCH (n) RETURN NOT (n.nan < 0.0) AS a, n.nan > 1 AS b')) = json('[{"a":true,"b":false}]');
INSERT INTO _assert SELECT 'math functions of NaN give NaN',
    json(cypher('RETURN isNaN(abs(0 / 0.0)) AS a, isNaN(ceil(0.0 / 0.0)) AS b, abs(-2) AS c, ceil(1.2) AS d'))
        = json('[{"a":true,"b":true,"c":2,"d":2}]');
INSERT INTO _assert SELECT 'parameter lists with a null element',
    json(cypher('RETURN $a = $b AS eq, $a <> $b AS neq', '{"a": [1, 2, 3], "b": [1, null, 3]}'))
        = json('[{"eq":null,"neq":null}]');
INSERT INTO _assert SELECT 'parameter maps',
    json(cypher('RETURN $a = $b AS eq, $a = $c AS e2', '{"a": {"k": 42}, "b": {"k": null}, "c": {"k": 42}}'))
        = json('[{"eq":null,"e2":true}]');
INSERT INTO _assert SELECT 'parameter scalars',
    json(cypher('RETURN $i = $f AS a, $t = $i AS b, $s = $s AS c', '{"i": 1, "f": 1.0, "t": true, "s": "x"}'))
        = json('[{"a":true,"b":false,"c":true}]');
INSERT INTO _assert SELECT 'float literal suffix',
    json(cypher('RETURN 1.0f AS a, 2.5d AS b')) = json('[{"a":1.0,"b":2.5}]');

SELECT 'Test 128 passed: ' || count(*) || ' assertions' AS result FROM _assert;
