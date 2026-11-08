-- ========================================================================
-- Test 135: CALL { } subqueries in sequence, row order, path import
--           (GQLITE-T-0523)
-- ========================================================================
-- PURPOSE: Two CALL { } clauses in sequence returned only the statistics.
--          A correlated CALL that forks rows put them out of the outer
--          order. A path imported into CALL arrived as a list of ids, so
--          length(q) counted ids. Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 135: CALL subquery rows ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'two CALLs in sequence',
    json(cypher('UNWIND [1, 2] AS x CALL { WITH x RETURN x * 10 AS a } CALL { WITH a RETURN a + 1 AS b } RETURN x, a, b'))
        = json('[{"x":1,"a":10,"b":11},{"x":2,"a":20,"b":21}]');
INSERT INTO _assert SELECT 'correlated CALL keeps outer order',
    json(cypher('UNWIND [1, 2] AS x CALL { WITH x UNWIND [1, 2] AS y RETURN y } RETURN x, y'))
        = json('[{"x":1,"y":1},{"x":1,"y":2},{"x":2,"y":1},{"x":2,"y":2}]');

SELECT cypher('CREATE (:P {v: 1})-[:R]->(:P {v: 2})');

INSERT INTO _assert SELECT 'path imported into CALL',
    json(cypher('MATCH p = (:P)-->() CALL { WITH p RETURN p AS q } RETURN length(q) AS l'))
        = json('[{"l":1}]');
INSERT INTO _assert SELECT 'single-node path imported into CALL',
    json(cypher('MATCH p = (:P) CALL { WITH p RETURN p AS q } RETURN length(q) AS l'))
        = json('[{"l":0},{"l":0}]');
INSERT INTO _assert SELECT 'nodes() of an imported path',
    json(cypher('MATCH p = (:P)-->() CALL { WITH p RETURN nodes(p) AS ns } RETURN size(ns) AS s'))
        = json('[{"s":2}]');

SELECT 'Test 135 passed: ' || count(*) || ' assertions' AS result FROM _assert;
