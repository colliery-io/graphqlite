-- ========================================================================
-- Test 55: inline property expressions, stored booleans, FOREACH entities,
--          CREATE after UNWIND
-- ========================================================================
-- PURPOSE: GQLITE-T-0421 to GQLITE-T-0424.
--          0421: an inline property with an expression value ({id: i})
--          was dropped, so the pattern matched every node.
--          0422: a stored boolean was equal to 1 / 0.
--          0423: FOREACH over nodes(p) or a collected list could not write
--          the entities; FOREACH after CREATE did not see it.
--          0424: CREATE ... WITH ... UNWIND ... CREATE ran one CREATE.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 55: inline properties, booleans, FOREACH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- 0421: inline property values that are expressions filter.
SELECT cypher('CREATE (:I55 {id: "a", v: 1, f: true}), (:I55 {id: "b", v: 2}), (:I55 {id: "c", v: 3})') as setup1;
SELECT cypher('MATCH (a:I55 {id: "a"}), (b:I55 {id: "b"}), (c:I55 {id: "c"}) CREATE (a)-[:R55 {w: 1}]->(b), (a)-[:R55 {w: 2}]->(c)') as setup2;
INSERT INTO _assert SELECT '0421 UNWIND variable',
    json(cypher('UNWIND ["a", "b"] AS i MATCH (n:I55 {id: i}) RETURN n.id AS id ORDER BY id')) = json('[{"id":"a"},{"id":"b"}]');
INSERT INTO _assert SELECT '0421 arithmetic value',
    json(cypher('WITH 1 AS x MATCH (n:I55 {v: x + 1}) RETURN n.id AS id')) = json('[{"id":"b"}]');
INSERT INTO _assert SELECT '0421 boolean value',
    json(cypher('UNWIND [true] AS t MATCH (n:I55 {f: t}) RETURN n.id AS id')) = json('[{"id":"a"}]');
INSERT INTO _assert SELECT '0421 relationship value from a row',
    json(cypher('UNWIND [2] AS k MATCH (:I55)-[:R55 {w: k}]->(t) RETURN t.id AS id')) = json('[{"id":"c"}]');
SELECT cypher('UNWIND ["a"] AS i MATCH (n:I55 {id: i}) SET n.v = 9') as write1;
INSERT INTO _assert SELECT '0421 write changes only the matched node',
    json(cypher('MATCH (n:I55) RETURN n.v AS v ORDER BY n.id')) = json('[{"v":9},{"v":2},{"v":3}]');

-- 0422: a stored boolean is not a number.
SELECT cypher('CREATE (:B55 {flag: true, v: 1})') as setup3;
INSERT INTO _assert SELECT '0422 stored boolean vs number',
    json(cypher('MATCH (n:B55) RETURN [n.flag = 1, n.flag IN [1], n.v = true, 1 IN [true], n.flag = true, n.flag IN [true], n.v = 1] AS r'))
    = json('[{"r":[false,false,false,false,true,true,true]}]');

-- 0423: FOREACH over entities, and after CREATE.
SELECT cypher('CREATE (:F55 {name: "A"})-[:L55]->(:F55 {name: "B"})-[:L55]->(:F55 {name: "C"})') as setup4;
SELECT cypher('MATCH p = (:F55 {name: "A"})-[:L55*]->(:F55 {name: "C"}) FOREACH (n IN nodes(p) | SET n.seen = true)') as write2;
INSERT INTO _assert SELECT '0423 FOREACH over nodes(p)',
    json_extract(cypher('MATCH (n:F55) WHERE n.seen RETURN count(n) AS c'), '$[0].c') = 3;
SELECT cypher('MATCH (a:F55) WITH collect(a) AS ns FOREACH (n IN ns | SET n.k = 1)') as write3;
INSERT INTO _assert SELECT '0423 FOREACH over a collected list',
    json_extract(cypher('MATCH (n:F55) RETURN sum(n.k) AS s'), '$[0].s') = 3;
SELECT cypher('CREATE (x:X55) FOREACH (i IN [1, 2] | CREATE (x)-[:R55]->(:Y55 {i: i}))') as write4;
INSERT INTO _assert SELECT '0423 FOREACH after CREATE',
    json(cypher('MATCH (:X55)-[:R55]->(y:Y55) RETURN y.i AS i ORDER BY i')) = json('[{"i":1},{"i":2}]');

-- 0424: every CREATE after WITH / UNWIND runs.
SELECT cypher('CREATE (n:C55 {id: "a"}) WITH n UNWIND [1, 2] AS x CREATE (n)-[:R55]->(:D55 {v: x})') as write5;
INSERT INTO _assert SELECT '0424 CREATE WITH UNWIND CREATE',
    json(cypher('MATCH (:C55)-[:R55]->(d:D55) RETURN d.v AS v ORDER BY v')) = json('[{"v":1},{"v":2}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 55 Complete ===' as test_section;
