-- ========================================================================
-- Test 101: RETURN * after LOAD CSV (GQLITE-T-0443)
-- ========================================================================
-- PURPOSE: LOAD CSV carries its line number and file in the internal
--          variables __gql_linenumber and __gql_file, and RETURN * showed
--          them. It now shows only user variables; WITH * still carries
--          them for linenumber() and file(). (An unclosed quote, a NUL byte,
--          a FIELDTERMINATOR of more than one character and a file too
--          large to load are now errors: see the Python tests.)
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 101: RETURN * after LOAD CSV ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'RETURN * shows the row only',
    (SELECT group_concat(key) FROM json_each(json_extract(cypher('LOAD CSV WITH HEADERS FROM "tests/functional/data/65_semicolon.csv" AS row FIELDTERMINATOR ";" RETURN * LIMIT 1'), '$[0]'))) = 'row';
INSERT INTO _assert SELECT 'WITH * then RETURN *',
    (SELECT group_concat(key) FROM json_each(json_extract(cypher('LOAD CSV WITH HEADERS FROM "tests/functional/data/65_semicolon.csv" AS row FIELDTERMINATOR ";" WITH * RETURN * LIMIT 1'), '$[0]'))) = 'row';
INSERT INTO _assert SELECT 'linenumber() after WITH *',
    json(cypher('LOAD CSV WITH HEADERS FROM "tests/functional/data/65_semicolon.csv" AS row FIELDTERMINATOR ";" WITH * RETURN linenumber() AS l ORDER BY l LIMIT 1'))
    = json('[{"l":2}]');

SELECT 'Test 101 passed: ' || count(*) || ' assertions' AS result FROM _assert;
