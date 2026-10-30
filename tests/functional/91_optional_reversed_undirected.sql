-- ========================================================================
-- Test 91: an OPTIONAL MATCH written from either end (GQLITE-T-0462)
-- ========================================================================
-- PURPOSE: an undirected relationship whose bound node is written on the
--          right, `OPTIONAL MATCH (d)-[:R]-(a)`, was read as `(d)-[:R]->(a)`:
--          an edge from a to d was not found. It is now read from the bound
--          end, as `(a)-[:R]-(d)`. Found by the metamorphic harness
--          (relation reverse_pattern).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 91: OPTIONAL MATCH from either end ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (n0:B91 {id: 0}), (n1:A91:B91 {id: 1}), (n1)-[:R91]->(n0), (:C91 {id: 4})') as setup;

INSERT INTO _assert SELECT 'bound node on the right, edge leaving it',
    json(cypher('MATCH (a:A91) OPTIONAL MATCH (d)-[:R91]-(a) RETURN d.id AS d'))
    = json('[{"d":0}]');
INSERT INTO _assert SELECT 'the same rows as written from the left',
    json(cypher('MATCH (a:B91) OPTIONAL MATCH (d)-[:R91]-(a) RETURN a.id AS a, d.id AS d ORDER BY a'))
    = json(cypher('MATCH (a:B91) OPTIONAL MATCH (a)-[:R91]-(d) RETURN a.id AS a, d.id AS d ORDER BY a'));
INSERT INTO _assert SELECT 'a type list',
    json(cypher('MATCH (a:A91) OPTIONAL MATCH (d)-[:R91|S91]-(a) RETURN d.id AS d'))
    = json('[{"d":0}]');
INSERT INTO _assert SELECT 'directed is unchanged',
    json(cypher('MATCH (a:B91) OPTIONAL MATCH (d)-[:R91]->(a) RETURN a.id AS a, d.id AS d ORDER BY a'))
    = json('[{"a":0,"d":1},{"a":1,"d":null}]');
INSERT INTO _assert SELECT 'two hops written right to left keep the row',
    json(cypher('MATCH (a:C91) OPTIONAL MATCH (e)-[]-(d)<-[p]-(a) RETURN a.id AS a, d.id AS d'))
    = json('[{"a":4,"d":null}]');
INSERT INTO _assert SELECT 'two hops, the other way',
    json(cypher('MATCH (a:C91) OPTIONAL MATCH (e)<-[t]-(d)-[]-(a) RETURN a.id AS a, d.id AS d'))
    = json('[{"a":4,"d":null}]');

SELECT 'Test 91 passed: ' || count(*) || ' assertions' AS result FROM _assert;
