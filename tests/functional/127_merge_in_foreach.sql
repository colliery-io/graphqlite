-- ========================================================================
-- Test 127: MERGE in FOREACH; FOREACH over a single value (GQLITE-T-0517)
-- ========================================================================
-- PURPOSE: A MERGE property that is an expression (`{x: x + 1}`) was
--          ignored: the MERGE matched any node and set nothing. It is now
--          evaluated, with the FOREACH values in scope, for the match and
--          for the create, on nodes and relationships. FOREACH over a
--          value that is not a list iterates once (Neo4j
--          ForeachAcceptance). Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 127: MERGE in FOREACH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));
CREATE TEMP VIEW _counts AS
    SELECT json(cypher('MATCH (n) OPTIONAL MATCH (n)-[r]->() RETURN count(DISTINCT n) AS n, count(r) AS r')) AS c;

SELECT cypher('WITH 3 AS y FOREACH(x IN [1, 2] | MERGE (a {x: x, y: y}) MERGE (b {x: x + 1, y: y}) MERGE (a)-[:FOO]->(b))');
INSERT INTO _assert SELECT 'expression values merge onto the same nodes',
    (SELECT c FROM _counts) = json('[{"n":3,"r":2}]')
    AND json(cypher('MATCH (n) RETURN n.x AS x, n.y AS y ORDER BY x'))
        = json('[{"x":1,"y":3},{"x":2,"y":3},{"x":3,"y":3}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

SELECT cypher('FOREACH(x IN [0, 1, 2] | FOREACH(y IN [0, 1, 2] | MERGE (a {x: x, y: y}) MERGE (b {x: x + 1, y: y}) MERGE (c {x: x, y: y + 1}) MERGE (d {x: x + 1, y: y + 1}) MERGE (a)-[:R]->(b) MERGE (a)-[:R]->(c) MERGE (b)-[:R]->(d) MERGE (c)-[:R]->(d)))');
INSERT INTO _assert SELECT 'nested FOREACH builds a 4x4 grid',
    (SELECT c FROM _counts) = json('[{"n":16,"r":24}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

SELECT cypher('FOREACH(x IN [1, 2] | MERGE (:A)-[:T {w: x * 10}]->(:B))');
INSERT INTO _assert SELECT 'expression value on a relationship',
    json(cypher('MATCH ()-[r:T]->() RETURN r.w AS w ORDER BY w')) = json('[{"w":10},{"w":20}]');
SELECT cypher('MATCH (n) DETACH DELETE n');

SELECT cypher('FOREACH(v IN 1 | MERGE ({p: v}))');
SELECT cypher('FOREACH(v IN true AND false XOR true | MERGE ({q: v}))');
SELECT cypher('FOREACH(v IN null | CREATE ({r: v}))');
INSERT INTO _assert SELECT 'a single value iterates once; null not at all',
    json(cypher('MATCH (n) RETURN properties(n) AS p ORDER BY keys(n)[0]'))
        = json('[{"p":{"p":1}},{"p":{"q":true}}]');

SELECT 'Test 127 passed: ' || count(*) || ' assertions' AS result FROM _assert;
