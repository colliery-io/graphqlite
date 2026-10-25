-- ========================================================================
-- Test 72: unary minus on properties, map access in WITH, properties(x).k,
--          multi-hop pattern comprehensions (GQLITE-T-0460, GQLITE-T-0451)
-- ========================================================================
-- PURPOSE: `-a.v` parsed as (-a).v, a key of the number 0 - a: null, and an
--          SQL error in WITH. A WITH item `{x: a.v}.x` was dropped (empty
--          column). properties(n).k was refused. A pattern comprehension
--          took one hop only, and did not keep its edges distinct.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 72: expression gaps ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A72 {n: 1, v: 2})-[:R72 {w: 5}]->(b:B72 {n: 2})-[:S72]->(:C72 {n: 3}), (b)-[:S72]->(:C72 {n: 4}), (:X72 {n: 9})-[:R72]->(b)') as setup;

INSERT INTO _assert SELECT 'unary minus binds looser than property access',
    json(cypher('MATCH (a:A72) RETURN -a.v AS x, -a.v + 1 AS y, -[1, 2][1] AS z')) = json('[{"x":-2,"y":-1,"z":-2}]');
INSERT INTO _assert SELECT 'unary minus still binds tighter than ^',
    json(cypher('RETURN -3^2 AS x')) = json('[{"x":9.0}]');
INSERT INTO _assert SELECT 'unary minus in WITH',
    json(cypher('MATCH (a:A72) WITH a, -a.v AS x RETURN x')) = json('[{"x":-2}]');
INSERT INTO _assert SELECT 'map access in WITH',
    json(cypher('MATCH (a:A72) WITH {x: a.v}.x AS x RETURN x')) = json('[{"x":2}]');
INSERT INTO _assert SELECT 'endpoint property in WITH',
    json(cypher('MATCH ()-[r:R72 {w: 5}]->() WITH startNode(r).v AS sv RETURN sv')) = json('[{"sv":2}]');
INSERT INTO _assert SELECT 'properties(x).key',
    json(cypher('MATCH (a:A72)-[r:R72]->() RETURN properties(a).v AS v, properties(r).w AS w, properties(a).nope AS n')) = json('[{"v":2,"w":5,"n":null}]');
INSERT INTO _assert SELECT 'two-hop pattern comprehension',
    json(cypher('MATCH (a:A72) RETURN [(a)-->(b)-->(c) | c.n] AS l')) = json('[{"l":[3,4]}]');
INSERT INTO _assert SELECT 'pattern comprehension keeps edges distinct',
    json(cypher('MATCH (a:A72) RETURN [(a)-->(b)<-[:R72]-(x) | x.n] AS l')) = json('[{"l":[9]}]');
INSERT INTO _assert SELECT 'two-hop comprehension with WHERE, and a path variable',
    json(cypher('MATCH (a:A72) RETURN [(a)-[:R72]->(b)-[:S72]->(c) WHERE c.n > 3 | c.n] AS l, [p = (a)-->()-->() | length(p)] AS ps')) = json('[{"l":[4],"ps":[2,2]}]');

SELECT name, ok FROM _assert;
SELECT '=== Test 72 Complete ===' as test_section;
