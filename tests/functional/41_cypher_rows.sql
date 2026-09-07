-- ========================================================================
-- Test 41: cypher_rows table-valued interface (perf review F10, ADR A-0006)
-- ========================================================================
-- PURPOSE: SELECT ... FROM cypher_rows(query [, params]) exposes a Cypher
--          result as SQL rows: `row` is the JSON object cypher() would emit
--          for that row, c0..c31 carry native SQLite types, and the table
--          composes with LIMIT, WHERE, joins and json_each.
-- ASSERT:  json_extract('FAIL', '$') raises "malformed JSON" on mismatch so
--          the -bail runner exits non-zero.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 41: cypher_rows ===' as test_section;

SELECT cypher('CREATE (a:P {name: "Ann", age: 30, score: 1.5, ok: true})-[:KNOWS {since: 2020}]->(b:P {name: "Bob", age: 25})') AS setup;

SELECT 'Test 41.1 - positional columns carry native types:' as test_name;
WITH r AS (SELECT c0, typeof(c0) t0, c1, typeof(c1) t1, c2, typeof(c2) t2, c3, typeof(c3) t3
           FROM cypher_rows('MATCH (n:P {name: "Ann"}) RETURN n.name, n.age, n.score, n.ok'))
SELECT CASE WHEN c0 = 'Ann' AND t0 = 'text' AND c1 = 30 AND t1 = 'integer'
              AND c2 = 1.5 AND t2 = 'real' AND c3 = 1 AND t3 = 'integer'
            THEN 'PASS' ELSE json_extract('FAIL: ' || c0 || t0 || c1 || t1 || c2 || t2 || c3 || t3, '$') END AS result FROM r;

SELECT 'Test 41.2 - row is the cypher() row object; cols/ncols describe it:' as test_name;
WITH r AS (SELECT row, cols, ncols FROM cypher_rows('MATCH (n:P) RETURN n.name AS name, n.age AS age ORDER BY name') LIMIT 1)
SELECT CASE WHEN json_valid(row) AND json_extract(row, '$.name') = 'Ann' AND json_extract(row, '$.age') = 30
              AND cols = '["name","age"]' AND ncols = 2
            THEN 'PASS: ' || row ELSE json_extract('FAIL: ' || row || cols || ncols, '$') END AS result FROM r;

SELECT 'Test 41.3 - row matches cypher() output for entities:' as test_name;
WITH a AS (SELECT json_extract(cypher('MATCH (n:P)-[r]->(m) RETURN n, r, m'), '$[0]') AS via_cypher),
     b AS (SELECT row AS via_rows FROM cypher_rows('MATCH (n:P)-[r]->(m) RETURN n, r, m'))
SELECT CASE WHEN json(via_cypher) = json(via_rows) THEN 'PASS' ELSE json_extract('FAIL: ' || via_cypher || ' vs ' || via_rows, '$') END AS result FROM a, b;

SELECT 'Test 41.4 - params argument:' as test_name;
WITH r AS (SELECT group_concat(c0, ',') AS names FROM cypher_rows('MATCH (n:P) WHERE n.age >= $min RETURN n.name ORDER BY n.name', '{"min": 26}'))
SELECT CASE WHEN names = 'Ann' THEN 'PASS' ELSE json_extract('FAIL: ' || names, '$') END AS result FROM r;

SELECT 'Test 41.5 - SQL LIMIT / WHERE / self-join over the table:' as test_name;
WITH l AS (SELECT count(*) AS n FROM (SELECT c0 FROM cypher_rows('MATCH (n:P) RETURN n.name') LIMIT 1)),
     w AS (SELECT c0 FROM cypher_rows('MATCH (n:P) RETURN n.name') WHERE c0 = 'Bob'),
     j AS (SELECT count(*) AS n FROM cypher_rows('MATCH (n:P) RETURN n.name') t
           JOIN cypher_rows('MATCH (n:P) RETURN n.name') u ON t.c0 = u.c0)
SELECT CASE WHEN l.n = 1 AND w.c0 = 'Bob' AND j.n = 2 THEN 'PASS' ELSE json_extract('FAIL: ' || l.n || w.c0 || j.n, '$') END AS result FROM l, w, j;

SELECT 'Test 41.6 - write query without RETURN yields one statistics row:' as test_name;
WITH r AS (SELECT row, c0, c4, ncols FROM cypher_rows('CREATE (:P {name: "Cy"})'))
SELECT CASE WHEN json_extract(row, '$.nodes_created') = 1 AND c0 = 1 AND c4 = 1 AND ncols = 5
            THEN 'PASS: ' || row ELSE json_extract('FAIL: ' || row, '$') END AS result FROM r;

SELECT 'Test 41.7 - lists/maps arrive as JSON text usable by json_each:' as test_name;
WITH r AS (SELECT c0, typeof(c0) AS t FROM cypher_rows('RETURN [1, 2, 3] AS l'))
SELECT CASE WHEN t = 'text' AND (SELECT sum(value) FROM json_each(c0)) = 6 THEN 'PASS' ELSE json_extract('FAIL: ' || c0 || t, '$') END AS result FROM r;

SELECT 'Test 41.8 - zero rows and NULL cells:' as test_name;
WITH z AS (SELECT count(*) AS n FROM cypher_rows('MATCH (n:Nope) RETURN n')),
     u AS (SELECT c1, typeof(c1) AS t FROM cypher_rows('MATCH (n:P {name: "Bob"}) RETURN n.name, n.score'))
SELECT CASE WHEN z.n = 0 AND u.c1 IS NULL AND u.t = 'null' THEN 'PASS' ELSE json_extract('FAIL: ' || z.n || u.t, '$') END AS result FROM z, u;

SELECT 'Test 41.9 - repeated use hits the executor statement cache (same connection):' as test_name;
WITH a AS (SELECT count(*) AS n FROM cypher_rows('MATCH (n:P) RETURN n.name')),
     b AS (SELECT count(*) AS n FROM cypher_rows('MATCH (n:P) RETURN n.name'))
SELECT CASE WHEN a.n = 3 AND b.n = 3 THEN 'PASS' ELSE json_extract('FAIL: ' || a.n || b.n, '$') END AS result FROM a, b;

SELECT '=== Test 41 complete ===' as test_section;
