-- ========================================================================
-- Test 100: CALL algorithm() YIELD on an empty graph (GQLITE-T-0442)
-- ========================================================================
-- PURPOSE: the outputs of a CALL were the keys of its first row: with no
--          rows, YIELD * or no YIELD returned an empty result whatever
--          clauses followed (`YIELD * RETURN count(*)` gave no row), and an
--          unknown YIELD name passed. Each algorithm now declares its
--          outputs; YIELD is checked against them, and an empty result
--          flows into the rest of the query.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 100: CALL YIELD on an empty graph ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'YIELD * then RETURN count(*)',
    json(cypher('CALL pageRank() YIELD * RETURN count(*) AS n')) = json('[{"n":0}]');
INSERT INTO _assert SELECT 'a named output then an aggregate',
    json(cypher('CALL wcc() YIELD component RETURN count(component) AS n')) = json('[{"n":0}]');
INSERT INTO _assert SELECT 'no rows: no CREATE',
    json_extract(cypher('CALL wcc() YIELD * CREATE (:Log100)'), '$.nodes_created') = 0;

SELECT cypher('CREATE (:N100 {id: ''a''})-[:R100]->(:N100 {id: ''b''})') as setup;
INSERT INTO _assert SELECT 'with rows',
    json(cypher('CALL pageRank() YIELD node_id, score RETURN count(*) AS n')) = json('[{"n":2}]');

SELECT 'Test 100 passed: ' || count(*) || ' assertions' AS result FROM _assert;
