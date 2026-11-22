-- ========================================================================
-- Test 155: two variable-length patterns in one query; inline properties
--           in OPTIONAL MATCH; WITH DISTINCT with an aggregate after a
--           CALL { } (GQLITE-T-0554)
-- ========================================================================
-- PURPOSE: - `MATCH (a)-[:R*]->(b) MATCH (a)-[:S*]->(c)`: the CTE of a
--            variable-length relationship was named by its position in
--            the pattern, so two clauses gave "duplicate WITH table name:
--            _varlen_path_1".
--          - `OPTIONAL MATCH (a)-[:R]->(b {k: v})`: the inline properties
--            of an optional pattern with a relationship were not applied;
--            every neighbour came back.
--          - `WITH DISTINCT k, collect(x)` after a CALL { } or a write was
--            refused.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 155: variable-length CTE names, OPTIONAL MATCH properties ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (r:Res155 {id: 1}), (r)<-[:TOPIC]-(:Topic155 {name: ''t''}),
                      (r)<-[:SUBJ]-(:Subj155 {name: ''s''})<-[:SUBJ]-(:Subj155 {name: ''s2''}),
                      (r)-[:OUT {w: 1}]->(:O155 {name: ''o1''}), (r)-[:OUT {w: 2}]->(:O155 {name: ''o2''}),
                      (:Res155 {id: 2})') as setup;

-- two variable-length patterns ----------------------------------------------
INSERT INTO _assert SELECT 'two MATCH clauses, each with a variable-length relationship',
    json(cypher('MATCH (r:Res155 {id: 1}) MATCH (r)<-[:TOPIC*1..2]-(t) MATCH (r)<-[:SUBJ*1..2]-(s) RETURN t.name AS t, s.name AS s ORDER BY s'))
    = json('[{"t":"t","s":"s"},{"t":"t","s":"s2"}]');
INSERT INTO _assert SELECT 'two OPTIONAL MATCH clauses',
    json(cypher('MATCH (r:Res155) OPTIONAL MATCH (r)<-[:TOPIC*1..2]-(t:Topic155) OPTIONAL MATCH (r)<-[:SUBJ*1..2]-(s:Subj155) RETURN r.id AS r, t.name AS t, s.name AS s ORDER BY r, s'))
    = json('[{"r":1,"t":"t","s":"s"},{"r":1,"t":"t","s":"s2"},{"r":2,"t":null,"s":null}]');
INSERT INTO _assert SELECT 'paths and relationship lists of both',
    json(cypher('MATCH p = (r:Res155 {id: 1})<-[a:SUBJ*1..2]-(s) MATCH q = (r)<-[b:TOPIC*]-(t) RETURN length(p) AS lp, length(q) AS lq, size(a) AS sa, size(b) AS sb ORDER BY lp'))
    = json('[{"lp":1,"lq":1,"sa":1,"sb":1},{"lp":2,"lq":1,"sa":2,"sb":1}]');
INSERT INTO _assert SELECT 'a second one after a WITH',
    json(cypher('MATCH (r:Res155 {id: 1})<-[:SUBJ*1..2]-(s) WITH r, s MATCH (s)<-[:SUBJ*0..1]-(u) RETURN s.name AS s, u.name AS u ORDER BY s, u'))
    = json('[{"s":"s","u":"s"},{"s":"s","u":"s2"},{"s":"s2","u":"s2"}]');

-- inline properties in OPTIONAL MATCH -------------------------------------------
INSERT INTO _assert SELECT 'a property of the new node',
    json(cypher('MATCH (r:Res155) OPTIONAL MATCH (r)-[:OUT]->(t {name: ''o1''}) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":"o1"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'no node has the property: null',
    json(cypher('MATCH (r:Res155) OPTIONAL MATCH (r)-[:OUT]->(t:O155 {name: ''zzz''}) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":null},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'the pattern written the other way, and undirected',
    json(cypher('MATCH (r:Res155) OPTIONAL MATCH (t {name: ''o2''})<-[:OUT]-(r) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":"o2"},{"r":2,"t":null}]')
    AND json(cypher('MATCH (r:Res155) OPTIONAL MATCH (r)-[:OUT]-(t {name: ''o2''}) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":"o2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'a property of the relationship',
    json(cypher('MATCH (r:Res155) OPTIONAL MATCH (r)-[e:OUT {w: 2}]->(t) RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":"o2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'an anonymous node with a property; RETURN * shows no hidden name',
    json(cypher('MATCH (r:Res155 {id: 1}) OPTIONAL MATCH (r)-[e:OUT]->({name: ''o1''}) RETURN e.w AS w')) = json('[{"w":1}]')
    AND (SELECT group_concat(key) FROM json_each(json_extract(
            cypher('MATCH (r:Res155 {id: 2}) OPTIONAL MATCH (r)-[:OUT]->({name: ''o1''}) RETURN *'), '$[0]'))) = 'r';
INSERT INTO _assert SELECT 'a property that reads a variable, with a WHERE as well',
    json(cypher('WITH ''o2'' AS want MATCH (r:Res155) OPTIONAL MATCH (r)-[:OUT]->(t {name: want}) WHERE t.name <> ''zz'' RETURN r.id AS r, t.name AS t ORDER BY r'))
    = json('[{"r":1,"t":"o2"},{"r":2,"t":null}]');
INSERT INTO _assert SELECT 'a property of a bound node that it does not have: nulls, the row stays',
    json(cypher('MATCH (r:Res155 {id: 1}) OPTIONAL MATCH (r {id: 2})-[:OUT]->(t) RETURN r.id AS r, t.name AS t')) = json('[{"r":1,"t":null}]');
INSERT INTO _assert SELECT 'a leading OPTIONAL MATCH',
    json(cypher('OPTIONAL MATCH (r:Res155)-[:OUT]->(t {name: ''o1''}) RETURN r.id AS r, t.name AS t')) = json('[{"r":1,"t":"o1"}]');
INSERT INTO _assert SELECT 'two hops, a property on each new node',
    json(cypher('MATCH (r:Res155 {id: 1}) OPTIONAL MATCH (r)<-[:SUBJ]-(m {name: ''s''})<-[:SUBJ]-(t {name: ''s2''}) RETURN m.name AS m, t.name AS t'))
    = json('[{"m":"s","t":"s2"}]');

-- WITH DISTINCT with an aggregate after a CALL { } ---------------------------------
INSERT INTO _assert SELECT 'WITH DISTINCT k, collect(x) after CALL { }',
    json(cypher('UNWIND [1, 1, 2] AS k CALL { WITH k RETURN k * 10 AS v } WITH DISTINCT k, collect(v) AS vs RETURN k, vs ORDER BY k'))
    = json('[{"k":1,"vs":[10,10]},{"k":2,"vs":[20]}]');

SELECT 'Test 155 passed: ' || count(*) || ' assertions' AS result FROM _assert;
