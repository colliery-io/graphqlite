-- ========================================================================
-- Test 75: expressions in CREATE property maps (GQLITE-T-0438, #155)
-- ========================================================================
-- PURPOSE: a property value that is an expression over the row (CASE, a
--          list holding a variable, toString(n.v), {x: i}.x) was evaluated
--          without the row's values and written as null: after MATCH ...
--          UNWIND (the row pipeline did not look into CASE, NOT, IS NULL,
--          maps), and on the fast MATCH+CREATE / UNWIND+CREATE paths (they
--          now hand such values to the row pipeline). Found by a user (#155)
--          and the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 75: CREATE property expressions ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:T75 {id: ''k'', v: 2})') as setup;

SELECT cypher('MATCH (k:T75) UNWIND [1, 2] AS i CREATE (:S75 {i: i, f: CASE WHEN i = 1 THEN ''a'' ELSE ''b'' END})') as w1;
INSERT INTO _assert SELECT '#155: CASE after MATCH ... UNWIND',
    json(cypher('MATCH (s:S75) RETURN s.i AS i, s.f AS f ORDER BY i')) = json('[{"i":1,"f":"a"},{"i":2,"f":"b"}]');

SELECT cypher('MATCH (k:T75) UNWIND [1, 2] AS i CREATE (:L75 {l: [k.v, i]})') as w2;
INSERT INTO _assert SELECT 'list holding variables after MATCH ... UNWIND',
    json(cypher('MATCH (s:L75) RETURN s.l AS l ORDER BY l[1]')) = json('[{"l":[2,1]},{"l":[2,2]}]');

SELECT cypher('MATCH (k:T75) CREATE (:M75 {f: CASE WHEN k.v > 1 THEN ''hi'' ELSE ''lo'' END, l: [k.v, 1], s: toString(k.v), n: k.v IS NULL})') as w3;
INSERT INTO _assert SELECT 'CASE, list, function, IS NULL after MATCH',
    json(cypher('MATCH (s:M75) RETURN s.f AS f, s.l AS l, s.s AS s, s.n AS n')) = json('[{"f":"hi","l":[2,1],"s":"2","n":false}]');

SELECT cypher('UNWIND [1, 2] AS i CREATE (:U75 {l: [i, 1], m: {x: i}.x})') as w4;
INSERT INTO _assert SELECT 'list and map access after UNWIND',
    json(cypher('MATCH (s:U75) RETURN s.l AS l, s.m AS m ORDER BY m')) = json('[{"l":[1,1],"m":1},{"l":[2,1],"m":2}]');

SELECT cypher('MATCH (k:T75) CREATE (k)-[:R75 {w: k.v * 2, l: [k.v]}]->(:Z75 {z: toUpper(k.id)})') as w5;
INSERT INTO _assert SELECT 'relationship and new node properties',
    json(cypher('MATCH (:T75)-[r:R75]->(z:Z75) RETURN r.w AS w, r.l AS l, z.z AS z')) = json('[{"w":4,"l":[2],"z":"K"}]');

SELECT cypher('CREATE (:P75 {k: {x: 2}.x, l: [1, 2][0], c: CASE WHEN 1 = 1 THEN ''y'' END})') as w6;
INSERT INTO _assert SELECT 'plain CREATE: map access, subscript, CASE',
    json(cypher('MATCH (n:P75) RETURN n.k AS k, n.l AS l, n.c AS c')) = json('[{"k":2,"l":1,"c":"y"}]');

SELECT cypher('MATCH (k:T75) UNWIND [1, 2] AS i CREATE (:Q75 {i: i, b: CASE WHEN i = 1 THEN k.v IS NULL ELSE k.v > 1 END})') as w7;
INSERT INTO _assert SELECT 'a CASE of booleans stays boolean',
    json(cypher('MATCH (n:Q75) RETURN n.b AS b ORDER BY n.i')) = json('[{"b":false},{"b":true}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 75 Complete ===' as test_section;
