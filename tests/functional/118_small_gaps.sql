-- ========================================================================
-- Test 118: percentileDisc type, COUNT { }, EXISTS { } in RETURN,
--           WITH *, items, n:A|B as an expression (GQLITE-T-0485)
-- ========================================================================
-- PURPOSE: percentileDisc gave 2.0 for integer input; COUNT { ... },
--          WITH *, a.v AS x and n:A|B as an expression did not parse;
--          EXISTS { pattern } was rejected in RETURN and WITH.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 118: small gaps ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A118 {v: 1, id: 1})-[:R]->(:A118 {v: 3, id: 2}), (:B118 {v: 2, id: 3}), (:C118 {id: 4})') as setup;
SELECT cypher('MATCH (a:A118 {id: 1}), (b:B118) CREATE (a)-[:R]->(b)') as setup2;

INSERT INTO _assert SELECT 'percentileDisc keeps the input type',
    json(cypher('MATCH (n) WHERE n.v IS NOT NULL RETURN percentileDisc(n.v, 0.5) AS p, percentileDisc(n.v * 1.0, 0.5) AS f'))
    = json('[{"p":2,"f":2.0}]');
INSERT INTO _assert SELECT 'COUNT { pattern }, with WHERE, with MATCH',
    json(cypher('MATCH (n:A118 {id: 1}) RETURN COUNT { (n)-->() } AS a, COUNT { (n)-->(m) WHERE m.v > 2 } AS b, count { MATCH (n)-->(m) RETURN m } AS c'))
    = json('[{"a":2,"b":1,"c":2}]');
INSERT INTO _assert SELECT 'COUNT { } in WHERE and WITH',
    json(cypher('MATCH (n) WHERE COUNT { (n)--() } > 1 WITH n, COUNT { (n)-->() } AS o RETURN n.id AS id, o'))
    = json('[{"id":1,"o":2}]');
INSERT INTO _assert SELECT 'EXISTS { pattern } in RETURN and WITH',
    json(cypher('MATCH (n:A118) WITH n, EXISTS { (n)-->(:B118) } AS b RETURN n.id AS id, b, EXISTS { (n)-->() } AS o ORDER BY id'))
    = json('[{"id":1,"b":true,"o":true},{"id":2,"b":false,"o":false}]');
INSERT INTO _assert SELECT 'WITH *, items',
    json(cypher('MATCH (a:A118)-[r]->(b:A118) WITH *, a.v + b.v AS s RETURN a.id AS a, b.id AS b, type(r) AS t, s'))
    = json('[{"a":1,"b":2,"t":"R","s":4}]');
INSERT INTO _assert SELECT 'n:A|B as an expression',
    json(cypher('MATCH (n) WHERE n.id IS NOT NULL RETURN n.id AS id, n:A118|B118 AS x, NOT n:B118|C118 AS y ORDER BY id'))
    = json('[{"id":1,"x":true,"y":true},{"id":2,"x":true,"y":true},{"id":3,"x":true,"y":false},{"id":4,"x":false,"y":false}]');
INSERT INTO _assert SELECT 'n:A|B in WHERE',
    json(cypher('MATCH (n) WHERE n:B118|C118 RETURN count(n) AS c'))
    = json('[{"c":2}]');
INSERT INTO _assert SELECT 'a comprehension keeps its projection',
    json(cypher('UNWIND [1, 2] AS i RETURN [x IN [i] WHERE x > 1 | x * 10] AS l'))
    = json('[{"l":[]},{"l":[20]}]');

SELECT 'Test 118 passed: ' || count(*) || ' assertions' AS result FROM _assert;
