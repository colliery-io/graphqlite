-- ========================================================================
-- Test 106: MATCH ... CREATE ... RETURN creates once per row
--           (GQLITE-T-0488)
-- ========================================================================
-- PURPOSE: when the created pattern bound no variable that the RETURN read,
--          the CREATE ran once, and the RETURN ran the MATCH again after
--          the write (it saw the new node). A MATCH with no rows and an
--          aggregate RETURN still created a node.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 106: CREATE per row with RETURN ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:S106 {id: 1}), (:S106 {id: 2})') as setup;

INSERT INTO _assert SELECT 'RETURN gives the MATCH rows only',
    json(cypher('MATCH (a:S106) CREATE (a)-[:T]->(:N106) RETURN a.id AS id ORDER BY id'))
    = json('[{"id":1},{"id":2}]');
INSERT INTO _assert SELECT 'one node per row',
    json(cypher('MATCH (n:N106) RETURN count(n) AS c')) = json('[{"c":2}]');

INSERT INTO _assert SELECT 'aggregate RETURN counts the rows',
    json(cypher('MATCH (a:S106) CREATE (:M106 {k: a.id}) RETURN count(*) AS c'))
    = json('[{"c":2}]');
INSERT INTO _assert SELECT 'aggregate RETURN: one node per row',
    json(cypher('MATCH (n:M106) RETURN count(n) AS c')) = json('[{"c":2}]');

INSERT INTO _assert SELECT 'no rows: count is 0',
    json(cypher('MATCH (a:Missing106) CREATE (:Z106) RETURN count(*) AS c'))
    = json('[{"c":0}]');
INSERT INTO _assert SELECT 'no rows: nothing created',
    json(cypher('MATCH (n:Z106) RETURN count(n) AS c')) = json('[{"c":0}]');

SELECT 'Test 106 passed: ' || count(*) || ' assertions' AS result FROM _assert;
