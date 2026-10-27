-- ========================================================================
-- Test 78: OPTIONAL MATCH with a named undirected relationship (GQLITE-T-0449)
-- ========================================================================
-- PURPOSE: the far node was always read as the edge's target_id, so only
--          outgoing edges were found: `MATCH (a) OPTIONAL MATCH (a)-[p]-(d)`
--          gave d = null for a node with only incoming edges. Found by the
--          metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 78: OPTIONAL MATCH undirected named ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:X78 {id: 5})-[:S78 {w: 1}]->(:X78 {id: 1}), (l:X78 {id: 9})-[:S78 {w: 2}]->(l), (:X78 {id: 7})') as setup;

INSERT INTO _assert SELECT 'both directions, a self-loop once, a null row',
    json(cypher('MATCH (a:X78) OPTIONAL MATCH (a)-[p]-(d) RETURN a.id AS a, d.id AS d, p.w AS w ORDER BY a'))
    = json('[{"a":1,"d":5,"w":1},{"a":5,"d":1,"w":1},{"a":7,"d":null,"w":null},{"a":9,"d":9,"w":2}]');
INSERT INTO _assert SELECT 'WHERE on the far node',
    json(cypher('MATCH (a:X78) OPTIONAL MATCH (a)-[p:S78]-(d:X78) WHERE d.id > 2 RETURN a.id AS a, d.id AS d ORDER BY a'))
    = json('[{"a":1,"d":5},{"a":5,"d":null},{"a":7,"d":null},{"a":9,"d":9}]');
INSERT INTO _assert SELECT 'the relationship keeps its stored direction',
    json(cypher('MATCH (a:X78 {id: 1}) OPTIONAL MATCH (a)-[p]-(d) RETURN startNode(p).id AS s, endNode(p).id AS e'))
    = json('[{"s":5,"e":1}]');
INSERT INTO _assert SELECT 'directed forms unchanged',
    json(cypher('MATCH (a:X78 {id: 1}) OPTIONAL MATCH (a)-[p]->(d) RETURN d.id AS d')) = json('[{"d":null}]')
    AND json(cypher('MATCH (a:X78 {id: 1}) OPTIONAL MATCH (a)<-[p]-(d) RETURN d.id AS d')) = json('[{"d":5}]');

SELECT cypher('CREATE (:A78 {num: 1})-[:T78 {name: ''r1''}]->(:B78 {num: 2})-[:T78 {name: ''r2''}]->(:C78 {num: 3})') as setup2;
INSERT INTO _assert SELECT 'WHERE on the relationship filters the edge (Match7 [11])',
    json(cypher('MATCH (a)-[r:T78 {name: ''r1''}]-(b) OPTIONAL MATCH (b)-[r2:T78]-(c) WHERE r <> r2 RETURN a.num AS a, c.num AS c ORDER BY a'))
    = json('[{"a":1,"c":3},{"a":2,"c":null}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 78 Complete ===' as test_section;
