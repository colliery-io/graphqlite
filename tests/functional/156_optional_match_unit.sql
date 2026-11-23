-- ========================================================================
-- Test 156: OPTIONAL MATCH matches its pattern as one unit (GQLITE-T-0555)
-- ========================================================================
-- PURPOSE: an optional pattern gives all of its match, or one null row.
--          - a variable-length pattern with a label, a WHERE or an inline
--            property on its end node gave a null row for each walk that
--            ended elsewhere, next to the real matches; an inline property
--            was not applied at all
--          - the labels of a new START node of a variable-length pattern
--            (`OPTIONAL MATCH (x:L)<-[:T*]-(t)`) were not applied
--          - two hops with a label on the end and an anonymous middle node
--            gave one null row for each first hop
--          - a leading OPTIONAL MATCH of two hops with a named middle node
--            failed ("no such column")
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 156: OPTIONAL MATCH as one unit ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (r:Res156 {id: 1}), (r)<-[:TOPIC]-(:PT156:Topic156 {name: ''n1''}),
                      (r)<-[:TOPIC]-(:Mid156 {name: ''mid''})<-[:TOPIC]-(:PT156:Topic156 {name: ''n2''}),
                      (:Res156 {id: 2})') as setup;

-- a variable-length pattern, filter on the end node ---------------------------
INSERT INTO _assert SELECT 'label on the end node',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:Topic156) RETURN r.id AS r, t.name AS t ORDER BY r, t'))
    = json('[{"r":1,"t":"n1"},{"r":1,"t":"n2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'two labels',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:PT156:Topic156) RETURN r.id AS r, t.name AS t ORDER BY r, t'))
    = json('[{"r":1,"t":"n1"},{"r":1,"t":"n2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'label alternatives',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:Mid156|Nope156) RETURN r.id AS r, t.name AS t ORDER BY r, t'))
    = json('[{"r":1,"t":"mid"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'the label in WHERE',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t) WHERE t:Topic156 RETURN r.id AS r, t.name AS t ORDER BY r, t'))
    = json('[{"r":1,"t":"n1"},{"r":1,"t":"n2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'an inline property',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t {name: ''n2''}) RETURN r.id AS r, t.name AS t ORDER BY r, t'))
    = json('[{"r":1,"t":"n2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'a property in WHERE',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t) WHERE t.name = ''n2'' RETURN r.id AS r, t.name AS t ORDER BY r, t'))
    = json('[{"r":1,"t":"n2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'no walk ends at the label: one null row',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:Nope156) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":null},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'no filter: as before',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t) RETURN r.id AS r, count(t) AS c ORDER BY r'))
    = json('[{"r":1,"c":3},{"r":2,"c":0}]');
INSERT INTO _assert SELECT 'the path and the relationship list of the walk',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH p = (r)<-[e:TOPIC*1..2]-(t:Topic156) RETURN r.id AS r, length(p) AS l, size(e) AS s ORDER BY r, l'))
    = json('[{"r":1,"l":1,"s":1},{"r":1,"l":2,"s":2},{"r":2,"l":null,"s":null}]');

-- the new node is the START of the walk ---------------------------------------------
INSERT INTO _assert SELECT 'label on a new start node',
    json(cypher('MATCH (t:Topic156) OPTIONAL MATCH (x:Res156)<-[:TOPIC*1..2]-(t) RETURN t.name AS t, x.id AS x ORDER BY t'))
    = json('[{"t":"n1","x":1},{"t":"n2","x":1}]');
INSERT INTO _assert SELECT 'a label no start node has',
    json(cypher('MATCH (t:Topic156) OPTIONAL MATCH (x:Nope156)<-[:TOPIC*1..2]-(t) RETURN t.name AS t, x.id AS x ORDER BY t'))
    = json('[{"t":"n1","x":null},{"t":"n2","x":null}]');
INSERT INTO _assert SELECT 'a property and a WHERE on a new start node',
    json(cypher('MATCH (t:Topic156) OPTIONAL MATCH (x {id: 1})<-[:TOPIC*1..2]-(t) RETURN t.name AS t, x.id AS x ORDER BY t'))
    = json('[{"t":"n1","x":1},{"t":"n2","x":1}]')
    AND json(cypher('MATCH (t:Topic156) OPTIONAL MATCH (x)<-[:TOPIC*1..2]-(t) WHERE x:Res156 RETURN t.name AS t, x.id AS x ORDER BY t'))
    = json('[{"t":"n1","x":1},{"t":"n2","x":1}]');

-- two hops, anonymous middle node -----------------------------------------------------
INSERT INTO _assert SELECT 'a label no end node has: one null row',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC]-()<-[:TOPIC]-(t:Nope156) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":null},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'a label the end node has',
    json(cypher('MATCH (r:Res156) OPTIONAL MATCH (r)<-[:TOPIC]-()<-[:TOPIC]-(t:Topic156) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":"n2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'RETURN * shows no hidden name',
    (SELECT group_concat(key) FROM json_each(json_extract(
        cypher('MATCH (r:Res156 {id: 2}) OPTIONAL MATCH (r)<-[:TOPIC]-()<-[:TOPIC]-(t:Topic156) RETURN *'), '$[0]'))) = 'r,t';

-- a leading OPTIONAL MATCH of two hops ------------------------------------------------
INSERT INTO _assert SELECT 'named middle node',
    json(cypher('OPTIONAL MATCH (r:Res156)<-[:TOPIC]-(m:Mid156)<-[:TOPIC]-(t:Topic156) RETURN r.id AS r, m.name AS m, t.name AS t'))
    = json('[{"r":1,"m":"mid","t":"n2"}]');
INSERT INTO _assert SELECT 'no match: one null row',
    json(cypher('OPTIONAL MATCH (r:Res156)<-[:TOPIC]-(m:Mid156)<-[:TOPIC]-(t:Nope156) RETURN r.id AS r, m.name AS m, t.name AS t'))
    = json('[{"r":null,"m":null,"t":null}]');
INSERT INTO _assert SELECT 'anonymous nodes, DISTINCT',
    json(cypher('OPTIONAL MATCH (:Res156)<-[:TOPIC]-(:Mid156)<-[:TOPIC]-(t:Topic156) RETURN DISTINCT labels(t) AS l'))
    = json('[{"l":["PT156","Topic156"]}]');

SELECT 'Test 156 passed: ' || count(*) || ' assertions' AS result FROM _assert;
