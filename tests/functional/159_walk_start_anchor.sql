-- ========================================================================
-- Test 159: a variable-length walk starts at the labels of its start node
--           (GQLITE-T-0558)
-- ========================================================================
-- PURPOSE: The walk table is seeded only from nodes that have the labels
--          of the start node, and each step looks up the next edges by
--          node. The answers must not change.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 159: walk start anchor ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:A159:X159 {i: 1})-[:N159]->(b:A159 {i: 2})-[:N159]->(c:B159 {i: 3})-[:N159]->(d:A159 {i: 4})') as setup;

INSERT INTO _assert SELECT 'label on the start node',
    json(cypher('MATCH (a:A159)-[:N159*1..3]->(b) RETURN a.i AS a, b.i AS b ORDER BY a, b'))
    = json('[{"a":1,"b":2},{"a":1,"b":3},{"a":1,"b":4},{"a":2,"b":3},{"a":2,"b":4}]');
INSERT INTO _assert SELECT 'two labels on the start node',
    json(cypher('MATCH (a:A159:X159)-[:N159*]->(b) RETURN a.i AS a, count(b) AS c'))
    = json('[{"a":1,"c":3}]');
INSERT INTO _assert SELECT 'label and property',
    json(cypher('MATCH (a:A159 {i: 2})-[:N159*]->(b) RETURN b.i AS b ORDER BY b'))
    = json('[{"b":3},{"b":4}]');
INSERT INTO _assert SELECT 'label that no start node has',
    json(cypher('MATCH (a:B159:X159)-[:N159*]->(b) RETURN count(*) AS c'))
    = json('[{"c":0}]');
INSERT INTO _assert SELECT 'label from an earlier MATCH',
    json(cypher('MATCH (a:B159) MATCH (a)-[:N159*]->(b) RETURN a.i AS a, b.i AS b'))
    = json('[{"a":3,"b":4}]');
INSERT INTO _assert SELECT 'label from an earlier MATCH, incoming walk',
    json(cypher('MATCH (a:B159) MATCH (a)<-[:N159*]-(b) RETURN b.i AS b ORDER BY b'))
    = json('[{"b":1},{"b":2}]');
INSERT INTO _assert SELECT 'no label: each node starts a walk',
    json(cypher('MATCH (a)-[:N159*2..2]->(b) RETURN a.i AS a, b.i AS b ORDER BY a'))
    = json('[{"a":1,"b":3},{"a":2,"b":4}]');
INSERT INTO _assert SELECT 'undirected walk from a label',
    json(cypher('MATCH (a:B159)-[:N159*1..1]-(b) RETURN b.i AS b ORDER BY b'))
    = json('[{"b":2},{"b":4}]');
INSERT INTO _assert SELECT 'zero-length walk keeps the label',
    json(cypher('MATCH (a:B159)-[:N159*0..1]->(b) RETURN b.i AS b ORDER BY b'))
    = json('[{"b":3},{"b":4}]');
INSERT INTO _assert SELECT 'walk in a pattern predicate',
    json(cypher('MATCH (a:A159) WHERE (a)-[:N159*]->(:B159) RETURN a.i AS a ORDER BY a'))
    = json('[{"a":1},{"a":2}]');
INSERT INTO _assert SELECT 'walk in a pattern comprehension',
    json(cypher('MATCH (a:A159 {i: 1}) UNWIND [(a)-[:N159*]->(b:A159) | b.i] AS b RETURN b ORDER BY b'))
    = json('[{"b":2},{"b":4}]');
INSERT INTO _assert SELECT 'OPTIONAL MATCH walk from a label',
    json(cypher('MATCH (a:A159) OPTIONAL MATCH (a)-[:N159*]->(b:B159) RETURN a.i AS a, b.i AS b ORDER BY a'))
    = json('[{"a":1,"b":3},{"a":2,"b":3},{"a":4,"b":null}]');

SELECT 'PASS: ' || count(*) || ' assertions' FROM _assert;
