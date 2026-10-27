-- ========================================================================
-- Test 79: RETURN ... ORDER BY after WITH ... ORDER BY (GQLITE-T-0464)
-- ========================================================================
-- PURPOSE: a WITH's ORDER BY was carried into the final ORDER BY as its
--          leading keys, so a RETURN ... ORDER BY after
--          `WITH ... ORDER BY ... LIMIT n MATCH ...` sorted by the WITH's
--          keys first. Found by the metamorphic harness (DESC must be the
--          reverse of ASC).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 79: RETURN ORDER BY after WITH ORDER BY ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:P79 {id: 3, v: 3})-[:S79]->(:P79 {id: 4, v: 1}), (:Q79 {k: 2}), (:Q79 {k: 1}), (:Q79 {k: 3})') as setup;

INSERT INTO _assert SELECT 'RETURN ORDER BY wins after WITH ORDER BY LIMIT and MATCH',
    json(cypher('MATCH (a:P79)-[r]-(b) WITH * ORDER BY a.id DESC LIMIT 4 MATCH (b) RETURN b.v AS c0 ORDER BY c0'))
    = json('[{"c0":1},{"c0":3}]');
INSERT INTO _assert SELECT 'descending, after OPTIONAL MATCH',
    json(cypher('MATCH (a:P79)-[r]-(b) WITH * ORDER BY a.id LIMIT 4 OPTIONAL MATCH (b)-->(f:Z79) RETURN b.v AS c0 ORDER BY c0 DESC'))
    = json('[{"c0":3},{"c0":1}]');
INSERT INTO _assert SELECT 'the WITH order and limit still choose the rows',
    json(cypher('MATCH (q:Q79) WITH q ORDER BY q.k DESC LIMIT 2 RETURN q.k AS k ORDER BY k')) = json('[{"k":2},{"k":3}]');
INSERT INTO _assert SELECT 'without RETURN ORDER BY the WITH order stays',
    json(cypher('MATCH (q:Q79) WITH q ORDER BY q.k RETURN collect(q.k) AS ks')) = json('[{"ks":[1,2,3]}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 79 Complete ===' as test_section;
