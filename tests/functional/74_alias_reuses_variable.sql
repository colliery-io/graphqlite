-- ========================================================================
-- Test 74: a RETURN alias that reuses a variable name (GQLITE-T-0434, #151)
-- ========================================================================
-- PURPOSE: the implicit GROUP BY was translated after the RETURN aliases
--          were registered, so `RETURN d.id AS d, count(r)` grouped by the
--          output column d (one row), and `RETURN a.id AS k, count(a) AS a`
--          grouped by COUNT ("misuse of aggregate"). Found by a user (#151)
--          and the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 74: alias reuses a variable name ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:S74 {id: ''d1''}), (:S74 {id: ''d2''}), (:S74 {id: ''d3''})') as setup1;
SELECT cypher('MATCH (s:S74) UNWIND [0, 1] AS i CREATE (s)-[:B74]->(:R74 {i: i})') as setup2;

INSERT INTO _assert SELECT '#151: RETURN d.id AS d, count(r)',
    json(cypher('MATCH (d:S74)-[:B74]->(r) RETURN d.id AS d, count(r) AS n ORDER BY d'))
    = json('[{"d":"d1","n":2},{"d":"d2","n":2},{"d":"d3","n":2}]');
INSERT INTO _assert SELECT 'aggregate aliased as the variable it counts',
    json(cypher('MATCH (a:S74) RETURN a.id AS k, count(a) AS a ORDER BY k'))
    = json('[{"k":"d1","a":1},{"k":"d2","a":1},{"k":"d3","a":1}]');
INSERT INTO _assert SELECT 'relationship function aliased as the relationship',
    json(cypher('MATCH (:S74)-[r:B74]->() RETURN type(r) AS r, count(*) AS n')) = json('[{"r":"B74","n":6}]');
INSERT INTO _assert SELECT 'swapped aliases',
    json(cypher('MATCH (d:S74)-[:B74]->(r) RETURN d.id AS r, collect(r.i) AS d ORDER BY r'))
    = json('[{"r":"d1","d":[0,1]},{"r":"d2","d":[0,1]},{"r":"d3","d":[0,1]}]');
INSERT INTO _assert SELECT 'with ORDER BY on the aggregate and LIMIT',
    json(cypher('MATCH (d:S74)-[:B74]->(r) RETURN d.id AS d, count(r) AS n ORDER BY n DESC, d LIMIT 2'))
    = json('[{"d":"d1","n":2},{"d":"d2","n":2}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 74 Complete ===' as test_section;
