-- ========================================================================
-- Test 137: UNWIND and MATCH after a write keep the row order
--           (GQLITE-T-0535)
-- ========================================================================
-- PURPOSE: An UNWIND after a write ran in a read handler: with a WITH
--          LIMIT before the CREATE it failed ("near '.'"), without it the
--          rows were interleaved. A tail WITH dropped the row's sort key,
--          and a tail MATCH put its extra matches after every row.
--          Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 137: UNWIND after a write ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:M {k: 1}), (:M {k: 2})');

INSERT INTO _assert SELECT 'UNWIND after CREATE, in order',
    json(cypher('UNWIND [1, 2] AS x CREATE (n:A {v: x}) WITH n, x UNWIND [10, 20] AS y RETURN x, y'))
        = json('[{"x":1,"y":10},{"x":1,"y":20},{"x":2,"y":10},{"x":2,"y":20}]');
INSERT INTO _assert SELECT 'WITH LIMIT, CREATE, UNWIND',
    json(cypher('UNWIND [1, 2] AS x MATCH (m:M {k: 1}) WITH x LIMIT 2 CREATE (n:B {v: x}) WITH n, x UNWIND [10, 20] AS y RETURN x, y'))
        = json('[{"x":1,"y":10},{"x":1,"y":20},{"x":2,"y":10},{"x":2,"y":20}]');
INSERT INTO _assert SELECT 'CREATE then UNWIND, values',
    json(cypher('CREATE (n:C {v: 9}) WITH n UNWIND [1, 2] AS y RETURN n.v AS v, y'))
        = json('[{"v":9,"y":1},{"v":9,"y":2}]');
INSERT INTO _assert SELECT 'MATCH after CREATE, in order',
    json(cypher('UNWIND [1, 2] AS x CREATE (n:D {v: x}) WITH n, x MATCH (m:M) RETURN x, m.k AS k'))
        = json('[{"x":1,"k":1},{"x":1,"k":2},{"x":2,"k":1},{"x":2,"k":2}]');
INSERT INTO _assert SELECT 'nodes created once per row',
    json(cypher('MATCH (n) WHERE n:A OR n:B OR n:C OR n:D RETURN count(n) AS c')) = json('[{"c":7}]');

SELECT 'Test 137 passed: ' || count(*) || ' assertions' AS result FROM _assert;
