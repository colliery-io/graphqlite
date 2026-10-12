-- ========================================================================
-- Test 54: regressions fixed in 0.9.3 and 0.9.4
-- ========================================================================
-- PURPOSE: GitHub #122-#125 and the bugs found with them (GQLITE-T-0377 to
--          GQLITE-T-0388), plus two Linux-only faults: the row-wise
--          pipeline freed strdup memory with sqlite3_free ("free(): invalid
--          pointer" with glibc), and SQLite 3.45 drops the JSON subtype
--          through CASE (nested lists in slices came back as strings).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 54: 0.9.3 / 0.9.4 regressions ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

-- #122: Dijkstra reads integer weights; parallel edges keep their weights.
SELECT cypher('CREATE (:W54 {id: "a"}), (:W54 {id: "b"}), (:W54 {id: "c"})') as setup1;
SELECT cypher('MATCH (a:W54 {id: "a"}), (b:W54 {id: "b"}) CREATE (a)-[:L {cost: 1}]->(b)') as setup2;
SELECT cypher('MATCH (b:W54 {id: "b"}), (c:W54 {id: "c"}) CREATE (b)-[:L {cost: 2}]->(c)') as setup3;
SELECT cypher('MATCH (a:W54 {id: "a"}), (c:W54 {id: "c"}) CREATE (a)-[:L {cost: 10}]->(c)') as setup4;
INSERT INTO _assert SELECT '#122 integer weights',
    json_extract(cypher('RETURN dijkstra("a", "c", "cost")'), '$[0].column_0.distance') = 3;
SELECT cypher('MATCH (a:W54 {id: "a"}), (c:W54 {id: "c"}) CREATE (a)-[:L {cost: 0.5}]->(c)') as setup5;
INSERT INTO _assert SELECT '#122 parallel edge uses its own weight',
    json_extract(cypher('RETURN dijkstra("a", "c", "cost")'), '$[0].column_0.distance') = 0.5;
SELECT cypher('MATCH (n:W54) DETACH DELETE n') as cleanup1;

-- #123: a bare boolean property is a condition.
SELECT cypher('CREATE (:P54 {name: "on", active: true}), (:P54 {name: "off", active: false})') as setup6;
INSERT INTO _assert SELECT '#123 CASE WHEN bare boolean',
    json(cypher('MATCH (p:P54) RETURN CASE WHEN p.active THEN "yes" ELSE "no" END AS r ORDER BY p.name')) = json('[{"r":"no"},{"r":"yes"}]');
INSERT INTO _assert SELECT '#123 WHERE bare boolean',
    json(cypher('MATCH (p:P54) WHERE p.active RETURN p.name AS n')) = json('[{"n":"on"}]');

-- #124: UNWIND map values reach CREATE properties with their types.
SELECT cypher('UNWIND $rows AS row CREATE (:I54 {id: row.id, v: row.v, f: row.f, b: row.b, e: row.v * 2})',
              '{"rows":[{"id":"x","v":1,"f":1.5,"b":true}]}') as setup7;
INSERT INTO _assert SELECT '#124 UNWIND map properties',
    json(cypher('MATCH (n:I54) RETURN n.id AS id, n.v AS v, n.f AS f, n.b AS b, n.e AS e')) = json('[{"id":"x","v":1,"f":1.5,"b":true,"e":2}]');

-- #125: PageRank keeps the rank of nodes without out-edges (scores sum to 1).
SELECT cypher('MATCH (n) DETACH DELETE n') as cleanup2;
SELECT cypher('CREATE (:R54 {id: "a"}), (:R54 {id: "b"})') as setup8;
SELECT cypher('MATCH (a:R54 {id: "a"}), (b:R54 {id: "b"}) CREATE (a)-[:L]->(b)') as setup9;
INSERT INTO _assert SELECT '#125 PageRank sums to 1',
    abs((SELECT sum(json_extract(value, '$.score')) FROM json_each(cypher('RETURN pageRank(0.85, 100)'), '$[0].column_0')) - 1.0) < 1e-4;
INSERT INTO _assert SELECT '#125 PageRank of the sink',
    abs((SELECT json_extract(value, '$.score') FROM json_each(cypher('RETURN pageRank(0.85, 100)'), '$[0].column_0')
         WHERE json_extract(value, '$.user_id') = 'b') - 0.649123) < 1e-4;

-- GQLITE-T-0386 / 0387: edge properties and booleans keep their types.
SELECT cypher('CREATE (:E54)-[:ER54 {w: 1}]->(:F54 {flags: [true, false]})') as setup10;
INSERT INTO _assert SELECT 'T-0386 edge property in arithmetic',
    json_extract(cypher('MATCH ()-[r:ER54]->() RETURN r.w + 1 AS p'), '$[0].p') = 2;
INSERT INTO _assert SELECT 'T-0387 boolean list in a comprehension',
    json(cypher('MATCH (f:F54) RETURN [x IN f.flags] AS l, f.flags[0] AS h')) = json('[{"l":[true,false],"h":true}]');

-- SQLite 3.45: nested lists in slices and comprehensions stay lists.
INSERT INTO _assert SELECT 'nested list slice',
    json(cypher('RETURN [5, [6, 7], [8, 9], 10][1..3] AS s')) = json('[{"s":[[6,7],[8,9]]}]');
INSERT INTO _assert SELECT 'nested list comprehension',
    json(cypher('RETURN [x IN [[1], {k: 1}, "a", null, true] | x] AS c')) = json('[{"c":[[1],{"k":1},"a",null,true]}]');

-- GQLITE-T-0388 and the row-wise free(): UNWIND of numbers before MATCH ... CREATE.
SELECT cypher('CREATE (:A54 {id: "0"})') as setup11;
INSERT INTO _assert SELECT 'T-0388 UNWIND + MATCH + CREATE',
    json(cypher('UNWIND [1, 2] AS i MATCH (n:A54) CREATE (n)-[:S54 {i: i}]->(m:M54 {i: i}) RETURN m.i AS mi ORDER BY mi')) = json('[{"mi":1},{"mi":2}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 54 Complete ===' as test_section;
