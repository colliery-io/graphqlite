-- ========================================================================
-- Test 148: EXPLAIN of MERGE, and FOREACH with MERGE (GQLITE-T-0541)
-- ========================================================================
-- PURPOSE: EXPLAIN of a query with MERGE failed with "Unsupported clause
--          type". It now gives the route, the lookup SQL and what the MERGE
--          does, and writes nothing.
--          The MATCH + MERGE and MERGE + WITH handlers ran only for shapes
--          with a FOREACH, where they skipped the FOREACH or refused the
--          query. They are removed; the row pipeline runs these shapes.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 148: EXPLAIN of MERGE, FOREACH with MERGE ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A148 {id: 1}), (:B148 {id: 2})') as setup;

-- EXPLAIN ---------------------------------------------------------------
INSERT INTO _assert SELECT 'EXPLAIN MATCH + MERGE: route, lookup SQL, MERGE line',
    x LIKE '%Pattern: ROW_PIPELINE%' AND x LIKE '%SQL: SELECT%edges%' AND
    x LIKE '%MERGE: the SQL finds the pattern for each row; a row with no match creates the pattern%'
    FROM (SELECT cypher('EXPLAIN MATCH (a:A148), (b:B148) MERGE (a)-[:R]->(b)') AS x);

INSERT INTO _assert SELECT 'EXPLAIN MERGE + WITH + MERGE',
    x LIKE '%Pattern: ROW_PIPELINE%' AND x LIKE '%SQL: SELECT%' AND x LIKE '%MERGE: %'
    FROM (SELECT cypher('EXPLAIN MERGE (a:A148) WITH a MERGE (a)-[:R]->(:C148)') AS x);

INSERT INTO _assert SELECT 'EXPLAIN MERGE with ON CREATE and ON MATCH',
    x LIKE '%then runs ON CREATE SET%' AND x LIKE '%a row with a match runs ON MATCH SET%'
    FROM (SELECT cypher('EXPLAIN MATCH (a:A148) MERGE (a)-[:R]->(b:B148) ON CREATE SET b.x = 1 ON MATCH SET b.y = 2') AS x);

INSERT INTO _assert SELECT 'EXPLAIN of a lone MERGE',
    x LIKE '%Pattern: MERGE%' AND x LIKE '%SQL: SELECT%'
    FROM (SELECT cypher('EXPLAIN MERGE (a:A148 {id: 5})') AS x);

INSERT INTO _assert SELECT 'EXPLAIN of a MERGE after a write has no lookup SQL',
    x LIKE '%SQL: (none:%' AND x LIKE '%MERGE: a lookup finds the pattern%'
    FROM (SELECT cypher('EXPLAIN CREATE (x:X148) MERGE (y:Y148)') AS x);

INSERT INTO _assert SELECT 'EXPLAIN wrote nothing',
    json(cypher('MATCH (n) WHERE n:A148 OR n:B148 OR n:C148 OR n:X148 OR n:Y148 RETURN count(n) AS c'))
    = json('[{"c":2}]')
    AND json(cypher('MATCH (:A148)-[r]->() RETURN count(r) AS c')) = json('[{"c":0}]');

-- PROFILE names the route that ran ----------------------------------------
INSERT INTO _assert SELECT 'PROFILE MATCH + MERGE names the row pipeline',
    json_extract(cypher('PROFILE MATCH (a:A148), (b:B148) MERGE (a)-[:P]->(b)'), '$[0].pattern') = 'ROW_PIPELINE';

-- FOREACH with MERGE ---------------------------------------------------------
SELECT cypher('MATCH (a:A148) FOREACH (x IN [1] | SET a.y = x) WITH a MATCH (b:B148) MERGE (a)-[:R]->(b)') as step1;
INSERT INTO _assert SELECT 'MATCH FOREACH WITH MATCH MERGE: the FOREACH runs',
    json(cypher('MATCH (a:A148)-[r:R]->(b:B148) RETURN a.y AS y, b.id AS id'))
    = json('[{"y":1,"id":2}]');

SELECT cypher('MERGE (a:A148 {id: 1}) WITH a FOREACH (x IN [1, 2] | CREATE (:Z148 {x: x}))') as step2;
INSERT INTO _assert SELECT 'MERGE WITH FOREACH',
    json(cypher('MATCH (z:Z148) RETURN z.x AS x ORDER BY x')) = json('[{"x":1},{"x":2}]');

SELECT cypher('FOREACH (x IN [1] | CREATE (:Y148)) MERGE (a:A148 {id: 1}) WITH a MERGE (c:C148 {id: a.id})') as step3;
INSERT INTO _assert SELECT 'leading FOREACH, then MERGE WITH MERGE',
    json(cypher('MATCH (n) WHERE n:Y148 OR n:C148 RETURN labels(n)[0] AS l, n.id AS id ORDER BY l'))
    = json('[{"l":"C148","id":1},{"l":"Y148","id":null}]');

INSERT INTO _assert SELECT 'leading FOREACH, then a read',
    json(cypher('FOREACH (x IN [1, 2] | CREATE (:W148 {x: x})) WITH 1 AS one MATCH (w:W148) RETURN w.x AS x ORDER BY x'))
    = json('[{"x":1},{"x":2}]');

SELECT 'Test 148 passed: ' || count(*) || ' assertions' AS result FROM _assert;
