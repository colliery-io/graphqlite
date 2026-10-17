-- ========================================================================
-- Test 65: LOAD CSV (GQLITE-T-0134)
-- ========================================================================
-- PURPOSE: LOAD CSV [WITH HEADERS] FROM a local file AS row: a map with
--          headers, a list without; quoted fields, "" and line breaks in
--          quotes; an empty field is null; FIELDTERMINATOR; linenumber()
--          and file(). Paths are relative to the working directory.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 65: LOAD CSV ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('LOAD CSV WITH HEADERS FROM "tests/functional/data/65_people.csv" AS row CREATE (:P65 {name: row.name, age: toInteger(row.age), note: row.note})') as load;
INSERT INTO _assert SELECT 'rows with headers',
    json(cypher('MATCH (p:P65) RETURN p.name AS n, p.age AS a, p.note AS note ORDER BY a'))
    = json('[{"n":"Sam","a":25,"note":null},{"n":"Robin","a":30,"note":"hello, world"},{"n":"Jamie \"J\"","a":41,"note":"two\nlines"}]');
INSERT INTO _assert SELECT 'rows without headers',
    json_extract(cypher('LOAD CSV FROM "tests/functional/data/65_people.csv" AS row RETURN row ORDER BY linenumber() LIMIT 1'), '$[0].row') = json('["name","age","note"]');
INSERT INTO _assert SELECT 'filter after LOAD CSV',
    json_extract(cypher('LOAD CSV WITH HEADERS FROM "tests/functional/data/65_people.csv" AS row WITH row WHERE toInteger(row.age) > 26 RETURN count(*) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'linenumber and file',
    json(cypher('LOAD CSV WITH HEADERS FROM "file://tests/functional/data/65_people.csv" AS row RETURN linenumber() AS l, file() AS f ORDER BY l LIMIT 1'))
    = json('[{"l":2,"f":"tests/functional/data/65_people.csv"}]');
INSERT INTO _assert SELECT 'field terminator',
    json(cypher('LOAD CSV WITH HEADERS FROM "tests/functional/data/65_semicolon.csv" AS row FIELDTERMINATOR ";" RETURN row.a AS a, row.b AS b ORDER BY a'))
    = json('[{"a":"1","b":"x"},{"a":"2","b":"y"}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 65 Complete ===' as test_section;
