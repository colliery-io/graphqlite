-- ========================================================================
-- Test 88: OPTIONAL MATCH with a WHERE on outer variables, and an
--          unanchored OPTIONAL path (GQLITE-T-0470)
-- ========================================================================
-- PURPOSE: an OPTIONAL MATCH gives, for each outer row, its matches that
--          pass the WHERE or one null row.
--          - A path with no bound node was joined node by node, so each
--            start node that failed the WHERE gave its own null row. It now
--            goes through the derived table: the whole path, then joined.
--          - A WHERE that names no path variable went only into the last
--            join: each edge gave a null row, and a named relationship
--            stayed bound. It now also filters the edge join.
--          Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 88: OPTIONAL MATCH WHERE on outer variables ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A88 {n: ''a''})') as setup1;
SELECT cypher('CREATE (x:A88 {n: ''b''})-[:S88]->(:Q88 {n: ''q1''})-[:T88]->(:O88 {n: ''o1''})') as setup2;
SELECT cypher('CREATE (c:C88 {n: ''c''})-[:S88]->(:Q88 {n: ''q2''}), (c)-[:S88]->(:Q88 {n: ''q3''})') as setup3;

INSERT INTO _assert SELECT 'unanchored path, WHERE through a pattern predicate',
    json(cypher('MATCH (a:A88) OPTIONAL MATCH (q)-[:T88]->(o) WHERE (a)-[:S88]->(q) RETURN a.n AS n, o.n AS o ORDER BY n'))
    = json('[{"n":"a","o":null},{"n":"b","o":"o1"}]');
INSERT INTO _assert SELECT 'unanchored path, WHERE on both',
    json(cypher('MATCH (a:A88) OPTIONAL MATCH (q)-[:T88]->(o) WHERE q.n = ''q1'' AND a.n = ''b'' RETURN a.n AS n, o.n AS o ORDER BY n'))
    = json('[{"n":"a","o":null},{"n":"b","o":"o1"}]');
INSERT INTO _assert SELECT 'unanchored path, WHERE on the outer row only',
    json(cypher('MATCH (a:A88) OPTIONAL MATCH (q)-[:T88]->(o) WHERE a.n = ''b'' RETURN a.n AS n, q.n AS q, o.n AS o ORDER BY n'))
    = json('[{"n":"a","q":null,"o":null},{"n":"b","q":"q1","o":"o1"}]');
INSERT INTO _assert SELECT 'unanchored path with labels',
    json(cypher('MATCH (a:A88) OPTIONAL MATCH (q:Q88)-[:T88]->(o:O88) WHERE a.n = ''b'' RETURN a.n AS n, o.n AS o ORDER BY n'))
    = json('[{"n":"a","o":null},{"n":"b","o":"o1"}]');
INSERT INTO _assert SELECT 'anchored, a failing outer WHERE is one null row',
    json(cypher('MATCH (a:C88) OPTIONAL MATCH (a)-[:S88]->(q) WHERE a.n = ''zzz'' RETURN a.n AS n, q.n AS q'))
    = json('[{"n":"c","q":null}]');
INSERT INTO _assert SELECT 'anchored, a named relationship is not bound',
    json(cypher('MATCH (a:C88) OPTIONAL MATCH (a)-[r:S88]->(q) WHERE a.n = ''zzz'' RETURN a.n AS n, type(r) AS t'))
    = json('[{"n":"c","t":null}]');
INSERT INTO _assert SELECT 'anchored, a passing outer WHERE keeps every match',
    json(cypher('MATCH (a:C88) OPTIONAL MATCH (a)-[:S88]->(q) WHERE a.n = ''c'' RETURN count(q) AS n'))
    = json('[{"n":2}]');

SELECT 'Test 88 passed: ' || count(*) || ' assertions' AS result FROM _assert;
