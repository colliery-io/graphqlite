-- ========================================================================
-- Test 80: multi-hop OPTIONAL MATCH with named relationships (GQLITE-T-0463)
-- ========================================================================
-- PURPOSE: a two-hop OPTIONAL MATCH that named a relationship was emitted
--          hop by hop: the second hop could reuse the first hop's edge, and
--          the path was not all-or-none. It now uses the combined derived
--          table (distinct edges, all-or-none) and joins each named
--          relationship back by its edge id. A WHERE over two of the
--          path's variables named a table joined later ("ON clause
--          references tables to its right"); it is now rewritten once per
--          join. Found by the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 80: OPTIONAL path with named relationships ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:U80 {id: 0})<-[:S80 {w: 5}]-(:V80 {id: 1})') as setup1;
SELECT cypher('CREATE (x:A80 {n: ''x''})-[:S80 {w: 1}]->(:Q80 {n: ''q''})-[:T80 {w: 2}]->(:O80 {n: ''o''}), (:A80 {n: ''y''})') as setup2;

INSERT INTO _assert SELECT 'one edge is not used twice; the path is all-or-none',
    json(cypher('MATCH (a:V80) OPTIONAL MATCH (a)-[p]->(d)<-[:S80]-(e) RETURN d.id AS d, e.id AS e, p.w AS w'))
    = json('[{"d":null,"e":null,"w":null}]');
INSERT INTO _assert SELECT 'named relationships bound, unmatched row kept',
    json(cypher('MATCH (a:A80) OPTIONAL MATCH (a)-[s:S80]->(q)-[t:T80]->(o) RETURN a.n AS a, s.w AS sw, t.w AS tw, type(t) AS ty ORDER BY a'))
    = json('[{"a":"x","sw":1,"tw":2,"ty":"T80"},{"a":"y","sw":null,"tw":null,"ty":null}]');
INSERT INTO _assert SELECT 'a type list in the path',
    json(cypher('MATCH (a:A80) OPTIONAL MATCH (a)-[s:S80|T80]->(q)-[t]->(o) RETURN a.n AS a, t.w AS tw ORDER BY a'))
    = json('[{"a":"x","tw":2},{"a":"y","tw":null}]');
INSERT INTO _assert SELECT 'WHERE on a named relationship rejects the whole path',
    json(cypher('MATCH (a:A80) OPTIONAL MATCH (a)-[s]->(q)-[:T80]->(o) WHERE s.w = 9 RETURN a.n AS a, q.n AS q ORDER BY a'))
    = json('[{"a":"x","q":null},{"a":"y","q":null}]');
INSERT INTO _assert SELECT 'WHERE over two path nodes',
    json(cypher('MATCH (a:A80) OPTIONAL MATCH (a)-[]->(q)-[]->(o) WHERE q.n = ''q'' AND o.n = ''o'' RETURN a.n AS a, o.n AS o ORDER BY a'))
    = json('[{"a":"x","o":"o"},{"a":"y","o":null}]');
INSERT INTO _assert SELECT 'WHERE over a relationship and a node',
    json(cypher('MATCH (a:A80) OPTIONAL MATCH (a)-[s]->(q)-[t]->(o) WHERE t.w = 3 AND o.n = ''o'' RETURN a.n AS a, o.n AS o, s.w AS sw ORDER BY a'))
    = json('[{"a":"x","o":null,"sw":null},{"a":"y","o":null,"sw":null}]');

SELECT cypher('CREATE (n0:B80 {id: 0}), (n3:B80 {id: 3}), (n3)-[:S80]->(n0), (n3)-[:R80]->(n0), (:A80 {id: 9})-[:R80]->(n0)') as setup3;
INSERT INTO _assert SELECT 'label on a new source node filters its edges (one null row)',
    json(cypher('MATCH (a:B80) OPTIONAL MATCH (a)<-[p:R80|S80]-(d:A80) RETURN a.id AS a, d.id AS d ORDER BY a'))
    = json('[{"a":0,"d":9},{"a":3,"d":null}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 80 Complete ===' as test_section;
