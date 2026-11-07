-- ========================================================================
-- Test 122: a label test on a list comprehension variable (GQLITE-T-0494)
-- ========================================================================
-- PURPOSE: in a comprehension over nodes, x is the node's JSON, and the
--          label test compared that text with a node id: [x IN [n] WHERE
--          x:A] was [] for an :A node. It now reads the JSON's id (a node)
--          or type (a relationship).
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 122: comprehension label test ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A122 {id: 1})-[:R122 {w: 1}]->(:B122 {id: 2}), (:A122:C122 {id: 3})') as setup;

INSERT INTO _assert SELECT 'nodes and relationships in a comprehension',
    json(cypher('MATCH (a:A122)-[r]->(b) RETURN [x IN [r] WHERE x:R122 | x.w] AS e, [x IN [a, b] WHERE x:A122 | x.id] AS n'))
    = json('[{"e":[1],"n":[1]}]');
INSERT INTO _assert SELECT 'over a collected list, with a disjunction and NOT',
    json(cypher('MATCH (n) WHERE n.id IS NOT NULL WITH collect(n) AS ns RETURN [x IN ns WHERE x:A122 | x.id] AS a, [x IN ns WHERE x:B122|C122 | x.id] AS bc, [x IN ns WHERE NOT x:A122 | x.id] AS na'))
    = json('[{"a":[1,3],"bc":[2,3],"na":[2]}]');
INSERT INTO _assert SELECT 'after UNWIND of a collected list',
    json(cypher('MATCH (n) WHERE n.id IS NOT NULL WITH collect(n) AS ns UNWIND ns AS u RETURN u.id AS id, u:A122 AS a ORDER BY id'))
    = json('[{"id":1,"a":true},{"id":2,"a":false},{"id":3,"a":true}]');

SELECT 'Test 122 passed: ' || count(*) || ' assertions' AS result FROM _assert;
