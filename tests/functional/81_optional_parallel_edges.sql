-- ========================================================================
-- Test 81: OPTIONAL MATCH with an anonymous relationship and parallel edges
--          (GQLITE-T-0465)
-- ========================================================================
-- PURPOSE: an OPTIONAL MATCH whose relationship had no variable was
--          checked with `c.id IN (SELECT ...)`: one row per node pair, so
--          two parallel edges gave one row. It now joins the edge, one
--          row per matching edge, as a named relationship does. Found by
--          the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 81: OPTIONAL MATCH parallel edges ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (n0:A81 {id: 0}), (n5:B81 {id: 5}), (n5)-[:S81 {w: 1}]->(n0), (n5)-[:S81 {w: 2}]->(n0)') as setup;

INSERT INTO _assert SELECT 'directed: one row per edge',
    json(cypher('MATCH (a) WHERE a:A81 OR a:B81 OPTIONAL MATCH (a)-[:S81]->(d) RETURN a.id AS aid, count(d) AS n ORDER BY aid'))
    = json('[{"aid":0,"n":0},{"aid":5,"n":2}]');
INSERT INTO _assert SELECT 'same count as a named relationship',
    json(cypher('MATCH (a) WHERE a:A81 OR a:B81 OPTIONAL MATCH (a)-[r:S81]->(d) RETURN a.id AS aid, count(d) AS n ORDER BY aid'))
    = json('[{"aid":0,"n":0},{"aid":5,"n":2}]');
INSERT INTO _assert SELECT 'undirected: both ends see both edges',
    json(cypher('MATCH (a) WHERE a:A81 OR a:B81 OPTIONAL MATCH (a)-[:S81]-(d) RETURN a.id AS aid, count(d) AS n ORDER BY aid'))
    = json('[{"aid":0,"n":2},{"aid":5,"n":2}]');
INSERT INTO _assert SELECT 'left arrow',
    json(cypher('MATCH (a:A81) OPTIONAL MATCH (a)<-[:S81]-(d) RETURN d.id AS did'))
    = json('[{"did":5},{"did":5}]');
INSERT INTO _assert SELECT 'no match keeps one null row',
    json(cypher('MATCH (a:A81) OPTIONAL MATCH (a)-[:S81]->(d) RETURN a.id AS aid, d.id AS did'))
    = json('[{"aid":0,"did":null}]');
INSERT INTO _assert SELECT 'WHERE on the far node keeps one null row',
    json(cypher('MATCH (a:B81) OPTIONAL MATCH (a)-[:S81]->(d) WHERE d.id = 9 RETURN a.id AS aid, d.id AS did'))
    = json('[{"aid":5,"did":null}]');
INSERT INTO _assert SELECT 'label on the far node',
    json(cypher('MATCH (a:B81) OPTIONAL MATCH (a)-[:S81]->(d:A81) RETURN count(d) AS n'))
    = json('[{"n":2}]');
INSERT INTO _assert SELECT 'wrong label on the far node keeps one null row',
    json(cypher('MATCH (a:B81) OPTIONAL MATCH (a)-[:S81]->(d:B81) RETURN a.id AS aid, d.id AS did'))
    = json('[{"aid":5,"did":null}]');

SELECT cypher('CREATE (l:L81 {id: 7})-[:S81]->(l)') as setup_loop;
INSERT INTO _assert SELECT 'undirected self-loop matches once',
    json(cypher('MATCH (a:L81) OPTIONAL MATCH (a)-[:S81]-(d) RETURN count(d) AS n'))
    = json('[{"n":1}]');

SELECT 'Test 81 passed: ' || count(*) || ' assertions' AS result FROM _assert;
