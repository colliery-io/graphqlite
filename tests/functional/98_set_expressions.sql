-- ========================================================================
-- Test 98: SET with any value expression (GQLITE-T-0456)
-- ========================================================================
-- PURPOSE: MATCH + SET refused CASE and `properties(a).s`, and stored
--          nothing for a `+=` map that reads the node, reverse(),
--          `a.v IS NULL`, `a.v ^ 2` or `{x: a.v}.x`. A SET whose value is
--          not plain now runs in the row pipeline, which computes the value
--          for the row. Found by the metamorphic harness (relation
--          set_readback).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 98: SET value expressions ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A98 {id: 1, v: 2, s: ''x''}), (:A98 {id: 2, v: 0, s: ''y''})') as setup;

INSERT INTO _assert SELECT '+= with values that read the node',
    json_extract(cypher('MATCH (a:A98) SET a += {k: a.s, j: toString(a.id)}'), '$.properties_set') = 4;
SELECT cypher('MATCH (a:A98) SET a.k2 = CASE WHEN a.v > 1 THEN ''hi'' END') as s2;
SELECT cypher('MATCH (a:A98) SET a.k3 = reverse([1, 2]), a.k4 = properties(a).s') as s3;
SELECT cypher('MATCH (a:A98) SET a.k5 = a.v IS NULL, a.k6 = a.v ^ 2, a.k7 = {x: a.v}.x, a.k8 = [a.id, 1]') as s4;
SELECT cypher('MATCH (a:A98) SET a.k9 = [x IN [1, 2, 3] WHERE x > a.v | x * 2]') as s5;
INSERT INTO _assert SELECT 'every value stored, per node',
    json(cypher('MATCH (a:A98) RETURN a.k AS k, a.j AS j, a.k2 AS k2, a.k3 AS k3, a.k4 AS k4, a.k5 AS k5, a.k6 AS k6, a.k7 AS k7, a.k8 AS k8, a.k9 AS k9 ORDER BY a.id'))
    = json('[{"k":"x","j":"1","k2":"hi","k3":[2,1],"k4":"x","k5":false,"k6":4.0,"k7":2,"k8":[1,1],"k9":[6]},'
           || '{"k":"y","j":"2","k2":null,"k3":[2,1],"k4":"y","k5":false,"k6":0.0,"k7":0,"k8":[2,1],"k9":[2,4,6]}]');
INSERT INTO _assert SELECT 'a plain property copy keeps a JSON-looking string a string',
    (SELECT json_extract(cypher('MATCH (a:A98 {id: 1}) SET a.t = ''[1]'' WITH a SET a.u = a.t RETURN a.u AS u'), '$[0].u')) = '[1]';

SELECT 'Test 98 passed: ' || count(*) || ' assertions' AS result FROM _assert;
