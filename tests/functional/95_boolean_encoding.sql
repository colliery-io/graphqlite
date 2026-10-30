-- ========================================================================
-- Test 95: one encoding for a boolean under UNION and DISTINCT
--          (GQLITE-T-0473)
-- ========================================================================
-- PURPOSE: a RETURN item that is a boolean literal or a label predicate
--          was the integer 1 / 0, where a property, a comparison or an
--          UNWIND value is the text 'true' / 'false' (_gql_bool_str). The
--          tag shows both as booleans, but UNION and DISTINCT compare 1
--          with 'true' and kept two rows. They now use the text too. An
--          UNWIND of a comparison (`UNWIND [1 > 0] AS x`) was the number 1.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 95: boolean encoding ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A95 {n: 5, b: true})') as setup;

INSERT INTO _assert SELECT 'property and literal',
    json(cypher('MATCH (a:A95) RETURN a.b AS b UNION RETURN true AS b')) = json('[{"b":true}]');
INSERT INTO _assert SELECT 'comparison and literal',
    json(cypher('MATCH (a:A95) RETURN a.n > 1 AS b UNION RETURN true AS b')) = json('[{"b":true}]');
INSERT INTO _assert SELECT 'UNWIND value and literal',
    json(cypher('UNWIND [true] AS x RETURN x UNION RETURN true AS x')) = json('[{"x":true}]');
INSERT INTO _assert SELECT 'label predicate and comparison',
    json(cypher('MATCH (a:A95) RETURN a:A95 AS b UNION MATCH (a:A95) RETURN a.n = a.n AS b')) = json('[{"b":true}]');
INSERT INTO _assert SELECT 'DISTINCT over a literal and a comparison',
    json(cypher('UNWIND [true, 1 > 0] AS x RETURN DISTINCT x')) = json('[{"x":true}]');
INSERT INTO _assert SELECT 'UNWIND of a comparison',
    json(cypher('UNWIND [1 > 0, 1 < 0] AS x RETURN x')) = json('[{"x":true},{"x":false}]');
INSERT INTO _assert SELECT 'literals and label predicates as values',
    json(cypher('MATCH (a:A95) RETURN true AS t, false AS f, a:A95 AS l, a:B95 AS m'))
    = json('[{"t":true,"f":false,"l":true,"m":false}]');

SELECT 'Test 95 passed: ' || count(*) || ' assertions' AS result FROM _assert;
