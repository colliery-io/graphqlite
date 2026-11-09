-- ========================================================================
-- Test 146: a MATCH or CALL after a write sees every row's writes
--           (GQLITE-T-0547)
-- ========================================================================
-- PURPOSE: The row pipeline ran each row through all clauses before the
--          next row, so a MATCH or CALL after a CREATE saw only the rows
--          written before it (counts 1, 2). Cypher runs a clause for every
--          row before the next: both rows see both nodes. Assertions are
--          hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 146: read after a write ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'MATCH after CREATE',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:C {i: i}) WITH n MATCH (b:C) RETURN n.i AS i, count(b) AS c ORDER BY i'))
        = json('[{"i":1,"c":2},{"i":2,"c":2}]');
INSERT INTO _assert SELECT 'CALL after CREATE',
    json(cypher('UNWIND [1, 2] AS i CREATE (n:B {i: i}) WITH n CALL { MATCH (b:B) RETURN count(b) AS c } RETURN n.i AS i, c ORDER BY i'))
        = json('[{"i":1,"c":2},{"i":2,"c":2}]');
INSERT INTO _assert SELECT 'MATCH after SET',
    json(cypher('MATCH (x:C) SET x.seen = true WITH x MATCH (y:C) WHERE y.seen RETURN x.i AS i, count(y) AS c ORDER BY i'))
        = json('[{"i":1,"c":2},{"i":2,"c":2}]');
INSERT INTO _assert SELECT 'order kept through the barrier',
    json(cypher('UNWIND [3, 1, 2] AS i CREATE (n:E {i: i}) WITH n, i MATCH (e:E) RETURN i, count(e) AS c'))
        = json('[{"i":3,"c":3},{"i":1,"c":3},{"i":2,"c":3}]');

SELECT 'Test 146 passed: ' || count(*) || ' assertions' AS result FROM _assert;
