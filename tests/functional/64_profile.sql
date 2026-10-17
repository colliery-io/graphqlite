-- ========================================================================
-- Test 64: PROFILE (GQLITE-T-0135)
-- ========================================================================
-- PURPOSE: PROFILE runs the query and gives one row: the dispatch pattern,
--          the clauses, the SQL and its SQLite plan, the row count, the
--          time and the writes. `profile` stays a name.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 64: PROFILE ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('UNWIND range(1, 20) AS i CREATE (:U64 {id: i, profile: i % 2})') as setup;
INSERT INTO _assert SELECT 'row count',
    json_extract(cypher('PROFILE MATCH (n:U64) WHERE n.id > 15 RETURN n.id AS id'), '$[0].rows') = 5;
INSERT INTO _assert SELECT 'pattern and plan',
    json_extract(cypher('PROFILE MATCH (n:U64) RETURN n'), '$[0].pattern') = 'MATCH+RETURN'
    AND json_array_length(json_extract(cypher('PROFILE MATCH (n:U64) RETURN n'), '$[0].plan')) > 0;
INSERT INTO _assert SELECT 'a second PROFILE profiles again',
    json_extract(cypher('PROFILE MATCH (n:U64) WHERE n.id > 15 RETURN n.id AS id'), '$[0].rows') = 5;
INSERT INTO _assert SELECT 'writes happen and are counted',
    json_extract(cypher('PROFILE MATCH (n:U64 {id: 1}) SET n.x = 1'), '$[0].properties_set') = 1
    AND json_extract(cypher('MATCH (n:U64 {id: 1}) RETURN n.x AS x'), '$[0].x') = 1;
INSERT INTO _assert SELECT 'profile is still a name',
    json(cypher('MATCH (n:U64 {id: 3}) RETURN n.profile AS profile')) = json('[{"profile":1}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 64 Complete ===' as test_section;
