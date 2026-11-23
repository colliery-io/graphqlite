-- ========================================================================
-- Test 153: a relationship value as the relationship of a pattern, and a
--           bound relationship in OPTIONAL MATCH (GQLITE-T-0552)
-- ========================================================================
-- PURPOSE: - a variable that holds a relationship as a value (out of a
--            list or a map) was refused as the relationship of a pattern
--            ("VariableTypeConflict … bound to a value"). It now binds the
--            pattern to that relationship.
--          - OPTIONAL MATCH with a relationship bound by an earlier MATCH
--            joined its new nodes ON 1=1: `MATCH (a)-[e]->(b) OPTIONAL
--            MATCH (x)-[e]->(y)` gave every node as x. An optional pattern
--            whose variables are all bound dropped the row when it did
--            not match.
--          - datetime({epochMillis: …}) and datetime({epochSeconds: …})
--            ignored the keys and gave 1 January of this year.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 153: relationship values in patterns ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A153 {n: ''a''})-[:R153 {id: ''r1''}]->(b:B153 {n: ''b''}), (b)-[:R153 {id: ''r2''}]->(c:C153 {n: ''c''})') as setup;

-- a relationship value binds the pattern ----------------------------------------
INSERT INTO _assert SELECT 'UNWIND of a list of relationships, then MATCH',
    json(cypher('MATCH (:A153)-[r]->() WITH [r] AS rs UNWIND rs AS e MATCH (x)-[e]->(y) RETURN x.n AS x, e.id AS e, y.n AS y'))
    = json('[{"x":"a","e":"r1","y":"b"}]');
INSERT INTO _assert SELECT 'last(relationships(p)), then MATCH',
    json(cypher('MATCH p = (:A153)-[:R153*]->(n) WITH n, last(relationships(p)) AS edge MATCH (pn)-[edge]->(n) RETURN pn.n AS pn, n.n AS n, edge.id AS e ORDER BY n'))
    = json('[{"pn":"a","n":"b","e":"r1"},{"pn":"b","n":"c","e":"r2"}]');
INSERT INTO _assert SELECT 'the same with OPTIONAL MATCH and WITH *',
    json(cypher('MATCH p = (:A153)-[:R153*]->(n) WITH *, last(relationships(p)) AS edge OPTIONAL MATCH (pn)-[edge]->(n) RETURN pn.n AS pn, n.n AS n, edge.id AS e ORDER BY n'))
    = json('[{"pn":"a","n":"b","e":"r1"},{"pn":"b","n":"c","e":"r2"}]');
INSERT INTO _assert SELECT 'a relationship out of a map, undirected',
    json(cypher('MATCH (:A153)-[r]->() WITH {k: r} AS m WITH m.k AS e MATCH (x)-[e]-(y) RETURN x.n AS x, type(e) AS t, y.n AS y ORDER BY x'))
    = json('[{"x":"a","t":"R153","y":"b"},{"x":"b","t":"R153","y":"a"}]');
INSERT INTO _assert SELECT 'the variable is the relationship after the MATCH',
    json(cypher('MATCH (:A153)-[r]->() WITH [r] AS rs UNWIND rs AS e MATCH (x)-[e]->(y) RETURN startNode(e).n AS s, e.id AS i, type(e) AS t'))
    = json('[{"s":"a","i":"r1","t":"R153"}]');
INSERT INTO _assert SELECT 'a longer pattern from the bound relationship',
    json(cypher('MATCH (:A153)-[r]->() WITH [r] AS rs UNWIND rs AS e MATCH (x)-[e]->(y)-[f]->(z) RETURN x.n AS x, f.id AS f, z.n AS z'))
    = json('[{"x":"a","f":"r2","z":"c"}]');
INSERT INTO _assert SELECT 'a value that is not a relationship matches nothing',
    json(cypher('UNWIND [1, ''x'', null] AS e MATCH (x)-[e]->(y) RETURN count(*) AS c')) = json('[{"c":0}]');
INSERT INTO _assert SELECT 'OPTIONAL MATCH: a null value keeps its row',
    json(cypher('MATCH (:A153)-[r]->() WITH [r, null] AS rs UNWIND rs AS e OPTIONAL MATCH (x)-[e]->(y) RETURN x.n AS x, y.n AS y ORDER BY x'))
    = json('[{"x":"a","y":"b"},{"x":null,"y":null}]');
INSERT INTO _assert SELECT 'OPTIONAL MATCH with a label the start node does not have',
    json(cypher('MATCH (:A153)-[r]->() WITH [r] AS rs UNWIND rs AS e OPTIONAL MATCH (x:C153)-[e]->(y) RETURN x.n AS x, e.id AS e'))
    = json('[{"x":null,"e":"r1"}]');
INSERT INTO _assert SELECT 'a literal is still a VariableTypeConflict (Match2 [13])',
    json_extract(cypher_validate('WITH true AS r MATCH ()-[r]-() RETURN r'), '$.valid') IS NOT NULL;

-- a relationship of an earlier MATCH in OPTIONAL MATCH -----------------------------
INSERT INTO _assert SELECT 'new end nodes are the ends of the relationship',
    json(cypher('MATCH (:A153)-[e]->() OPTIONAL MATCH (x)-[e]->(y) RETURN x.n AS x, y.n AS y')) = json('[{"x":"a","y":"b"}]');
INSERT INTO _assert SELECT 'reversed: the ends swap',
    json(cypher('MATCH (:A153)-[e]->() OPTIONAL MATCH (x)<-[e]-(y) RETURN x.n AS x, y.n AS y')) = json('[{"x":"b","y":"a"}]');
INSERT INTO _assert SELECT 'undirected: both ways',
    json(cypher('MATCH (:A153)-[e]->() OPTIONAL MATCH (x)-[e]-(y) RETURN x.n AS x, y.n AS y ORDER BY x'))
    = json('[{"x":"a","y":"b"},{"x":"b","y":"a"}]');
INSERT INTO _assert SELECT 'a label the start node does not have: nulls',
    json(cypher('MATCH (:A153)-[e]->() OPTIONAL MATCH (x:C153)-[e]->(y) RETURN x.n AS x, e.id AS e')) = json('[{"x":null,"e":"r1"}]');
INSERT INTO _assert SELECT 'a bound start node that is not the start: null end',
    json(cypher('MATCH (a:A153)-[e]->(b) OPTIONAL MATCH (b)-[e]->(z) RETURN a.n AS a, z.n AS z')) = json('[{"a":"a","z":null}]');
INSERT INTO _assert SELECT 'a bound end node, reversed arrow',
    json(cypher('MATCH (a:A153)-[e]->(b) OPTIONAL MATCH (b)<-[e]-(z) RETURN a.n AS a, z.n AS z')) = json('[{"a":"a","z":"a"}]');
INSERT INTO _assert SELECT 'a WHERE that fails: nulls, the relationship stays',
    json(cypher('MATCH (:A153)-[e]->() OPTIONAL MATCH (x)-[e]->(y) WHERE y.n = ''zzz'' RETURN x.n AS x, e.id AS e')) = json('[{"x":null,"e":"r1"}]');
INSERT INTO _assert SELECT 'a longer optional pattern',
    json(cypher('MATCH (a)-[e:R153]->() OPTIONAL MATCH (x)-[e]->(y)-[f]->(z) RETURN a.n AS a, x.n AS x, f.id AS f ORDER BY a'))
    = json('[{"a":"a","x":"a","f":"r2"},{"a":"b","x":null,"f":null}]');
INSERT INTO _assert SELECT 'all variables bound, no match: the row stays',
    json(cypher('MATCH (a:A153)-[e]->(b) OPTIONAL MATCH (a)<-[e]-(b) RETURN a.n AS a, b.n AS b, e.id AS e')) = json('[{"a":"a","b":"b","e":"r1"}]')
    AND json(cypher('MATCH (a:A153)-[r]->(b) WITH a, b, r AS e OPTIONAL MATCH (a)<-[e]-(b) RETURN a.n AS a, b.n AS b')) = json('[{"a":"a","b":"b"}]');
INSERT INTO _assert SELECT 'MATCH with the bound relationship is as before',
    json(cypher('MATCH (:A153)-[e]->() MATCH (x)-[e]->(y) RETURN x.n AS x, y.n AS y')) = json('[{"x":"a","y":"b"}]');

-- datetime from an epoch value ---------------------------------------------------
INSERT INTO _assert SELECT 'datetime({epochMillis})',
    json(cypher('RETURN datetime({epochMillis: 1577836800000}) AS a, datetime({epochMillis: 1577836800123}) AS b, datetime(''2020-01-01T00:00Z'') = datetime({epochMillis: 1577836800000}) AS eq'))
    = json('[{"a":"2020-01-01T00:00Z","b":"2020-01-01T00:00:00.123Z","eq":true}]');
INSERT INTO _assert SELECT 'datetime({epochSeconds [, nanosecond]})',
    json(cypher('RETURN datetime({epochSeconds: 1577836861}) AS a, datetime({epochSeconds: 1577836800, nanosecond: 5}) AS b'))
    = json('[{"a":"2020-01-01T00:01:01Z","b":"2020-01-01T00:00:00.000000005Z"}]');
INSERT INTO _assert SELECT 'a variable, null, and the way back',
    json(cypher('WITH 1577836800000 AS ms RETURN datetime({epochMillis: ms}).epochMillis AS back, datetime({epochMillis: null}) AS n, datetime({year: 2020, month: 2, day: 3}) AS still'))
    = json('[{"back":1577836800000,"n":null,"still":"2020-02-03T00:00Z"}]');

SELECT 'Test 153 passed: ' || count(*) || ' assertions' AS result FROM _assert;
