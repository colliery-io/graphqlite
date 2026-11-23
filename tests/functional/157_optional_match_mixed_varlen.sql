-- ========================================================================
-- Test 157: OPTIONAL MATCH of a pattern that mixes a variable-length
--           relationship with others (GQLITE-T-0556)
-- ========================================================================
-- PURPOSE: `OPTIONAL MATCH (a)-[:R*]->(b)-[:S]->(c)`: each relationship
--          had its own LEFT JOIN, so a later hop that failed left the row
--          of the walk (b bound, c null). The pattern is one unit: all of
--          it, or one null row. The walk now goes into the derived table
--          of the pattern with the other hops.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 157: OPTIONAL MATCH, walk and fixed hops ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (r:Res157 {id: 1}), (r)<-[:TOPIC]-(:Topic157 {name: ''n1''}),
                      (r)<-[:TOPIC]-(:Mid157 {name: ''mid''})<-[:TOPIC]-(:Topic157 {name: ''n2''}),
                      (:Res157 {id: 2})') as setup;

-- no match of the whole pattern: one null row ---------------------------------
INSERT INTO _assert SELECT 'walk, then a hop that does not exist',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:Topic157)<-[:NOPE]-(z) RETURN r.id AS r, t.name AS t, z ORDER BY r'))
    = json('[{"r":1,"t":null,"z":null},{"r":2,"t":null,"z":null}]');
INSERT INTO _assert SELECT 'hop, then a walk to a label no node has',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC]-(m)<-[:TOPIC*1..2]-(t:Nope157) RETURN r.id AS r, m.name AS m, t.name AS t ORDER BY r'))
    = json('[{"r":1,"m":null,"t":null},{"r":2,"m":null,"t":null}]');
INSERT INTO _assert SELECT 'two walks',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC*1..1]-(m)<-[:TOPIC*1..1]-(t:Nope157) RETURN r.id AS r, m.name AS m, t.name AS t ORDER BY r'))
    = json('[{"r":1,"m":null,"t":null},{"r":2,"m":null,"t":null}]');
INSERT INTO _assert SELECT 'a WHERE that no match passes',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(m)<-[:TOPIC]-(t) WHERE t.name = ''zzz'' RETURN r.id AS r, m.name AS m, t.name AS t ORDER BY r'))
    = json('[{"r":1,"m":null,"t":null},{"r":2,"m":null,"t":null}]');

-- matches ----------------------------------------------------------------------
INSERT INTO _assert SELECT 'hop, then walk',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC]-(m)<-[:TOPIC*1..2]-(t:Topic157) RETURN r.id AS r, m.name AS m, t.name AS t ORDER BY r'))
    = json('[{"r":1,"m":"mid","t":"n2"},{"r":2,"m":null,"t":null}]');
INSERT INTO _assert SELECT 'walk, then hop',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(m)<-[:TOPIC]-(t) RETURN r.id AS r, m.name AS m, t.name AS t ORDER BY r'))
    = json('[{"r":1,"m":"mid","t":"n2"},{"r":2,"m":null,"t":null}]');
INSERT INTO _assert SELECT 'a walk of zero hops',
    json(cypher('MATCH (r:Res157 {id: 1}) OPTIONAL MATCH (r)<-[:TOPIC*0..1]-(m)<-[:TOPIC]-(t) RETURN t.name AS t ORDER BY t'))
    = json('[{"t":"mid"},{"t":"n1"},{"t":"n2"}]');
INSERT INTO _assert SELECT 'both ends bound',
    json(cypher('MATCH (r:Res157), (t:Topic157 {name: ''n2''}) OPTIONAL MATCH (r)<-[:TOPIC*1..1]-(m)<-[:TOPIC]-(t) RETURN r.id AS r, m.name AS m ORDER BY r'))
    = json('[{"r":1,"m":"mid"},{"r":2,"m":null}]');
INSERT INTO _assert SELECT 'undirected: the walk does not use the relationship of the hop',
    json(cypher('MATCH (r:Res157 {id: 1}) OPTIONAL MATCH (r)-[:TOPIC*1..2]-(m)-[:TOPIC]-(t) RETURN m.name AS m, t.name AS t'))
    = json('[{"m":"mid","t":"n2"}]');
INSERT INTO _assert SELECT 'a named hop and an inline property',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC*]-(m)<-[e:TOPIC]-(t {name: ''n2''}) RETURN r.id AS r, m.name AS m, type(e) AS e ORDER BY r'))
    = json('[{"r":1,"m":"mid","e":"TOPIC"},{"r":2,"m":null,"e":null}]');

-- the walk has a variable, the pattern has a path variable ----------------------------
INSERT INTO _assert SELECT 'a walk variable: its relationships',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[w:TOPIC*1..2]-(m)<-[:TOPIC]-(t) RETURN r.id AS r, t.name AS t, size(w) AS n ORDER BY r'))
    = json('[{"r":1,"t":"n2","n":1},{"r":2,"t":null,"n":null}]');
INSERT INTO _assert SELECT 'a walk variable, no match',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[w:TOPIC*1..2]-(t:Topic157)<-[:NOPE]-(z) RETURN r.id AS r, t.name AS t, w IS NULL AS nw ORDER BY r'))
    = json('[{"r":1,"t":null,"nw":true},{"r":2,"t":null,"nw":true}]');
INSERT INTO _assert SELECT 'a path variable',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH p = (r)<-[:TOPIC*1..2]-(m)<-[:TOPIC]-(t) RETURN r.id AS r, length(p) AS l ORDER BY r'))
    = json('[{"r":1,"l":2},{"r":2,"l":null}]')
    AND json(cypher('MATCH (r:Res157 {id: 1}) OPTIONAL MATCH p = (r)<-[:TOPIC*1..2]-(m)<-[:TOPIC]-(t) RETURN [n IN nodes(p) | n.name] AS ns, size(relationships(p)) AS nr'))
    = json('[{"ns":[null,"mid","n2"],"nr":2}]');
INSERT INTO _assert SELECT 'a path variable, no match',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH p = (r)<-[:TOPIC*1..2]-(t:Topic157)<-[:NOPE]-(z) RETURN r.id AS r, t.name AS t, p IS NULL AS np ORDER BY r'))
    = json('[{"r":1,"t":null,"np":true},{"r":2,"t":null,"np":true}]');

-- with an earlier walk in the query (CTE names) ------------------------------------
INSERT INTO _assert SELECT 'after another OPTIONAL MATCH with a walk',
    json(cypher('MATCH (r:Res157) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:Topic157) OPTIONAL MATCH (r)<-[:TOPIC]-(m)<-[:TOPIC*1..2]-(u) RETURN r.id AS r, t.name AS t, m.name AS m, u.name AS u ORDER BY r, t'))
    = json('[{"r":1,"t":"n1","m":"mid","u":"n2"},{"r":1,"t":"n2","m":"mid","u":"n2"},{"r":2,"t":null,"m":null,"u":null}]');

SELECT 'Test 157 passed: ' || count(*) || ' assertions' AS result FROM _assert;
