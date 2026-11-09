-- ========================================================================
-- Test 141: FOREACH ... SET of expressions on the loop variable
--           (GQLITE-T-0543)
-- ========================================================================
-- PURPOSE: FOREACH (a IN nodes | SET a.k = a.id) stored the internal id:
--          the loop variable was read as the node's JSON. a.s and
--          reverse([a.id, 2]) stored null; CASE and properties(a).s were
--          refused. A list result of a function in SET (reverse, split)
--          was dropped. Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 141: FOREACH SET values ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A {id: 3, v: 2, s: ''xy''}), (:A {id: 5, v: 0, s: ''y''})');
SELECT cypher('MATCH (a:A) WITH collect(a) AS ns FOREACH (a IN ns | SET a.k = a.id, a.k2 = a.s, a.k3 = reverse([a.id, 2]), a.k4 = CASE a.s WHEN ''xy'' THEN 1 ELSE 0 END, a.k5 = [a.id, 1], a.k6 = properties(a).s, a.k7 = {x: a.v}.x)');
INSERT INTO _assert SELECT 'FOREACH SET reads the node',
    json(cypher('MATCH (a:A) RETURN a.id AS id, a.k AS k, a.k2 AS k2, a.k3 AS k3, a.k4 AS k4, a.k5 AS k5, a.k6 AS k6, a.k7 AS k7 ORDER BY id'))
        = json('[{"id":3,"k":3,"k2":"xy","k3":[2,3],"k4":1,"k5":[3,1],"k6":"xy","k7":2},{"id":5,"k":5,"k2":"y","k3":[2,5],"k4":0,"k5":[5,1],"k6":"y","k7":0}]');

SELECT cypher('FOREACH (i IN [1, 2] | CREATE (:N {v: i}))');
SELECT cypher('MATCH (n:N) WITH collect(n) AS ns FOREACH (x IN ns | SET x.w = x.v * 10)');
INSERT INTO _assert SELECT 'FOREACH SET arithmetic',
    json(cypher('MATCH (n:N) RETURN n.v AS v, n.w AS w ORDER BY v')) = json('[{"v":1,"w":10},{"v":2,"w":20}]');

SELECT cypher('UNWIND [{a: 1}, {a: 2}] AS row FOREACH (x IN [row] | CREATE (:R {a: x.a}))');
INSERT INTO _assert SELECT 'FOREACH over maps still reads keys',
    json(cypher('MATCH (r:R) RETURN r.a AS a ORDER BY a')) = json('[{"a":1},{"a":2}]');

INSERT INTO _assert SELECT 'SET of a list-valued function',
    json(cypher('CREATE (b:B {s: ''a,b''}) SET b.k = split(b.s, '',''), b.r = reverse([1, 2]) RETURN b.k AS k, b.r AS r'))
        = json('[{"k":["a","b"],"r":[2,1]}]');

SELECT 'Test 141 passed: ' || count(*) || ' assertions' AS result FROM _assert;
