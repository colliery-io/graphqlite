-- ========================================================================
-- Test 83: undirected variable-length pattern over a self-loop (GQLITE-T-0459)
-- ========================================================================
-- PURPOSE: an undirected var-length walk seeds each edge both ways round.
--          A self-loop reads the same both ways, so `-[*1..1]-` gave it
--          twice where `-[]-` gives it once (as the TCK). The reversed seed
--          now skips self-loops. Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 83: undirected var-length self-loop ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (n:A83 {id: 1})-[:S83]->(n), (n)-[:S83]->(:B83 {id: 2})') as setup;

INSERT INTO _assert SELECT '*1..1 gives the rows of -[]-',
    json(cypher('MATCH (a)-[:S83*1..1]-(b) RETURN a.id AS a, b.id AS b ORDER BY a, b'))
    = json(cypher('MATCH (a)-[:S83]-(b) RETURN a.id AS a, b.id AS b ORDER BY a, b'));
INSERT INTO _assert SELECT '*1..1 rows',
    json(cypher('MATCH (a)-[:S83*1..1]-(b) RETURN a.id AS a, b.id AS b ORDER BY a, b'))
    = json('[{"a":1,"b":1},{"a":1,"b":2},{"a":2,"b":1}]');
INSERT INTO _assert SELECT '*2..2 gives the rows of two fixed hops',
    json(cypher('MATCH (a)-[:S83*2..2]-(b) RETURN a.id AS a, b.id AS b ORDER BY a, b'))
    = json(cypher('MATCH (a)-[:S83]-()-[:S83]-(b) RETURN a.id AS a, b.id AS b ORDER BY a, b'));
INSERT INTO _assert SELECT '*1..3 is the union of fixed lengths',
    json(cypher('MATCH (a)-[:S83*1..3]-(b) RETURN count(*) AS n'))
    = json('[{"n":5}]');
INSERT INTO _assert SELECT 'anchored start',
    json(cypher('MATCH (a:A83)-[:S83*1..2]-(b) RETURN count(*) AS n'))
    = json('[{"n":3}]');

SELECT 'Test 83 passed: ' || count(*) || ' assertions' AS result FROM _assert;
