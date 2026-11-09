-- ========================================================================
-- Test 145: ORDER BY, LIMIT, min/max of lists, and booleans after a write
--           (GQLITE-T-0548)
-- ========================================================================
-- PURPOSE: UNWIND ... CREATE / MERGE ... RETURN ... ORDER BY returned the
--          rows unsorted (and LIMIT took the first ones created). The row
--          pipeline compared lists as JSON text ([] after [2, 4, 6]). A
--          boolean function result in CREATE (coalesce(false, 0)) was
--          stored as 0. Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 145: ordering after a write ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'UNWIND CREATE ORDER BY',
    json(cypher('UNWIND [3, 1, 2] AS l CREATE (n:A {k: l}) RETURN n.k AS k ORDER BY k'))
        = json('[{"k":1},{"k":2},{"k":3}]');
INSERT INTO _assert SELECT 'UNWIND CREATE ORDER BY DESC LIMIT',
    json(cypher('UNWIND [3, 1, 2] AS l CREATE (n:B {k: l}) RETURN n.k AS k ORDER BY n.k DESC LIMIT 2'))
        = json('[{"k":3},{"k":2}]');
INSERT INTO _assert SELECT 'UNWIND CREATE DISTINCT',
    json(cypher('UNWIND [1, 1, 2] AS l CREATE (n:C {k: l}) RETURN DISTINCT n.k AS k ORDER BY k'))
        = json('[{"k":1},{"k":2}]');
INSERT INTO _assert SELECT 'UNWIND MERGE ORDER BY',
    json(cypher('UNWIND [3, 1, 2, 2] AS l MERGE (m:M {k: l}) RETURN m.k AS k ORDER BY k DESC LIMIT 2'))
        = json('[{"k":3},{"k":2}]');
INSERT INTO _assert SELECT 'MERGE made one node per value',
    json(cypher('MATCH (m:M) RETURN count(m) AS c')) = json('[{"c":3}]');

INSERT INTO _assert SELECT 'ORDER BY lists after CREATE',
    json(cypher('UNWIND [[2, 4], [], [1], [2, 3]] AS l CREATE (n:L {k: l}) RETURN n.k AS k ORDER BY k'))
        = json('[{"k":[]},{"k":[1]},{"k":[2,3]},{"k":[2,4]}]');
SELECT cypher('CREATE (:V {v: 0}), (:V {v: 5})');
INSERT INTO _assert SELECT 'max and min of lists after CREATE',
    json(cypher('MATCH (a:V) UNWIND [1, 2] AS i CREATE (n:W {k: [x IN [1, 2, 3] WHERE x > a.v | x * 2]}) RETURN max(n.k) AS mx, min(n.k) AS mn'))
        = json('[{"mx":[2,4,6],"mn":[]}]');

INSERT INTO _assert SELECT 'boolean function result in CREATE',
    json(cypher('CREATE (n:X {k: coalesce(false, 0), j: coalesce(2 IS NULL, 0)}) RETURN n.k AS k, n.j AS j'))
        = json('[{"k":false,"j":false}]');

SELECT 'Test 145 passed: ' || count(*) || ' assertions' AS result FROM _assert;
