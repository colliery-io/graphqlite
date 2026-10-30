-- ========================================================================
-- Test 84: DISTINCT and UNION treat every null as one value (GQLITE-T-0467)
-- ========================================================================
-- PURPOSE: DISTINCT and UNION compare the hidden type tag column with the
--          value (GQLITE-A-0007). A null got a tag from its source: 'text'
--          for a missing property, 'null' from _gql_typetag, 'bool' for a
--          comparison, none for a literal. Two nulls then stayed apart. A
--          null now has no tag. Found by the metamorphic harness (also the
--          UNION part of GQLITE-T-0458).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 84: DISTINCT and UNION nulls ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A84 {id: 3, s: ''ab'', n: 5})-[:R84]->(:B84 {id: 4})') as setup;

INSERT INTO _assert SELECT 'missing property and list null',
    json(cypher('MATCH (a:A84) OPTIONAL MATCH (a)<-[:R84]-(d) WITH d UNWIND [d.id, null] AS i RETURN DISTINCT i AS c1'))
    = json('[{"c1":null}]');
INSERT INTO _assert SELECT 'UNION of two string expressions',
    json(cypher('MATCH (a:B84) RETURN substring(a.s, 0, 1) AS c UNION MATCH (a:B84) RETURN a.s + ''q'' AS c'))
    = json('[{"c":null}]');
INSERT INTO _assert SELECT 'UNION of a comparison and a literal',
    json(cypher('MATCH (a:B84) RETURN a.n > 1 AS c UNION RETURN null AS c'))
    = json('[{"c":null}]');
INSERT INTO _assert SELECT 'UNION of a property and a literal',
    json(cypher('MATCH (a:B84) RETURN a.n AS c UNION RETURN null AS c'))
    = json('[{"c":null}]');
INSERT INTO _assert SELECT 'UNION of a count and an arithmetic value',
    json(cypher('MATCH (a:B84) RETURN count(*) AS c UNION MATCH (a:B84) RETURN a.id - 3 AS c'))
    = json('[{"c":1}]');
INSERT INTO _assert SELECT 'values keep their types',
    json(cypher('MATCH (a:A84) RETURN DISTINCT a.s AS s, a.n > 1 AS b, toString(a.n) AS t UNION MATCH (a:A84) RETURN ''ab'' AS s, a.n > 2 AS b, ''5'' AS t'))
    = json('[{"s":"ab","b":true,"t":"5"}]');
INSERT INTO _assert SELECT 'SET to null removes the property',
    json(cypher('MATCH (a:A84) WITH a, CASE WHEN a.n > 9 THEN 1 END AS v SET a.n = v RETURN a.n AS n'))
    = json('[{"n":null}]');
INSERT INTO _assert SELECT 'no stored null',
    (SELECT count(*) FROM node_props_int WHERE value IS NULL) + (SELECT count(*) FROM node_props_text WHERE value IS NULL) = 0;

SELECT 'Test 84 passed: ' || count(*) || ' assertions' AS result FROM _assert;
