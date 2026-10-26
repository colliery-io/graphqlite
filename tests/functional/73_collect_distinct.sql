-- ========================================================================
-- Test 73: collect(DISTINCT x) keeps each value once (GQLITE-T-0435, #152)
-- ========================================================================
-- PURPOSE: collect() ignored DISTINCT: collect(DISTINCT b.n) gave
--          ["x","x"], and UNWIND [1, 1, 2] gave [1, 1, 2]. count(DISTINCT)
--          was right. Found by a user (#152) and the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 73: collect(DISTINCT) ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A73)-[:R73]->(:B73 {n: ''x'', b: true, l: [1]}), (a)-[:R73]->(:B73 {n: ''x'', b: true, l: [1]}), (a)-[:R73]->(:B73 {n: ''y'', b: false, l: [2]}), (a)-[:R73]->(:B73)') as setup;

INSERT INTO _assert SELECT '#152: collect(DISTINCT b.n) after MATCH',
    json(cypher('MATCH (:A73)-[:R73]->(b:B73) RETURN collect(DISTINCT b.n) AS v')) = json('[{"v":["x","y"]}]');
INSERT INTO _assert SELECT 'booleans and lists',
    json(cypher('MATCH (:A73)-[:R73]->(b:B73) RETURN collect(DISTINCT b.b) AS v, collect(DISTINCT b.l) AS l')) = json('[{"v":[true,false],"l":[[1],[2]]}]');
INSERT INTO _assert SELECT 'scalars from UNWIND, nulls dropped',
    json(cypher('UNWIND [1, 1, 2, null] AS x RETURN collect(DISTINCT x) AS c')) = json('[{"c":[1,2]}]');
INSERT INTO _assert SELECT 'entities by identity',
    json(cypher('MATCH (a:A73)-[:R73]->(b) MATCH (a)-[:R73]->(c) RETURN size(collect(DISTINCT a)) AS na, size(collect(DISTINCT b)) AS nb')) = json('[{"na":1,"nb":4}]');
INSERT INTO _assert SELECT 'in WITH',
    json(cypher('MATCH (a:A73)-[:R73]->(b:B73) WITH a, collect(DISTINCT b.n) AS ns RETURN ns')) = json('[{"ns":["x","y"]}]');
INSERT INTO _assert SELECT 'collect without DISTINCT keeps duplicates',
    json(cypher('MATCH (:A73)-[:R73]->(b:B73) RETURN collect(b.n) AS v')) = json('[{"v":["x","x","y"]}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 73 Complete ===' as test_section;
