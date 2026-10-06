-- ========================================================================
-- Test 53: row-wise write pipeline (GQLITE-T-0371)
-- ========================================================================
-- PURPOSE: MATCH/WITH/UNWIND prefixes drive DELETE / MERGE / WITH / RETURN
--          per row: DELETEs run for all rows before any MERGE (eager),
--          scalar WITH projections feed MERGE property maps (with
--          expressions and list values), WITH rename chains carry entities,
--          and a double UNWIND + CREATE creates the cartesian product.
-- COVERS:  openCypher TCK Merge1 [9]/[14], Merge5 [14]/[18]/[19]/[21].
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 53: row-wise write pipeline ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- Double UNWIND + CREATE (Merge1 [9] setup).
SELECT cypher('UNWIND [0, 1, 2] AS x UNWIND [0, 1, 2] AS y CREATE (:P53 {x: x, y: y})') as setup;
INSERT INTO _assert SELECT 'T-0371 1.1 double UNWIND CREATE makes 9 nodes',
    json_extract(cypher('MATCH (n:P53) RETURN count(n) AS c, count(n.y) AS cy'), '$[0].c') = 9
    AND json_extract(cypher('MATCH (n:P53) RETURN count(n) AS c, count(n.y) AS cy'), '$[0].cy') = 9;

-- Scalar WITH projections into MERGE property maps with expressions (Merge1 [9]).
INSERT INTO _assert SELECT 'T-0371 1.2 WITH scalars drive MERGE, one row per input row',
    json_array_length(cypher('MATCH (foo:P53) WITH foo.x AS x, foo.y AS y MERGE (:N53 {x: x, y: y + 1}) MERGE (:N53 {x: x, y: y}) MERGE (:N53 {x: x + 1, y: y}) RETURN x, y')) = 9;
INSERT INTO _assert SELECT 'T-0371 1.3 MERGE created exactly the distinct pairs',
    json_extract(cypher('MATCH (n:N53) RETURN count(n) AS c'), '$[0].c') = 15;

-- DELETE then MERGE: deletes are eager, MERGE cannot see deleted nodes (Merge1 [14]).
SELECT cypher('CREATE (:A53 {num: 1}), (:A53 {num: 2})') as setup2;
INSERT INTO _assert SELECT 'T-0371 2.1 DELETE+MERGE returns one row per input row',
    json(cypher('MATCH (a:A53) DELETE a MERGE (a2:A53) RETURN a2.num AS n')) = json('[{"n":null},{"n":null}]');
INSERT INTO _assert SELECT 'T-0371 2.2 exactly one A53 remains',
    json_extract(cypher('MATCH (a:A53) RETURN count(a) AS c'), '$[0].c') = 1;

-- DELETE relationships then MERGE a new one (Merge5 [21]).
SELECT cypher('CREATE (a:B53), (b:C53) CREATE (a)-[:T53 {name: "rel1"}]->(b), (a)-[:T53 {name: "rel2"}]->(b)') as setup3;
INSERT INTO _assert SELECT 'T-0371 2.3 DELETE rels then MERGE one',
    json(cypher('MATCH (a:B53)-[t:T53]->(b:C53) DELETE t MERGE (a)-[t2:T53 {name: "rel3"}]->(b) RETURN t2.name AS n')) = json('[{"n":"rel3"},{"n":"rel3"}]');
INSERT INTO _assert SELECT 'T-0371 2.4 exactly one T53 remains',
    json_extract(cypher('MATCH (:B53)-[t:T53]->(:C53) RETURN count(t) AS c'), '$[0].c') = 1;

-- WITH rename chain across MERGEs (Merge5 [18]).
SELECT cypher('CREATE (:D53 {id: 0})') as setup4;
INSERT INTO _assert SELECT 'T-0371 3.1 rename chain',
    json(cypher('MATCH (n:D53) MATCH (m:D53) WITH n AS a, m AS b MERGE (a)-[:R53]->(b) WITH a AS x, b AS y MERGE (a:D53) MERGE (b:D53) MERGE (a)-[:R53]->(b) RETURN x.id AS x, y.id AS y')) = json('[{"x":0,"y":0}]');
INSERT INTO _assert SELECT 'T-0371 3.2 rename chain created one relationship',
    json_extract(cypher('MATCH (:D53)-[r:R53]->(:D53) RETURN count(r) AS c'), '$[0].c') = 1;

-- CREATE ... WITH ... UNWIND ... WITH split() ... MERGE with a list property (Merge5 [14]).
INSERT INTO _assert SELECT 'T-0371 4.1 list property from WITH into MERGE',
    json_extract(cypher('CREATE (a:F53), (b:G53) WITH a, b UNWIND ["a,b", "a,b"] AS str WITH a, b, split(str, ",") AS roles MERGE (a)-[r:FB53 {foobar: roles}]->(b) RETURN count(*) AS c'), '$[0].c') = 2;
INSERT INTO _assert SELECT 'T-0371 4.2 one FB53 relationship with the list value',
    json_extract(cypher('MATCH (:F53)-[r:FB53]->(:G53) RETURN count(r) AS c, r.foobar AS fb'), '$[0].c') = 1
    AND json(json_extract(cypher('MATCH (:F53)-[r:FB53]->(:G53) RETURN r.foobar AS fb'), '$[0].fb')) = json('["a","b"]');

SELECT name, ok FROM _assert;
SELECT '=== Test 53 Complete ===' as test_section;
