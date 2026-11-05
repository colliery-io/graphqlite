-- ========================================================================
-- Test 117: Infinity and NaN in results (GQLITE-T-0484)
-- ========================================================================
-- PURPOSE: a float division by zero gave Infinity, Infinity.0, Inf.0,
--          inf.0, 9.0e+999, "\u0001NaN" or null, by form. Each form now
--          gives the bare tokens Infinity, -Infinity and NaN. isNaN() tests
--          for NaN and is a boolean.
-- NOTE:    The tokens are not strict JSON, so results are compared as text.
--          Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 117: Infinity and NaN ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A117 {v: 1})') as setup;

INSERT INTO _assert SELECT 'literal division',
    cypher('RETURN 1 / 0.0 AS a, -1 / 0.0 AS b, 0.0 / 0.0 AS c')
    = '[{"a":Infinity,"b":-Infinity,"c":NaN}]';
INSERT INTO _assert SELECT 'property division',
    cypher('MATCH (n:A117) RETURN n.v / 0.0 AS a, (n.v - 1) / 0.0 AS c')
    = '[{"a":Infinity,"c":NaN}]';
INSERT INTO _assert SELECT 'list, map and subscript',
    cypher('MATCH (n:A117) RETURN [n.v / 0.0, 0.0 / 0.0] AS l, {x: -n.v / 0.0} AS m, [n.v / 0.0][0] AS i')
    = '[{"l":[Infinity,NaN],"m":{"x":-Infinity},"i":Infinity}]';
INSERT INTO _assert SELECT 'WITH, UNWIND and CALL',
    cypher('MATCH (n:A117) WITH n.v / 0.0 AS x UNWIND [x, 0.0 / 0.0] AS y CALL { WITH y RETURN y AS z } RETURN z')
    = '[{"z":Infinity},{"z":NaN}]';
INSERT INTO _assert SELECT 'collect and max',
    cypher('MATCH (n:A117) RETURN collect(n.v / 0.0) AS c, max(n.v / 0.0) AS m')
    = '[{"c":[Infinity],"m":Infinity}]';
INSERT INTO _assert SELECT 'isNaN and comparisons',
    cypher('RETURN isNaN(0.0 / 0.0) AS a, isNaN(1.0) AS b, 1 / 0.0 > 0 AS c, (0.0 / 0.0) = (0.0 / 0.0) AS d')
    = '[{"a":true,"b":false,"c":true,"d":false}]';
INSERT INTO _assert SELECT 'toString and toFloat',
    cypher('RETURN toString(0.0 / 0.0) AS a, toString(-1 / 0.0) AS b, toFloat(''NaN'') AS c, toFloat(''Infinity'') AS d')
    = '[{"a":"NaN","b":"-Infinity","c":NaN,"d":Infinity}]';
INSERT INTO _assert SELECT 'strings that look like the tokens stay strings',
    cypher('RETURN ''Infinity'' AS a, ''NaN'' AS b, [''Inf.0''] AS c, ''9e999'' AS d')
    = '[{"a":"Infinity","b":"NaN","c":["Inf.0"],"d":"9e999"}]';

SELECT cypher('MATCH (n:A117) SET n.i = n.v / 0.0, n.z = 0.0 / 0.0') as set_specials;

INSERT INTO _assert SELECT 'stored values',
    cypher('MATCH (n:A117) RETURN n.i AS i, n.z AS z, n.i > 0 AS p, isNaN(n.z) AS q')
    = '[{"i":Infinity,"z":NaN,"p":true,"q":true}]';
INSERT INTO _assert SELECT 'stored values in a node',
    cypher('MATCH (n:A117) RETURN properties(n) AS p')
    = '[{"p":{"v":1,"i":Infinity,"z":NaN}}]';
INSERT INTO _assert SELECT 'cypher_rows',
    (SELECT group_concat(row, '|') FROM cypher_rows('MATCH (n:A117) RETURN n.i AS i, [0.0 / 0.0] AS l'))
    LIKE '%Infinity%NaN%';

SELECT 'Test 117 passed: ' || count(*) || ' assertions' AS result FROM _assert;
