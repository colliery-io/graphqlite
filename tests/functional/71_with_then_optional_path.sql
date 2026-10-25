-- ========================================================================
-- Test 71: WITH, then a two-hop OPTIONAL MATCH (GQLITE-T-0436, GitHub #153)
-- ========================================================================
-- PURPOSE: a node carried through WITH / UNWIND was not seen as bound by the
--          multi-hop OPTIONAL MATCH builders: it got `LEFT JOIN nodes AS
--          _with_0.a` and `_with_0.a.id` ("near \".\": syntax error"). Each
--          query must give what the same query gives without the WITH.
--          WITH * also copied the internal name of an anonymous OPTIONAL
--          relationship, which has no table ("no such column").
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 71: WITH then two-hop OPTIONAL MATCH ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A71 {n: ''a''})') as setup1;
SELECT cypher('CREATE (x:A71 {n: ''b''})-[:S71]->(:Q71 {n: ''q1''})-[:T71]->(:O71 {n: ''o1''}), (x)-[:S71]->(:Q71 {n: ''q2''})-[:U71]->(:O71 {n: ''o2''})') as setup2;

INSERT INTO _assert SELECT '#153: WITH + aggregate, then anonymous typed two hops',
    json(cypher('MATCH (a:A71) OPTIONAL MATCH (a)-[:R71]->(r) WITH a, collect(r.n) AS rs OPTIONAL MATCH (a)-[:S71]->(q)-[:T71]->(o) RETURN a.n AS n, rs, collect(o.n) AS os ORDER BY n'))
    = json('[{"n":"a","rs":[],"os":[]},{"n":"b","rs":[],"os":["o1"]}]');
INSERT INTO _assert SELECT 'WITH a, then two hops',
    json(cypher('MATCH (a:A71) WITH a OPTIONAL MATCH (a)-[:S71]->(q)-[:T71]->(o) RETURN a.n AS n, o.n AS o ORDER BY n'))
    = json('[{"n":"a","o":null},{"n":"b","o":"o1"}]');
INSERT INTO _assert SELECT 'UNWIND, then two hops',
    json(cypher('MATCH (a:A71) UNWIND [1] AS i OPTIONAL MATCH (a)-[:S71]->(q)-[:T71]->(o) RETURN a.n AS n, o.n AS o ORDER BY n'))
    = json('[{"n":"a","o":null},{"n":"b","o":"o1"}]');
INSERT INTO _assert SELECT 'two carried nodes, one free',
    json(cypher('MATCH (a:A71)-[:S71]->(q) WITH a, q OPTIONAL MATCH (a)-[:S71]->(q)-[:T71]->(o) RETURN q.n AS q, o.n AS o ORDER BY q'))
    = json('[{"q":"q1","o":"o1"},{"q":"q2","o":null}]');
INSERT INTO _assert SELECT 'carried ends, free middle',
    json(cypher('MATCH (a:A71)-[:S71]->(q)-[]->(o) WITH a, o OPTIONAL MATCH (a)-[:S71]->(m)-[:T71]->(o) RETURN o.n AS o, m.n AS m ORDER BY o'))
    = json('[{"o":"o1","m":"q1"},{"o":"o2","m":null}]');

INSERT INTO _assert SELECT 'WITH * after OPTIONAL MATCH with an anonymous relationship',
    json(cypher('MATCH (a:A71) OPTIONAL MATCH (a)-[:S71]->(q) WITH * RETURN a.n AS n, q.n AS q ORDER BY n, q'))
    = json('[{"n":"a","q":null},{"n":"b","q":"q1"},{"n":"b","q":"q2"}]');
INSERT INTO _assert SELECT 'WITH * WHERE, then UNWIND',
    json(cypher('MATCH (a:A71) OPTIONAL MATCH (a)-[]->(q) WITH * WHERE q IS NOT NULL UNWIND [1] AS i RETURN q.n AS q ORDER BY q'))
    = json('[{"q":"q1"},{"q":"q2"}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 71 Complete ===' as test_section;
