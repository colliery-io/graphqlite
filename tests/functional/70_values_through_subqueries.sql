-- ========================================================================
-- Test 70: booleans, lists and entities read through scalar subqueries
-- ========================================================================
-- PURPOSE: from SQLite 3.54.1 a scalar subquery drops the subtype of its
--          value, so booleans and lists read by a subquery came back as text
--          ("true", "[1,2]") and entities lost their labels / properties
--          objects (GQLITE-T-0453, GQLITE-A-0007). The CI job sqlite-latest
--          runs this file on that SQLite; on older ones it passes either way.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 70: values through subqueries ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:I70 {b: true, l: [1, 2], s: ''x''})-[:R70 {f: false, w: [3]}]->(c:J70 {n: 1})') as setup;

INSERT INTO _assert SELECT 'boolean and list properties',
    json(cypher('MATCH (a:I70) RETURN a.b AS b, a.l AS l')) = json('[{"b":true,"l":[1,2]}]');
INSERT INTO _assert SELECT 'relationship boolean and list properties',
    json(cypher('MATCH ()-[r:R70]->() RETURN r.f AS f, r.w AS w')) = json('[{"f":false,"w":[3]}]');
INSERT INTO _assert SELECT 'properties nested in a list',
    json(cypher('MATCH (a:I70) RETURN [a.b, a.l] AS v')) = json('[{"v":[true,[1,2]]}]');
INSERT INTO _assert SELECT 'node object',
    json_extract(cypher('MATCH (a:I70) RETURN a'), '$[0].a.labels[0]') = 'I70'
    AND json_extract(cypher('MATCH (a:I70) RETURN a'), '$[0].a.properties.b') = 1
    AND json_type(cypher('MATCH (a:I70) RETURN a'), '$[0].a.properties.l') = 'array';
INSERT INTO _assert SELECT 'relationship object',
    json_extract(cypher('MATCH ()-[r:R70]->() RETURN r'), '$[0].r.type') = 'R70'
    AND json_type(cypher('MATCH ()-[r:R70]->() RETURN r'), '$[0].r.properties.f') = 'false';
INSERT INTO _assert SELECT 'endpoint property and labels',
    json(cypher('MATCH ()-[r:R70]->() RETURN startNode(r).b AS b, labels(endNode(r)) AS l')) = json('[{"b":true,"l":["J70"]}]');
INSERT INTO _assert SELECT 'collect of entities keeps objects',
    json_extract(cypher('MATCH (n:I70) WITH collect(n) AS ns RETURN [x IN ns | x] AS v'), '$[0].v[0].labels[0]') = 'I70';
INSERT INTO _assert SELECT 'keys, labels, split, slice',
    json(cypher('MATCH (a:I70) RETURN keys(a) AS k, labels(a) AS l, split(''a,b'', '','') AS s, a.l[0..1] AS sl')) = json('[{"k":["b","l","s"],"l":["I70"],"s":["a","b"],"sl":[1]}]');
INSERT INTO _assert SELECT 'path nodes and relationships',
    json_extract(cypher('MATCH p = (:I70)-->() RETURN nodes(p) AS n, relationships(p) AS r'), '$[0].n[1].labels[0]') = 'J70'
    AND json_extract(cypher('MATCH p = (:I70)-->() RETURN nodes(p) AS n, relationships(p) AS r'), '$[0].r[0].type') = 'R70';
INSERT INTO _assert SELECT 'pattern comprehension',
    json(cypher('MATCH (a:I70) RETURN [(a)-->(m) | m.n] AS v')) = json('[{"v":[1]}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 70 Complete ===' as test_section;
