-- ========================================================================
-- Test 76: OPTIONAL MATCH with an anonymous relationship (GQLITE-T-0437, #154)
-- ========================================================================
-- PURPOSE: the target joined ON EXISTS (SELECT 1 FROM edges WHERE source_id
--          = <bound> AND target_id = c.id): every node was tested for every
--          outer row, so the query slowed down with the size of the graph.
--          It now joins ON c.id IN (SELECT target_id FROM edges WHERE
--          source_id = <bound>), driven by the edge index. The undirected
--          form also dropped a type list on its reversed side.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 76: OPTIONAL MATCH edge lookup ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (h:H76 {n: ''h''}), (a:A76 {n: ''a''})-[:S76]->(h), (b:A76 {n: ''b''}), (h)-[:T76]->(c:A76 {n: ''c''}), (x:X76 {n: ''x''})') as setup;

INSERT INTO _assert SELECT 'outgoing, with and without a match',
    json(cypher('MATCH (n:A76) OPTIONAL MATCH (n)-[:S76]->(m) RETURN n.n AS n, m.n AS m ORDER BY n'))
    = json('[{"n":"a","m":"h"},{"n":"b","m":null},{"n":"c","m":null}]');
INSERT INTO _assert SELECT 'incoming',
    json(cypher('MATCH (n:H76) OPTIONAL MATCH (n)<-[:S76]-(m) RETURN m.n AS m')) = json('[{"m":"a"}]');
INSERT INTO _assert SELECT 'undirected with a type list',
    json(cypher('MATCH (n:H76) OPTIONAL MATCH (n)-[:S76|T76]-(m) RETURN m.n AS m ORDER BY m')) = json('[{"m":"a"},{"m":"c"}]');
INSERT INTO _assert SELECT 'undirected, type list, reversed side only',
    json(cypher('MATCH (n:A76 {n: ''c''}) OPTIONAL MATCH (n)-[:S76|T76]-(m) RETURN m.n AS m')) = json('[{"m":"h"}]');
INSERT INTO _assert SELECT 'label on the optional node',
    json(cypher('MATCH (n:H76) OPTIONAL MATCH (n)-[]-(m:X76) RETURN m.n AS m')) = json('[{"m":null}]');
INSERT INTO _assert SELECT 'the edge lookup drives the join',
    json_extract(cypher('EXPLAIN MATCH (n:A76) OPTIONAL MATCH (n)-[:S76]->(m) RETURN m.n'), '$[0].column_0') LIKE '%.id IN (SELECT _e.target_id FROM edges _e WHERE _e.source_id = %';

SELECT name, ok FROM _assert;
SELECT '=== Test 76 Complete ===' as test_section;
