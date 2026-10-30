-- ========================================================================
-- Test 87: a write after two MATCH clauses whose second WHERE reads the
--          first (GQLITE-T-0469)
-- ========================================================================
-- PURPOSE: the MATCH + CREATE / SET handlers bound each MATCH on its own, so
--          `MATCH (a) MATCH (b) WHERE b.i = a.i + 1 CREATE (a)-[:R]->(b)`
--          failed with "`a` is not defined", and with a RETURN counted the
--          wrong rows. Such a query now runs in the row pipeline, which reads
--          all MATCH clauses as one query.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 87: write after MATCH ... MATCH ... WHERE ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('UNWIND range(1, 4) AS i CREATE (:N87 {i: i})') as setup;

INSERT INTO _assert SELECT 'CREATE links the pairs',
    json_extract(cypher('MATCH (a:N87) MATCH (b:N87) WHERE b.i = a.i + 1 CREATE (a)-[:R87]->(b)'), '$.relationships_created') = 3;
INSERT INTO _assert SELECT 'the links',
    json(cypher('MATCH (a:N87)-[:R87]->(b) RETURN a.i AS a, b.i AS b ORDER BY a'))
    = json('[{"a":1,"b":2},{"a":2,"b":3},{"a":3,"b":4}]');
INSERT INTO _assert SELECT 'SET reads both',
    json_extract(cypher('MATCH (a:N87) MATCH (b:N87) WHERE b.i = a.i + 1 SET b.p = a.i'), '$.properties_set') = 3;
INSERT INTO _assert SELECT 'the set values',
    json(cypher('MATCH (b:N87) RETURN b.i AS i, b.p AS p ORDER BY i'))
    = json('[{"i":1,"p":null},{"i":2,"p":1},{"i":3,"p":2},{"i":4,"p":3}]');
INSERT INTO _assert SELECT 'CREATE with RETURN counts the matched rows',
    json(cypher('MATCH (a:N87) MATCH (b:N87) WHERE b.i = a.i + 1 CREATE (a)-[:Q87]->(b) RETURN count(*) AS n'))
    = json('[{"n":3}]');
INSERT INTO _assert SELECT 'OPTIONAL MATCH keeps every row',
    json(cypher('MATCH (a:N87) OPTIONAL MATCH (b:N87) WHERE b.i = a.i + 10 CREATE (a)-[:Z87]->(:Q87) RETURN count(*) AS n'))
    = json('[{"n":4}]');

SELECT 'Test 87 passed: ' || count(*) || ' assertions' AS result FROM _assert;
