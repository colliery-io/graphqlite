-- ========================================================================
-- Test 107: SET that rewrites a key of the inline MATCH properties
--           (GQLITE-T-0478)
-- ========================================================================
-- PURPOSE: MATCH + SET stepped the MATCH while it wrote. A SET item that
--          wrote a key of the inline properties again put the row back
--          under a new rowid; the MATCH found it again, without end.
--          The MATCH rows are now read before the first write.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 107: SET of an inline MATCH key ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:K107 {id: 1, v: 5}), (:K107 {id: 1, v: 6}), (:K107 {id: 2, v: 7})') as setup;
SELECT cypher('MATCH (n:K107 {id: 1}) SET n.a = 1, n.id = n.id') as step1;
INSERT INTO _assert SELECT 'two items, one rewrites the inline key',
    json(cypher('MATCH (n:K107) RETURN n.v AS v, n.id AS id, n.a AS a ORDER BY v'))
    = json('[{"v":5,"id":1,"a":1},{"v":6,"id":1,"a":1},{"v":7,"id":2,"a":null}]');

INSERT INTO _assert SELECT 'with RETURN',
    json(cypher('MATCH (n:K107 {id: 1}) SET n.b = 2, n.id = n.id RETURN n.v AS v, n.b AS b ORDER BY v'))
    = json('[{"v":5,"b":2},{"v":6,"b":2}]');

SELECT cypher('MATCH (n:K107 {id: 1}) SET n.w = n.id, n.id = n.v') as step3;
INSERT INTO _assert SELECT 'items read the values set before them',
    json(cypher('MATCH (n:K107) WHERE n.w IS NOT NULL RETURN n.id AS id, n.w AS w ORDER BY id'))
    = json('[{"id":5,"w":1},{"id":6,"w":1}]');

SELECT 'Test 107 passed: ' || count(*) || ' assertions' AS result FROM _assert;
