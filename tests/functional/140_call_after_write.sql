-- ========================================================================
-- Test 140: CALL { } after a write; an aggregating WITH over no rows
--           (GQLITE-T-0542)
-- ========================================================================
-- PURPOSE: CREATE ... WITH ... CALL { } returned only statistics and made
--          nothing; MATCH ... SET ... WITH ... CALL { } failed ("CALL {}
--          subquery requires executor-level handling"). In the row
--          pipeline an aggregating WITH that no row reached gave no row;
--          without grouping keys it gives one. Assertions are hard: _assert
--          has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 140: CALL after a write ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A {x: 1}), (:A {x: 2})');

INSERT INTO _assert SELECT 'CREATE then CALL sees the node',
    json(cypher('CREATE (:New) WITH 1 AS w CALL { MATCH (z:New) RETURN count(z) AS y } RETURN y'))
        = json('[{"y":1}]');
INSERT INTO _assert SELECT 'MATCH CREATE count then CALL',
    json(cypher('MATCH (a:A) CREATE (:New) WITH count(*) AS w CALL { MATCH (z:New) RETURN count(z) AS y } RETURN w, y'))
        = json('[{"w":2,"y":3}]');
INSERT INTO _assert SELECT 'SET then correlated CALL',
    json(cypher('MATCH (a:A) SET a.x = a.x * 10 WITH a CALL { WITH a RETURN a.x AS y } RETURN y ORDER BY y'))
        = json('[{"y":10},{"y":20}]');
INSERT INTO _assert SELECT 'MERGE then CALL',
    json(cypher('MERGE (m:M {k: 1}) WITH m CALL { MATCH (q:M) RETURN count(q) AS c } RETURN c'))
        = json('[{"c":1}]');

INSERT INTO _assert SELECT 'no rows: count(*) after a write is 0',
    json(cypher('MATCH (x:None) CREATE (:Z) WITH count(*) AS c RETURN c')) = json('[{"c":0}]');
INSERT INTO _assert SELECT 'no rows: CALL after count(*)',
    json(cypher('MATCH (x:None) CREATE (:Z) WITH count(*) AS w CALL { MATCH (z:Z) RETURN count(z) AS y } RETURN w, y'))
        = json('[{"w":0,"y":0}]');
INSERT INTO _assert SELECT 'no rows: grouping keys give no row',
    json(cypher('MATCH (x:None) CREATE (:Z) WITH x.k AS k, count(*) AS c RETURN k, c')) = json('[]');

SELECT 'Test 140 passed: ' || count(*) || ' assertions' AS result FROM _assert;
