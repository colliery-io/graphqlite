-- ========================================================================
-- Test 108: writes after two MATCH clauses run once per row
--           (GQLITE-T-0477)
-- ========================================================================
-- PURPOSE: with two MATCH clauses (or MATCH then OPTIONAL MATCH) before a
--          write, the MATCH + write handlers bound each MATCH on its own
--          and kept its first row: SET and CREATE ran once, DELETE and
--          REMOVE did nothing, and after OPTIONAL MATCH the write reached
--          nodes the first MATCH never matched. SET or REMOVE through a
--          null variable must do nothing.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 108: writes after two MATCH clauses ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A108 {id: 1})-[:R]->(:B108 {id: 2}), (a)-[:R]->(:B108 {id: 3}),
               (:A108 {id: 4})-[:R]->(:B108 {id: 5}), (:B108 {id: 6})') as setup;

SELECT cypher('MATCH (a:A108) MATCH (a)-->(b) SET b.z = a.id') as w1;
INSERT INTO _assert SELECT 'SET once per row',
    json(cypher('MATCH (b:B108) RETURN b.id AS id, b.z AS z ORDER BY id'))
    = json('[{"id":2,"z":1},{"id":3,"z":1},{"id":5,"z":4},{"id":6,"z":null}]');

SELECT cypher('MATCH (a:A108) MATCH (a)-->(b) CREATE (:N108 {a: a.id, b: b.id})') as w2;
INSERT INTO _assert SELECT 'CREATE once per row',
    json(cypher('MATCH (n:N108) RETURN n.a AS a, n.b AS b ORDER BY a, b'))
    = json('[{"a":1,"b":2},{"a":1,"b":3},{"a":4,"b":5}]');

SELECT cypher('MATCH (a:A108 {id: 4}) MATCH (a)-->(b) REMOVE b.z') as w3;
INSERT INTO _assert SELECT 'REMOVE after two MATCH clauses',
    json(cypher('MATCH (b:B108 {id: 5}) RETURN b.z AS z')) = json('[{"z":null}]');

SELECT cypher('MATCH (n:B108 {id: 6}) OPTIONAL MATCH (n)-->(m) SET m.q = 1') as w4;
SELECT cypher('MATCH (n:X108) OPTIONAL MATCH (n)-->(m) SET m.q = 1') as w5;
SELECT cypher('MATCH (n:B108 {id: 6}) OPTIONAL MATCH (n)-->(m) WITH n, m SET m.q = 1, m:Hit') as w6;
INSERT INTO _assert SELECT 'SET through a null variable does nothing',
    json(cypher('MATCH (n) WHERE n.q IS NOT NULL OR n:Hit RETURN count(n) AS c'))
    = json('[{"c":0}]');

SELECT cypher('MATCH (n:B108 {id: 6}) OPTIONAL MATCH (n)-->(m) CREATE (:M108 {n: n.id, m: m.id})') as w7;
INSERT INTO _assert SELECT 'CREATE after OPTIONAL MATCH sees the first MATCH',
    json(cypher('MATCH (x:M108) RETURN x.n AS n, x.m AS m')) = json('[{"n":6,"m":null}]');

SELECT cypher('MATCH (a:A108 {id: 1}) MATCH (a)-[r]->(b) DELETE r') as w8;
INSERT INTO _assert SELECT 'DELETE after two MATCH clauses',
    json(cypher('MATCH (:A108 {id: 1})-[r]->() RETURN count(r) AS c')) = json('[{"c":0}]');

SELECT 'Test 108 passed: ' || count(*) || ' assertions' AS result FROM _assert;
