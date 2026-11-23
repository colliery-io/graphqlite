-- ========================================================================
-- Test 158: list comprehensions and list predicates over null
--           (GQLITE-T-0557)
-- ========================================================================
-- PURPOSE: [x IN null | x] gave []; any / all / none / single over null
--          gave the answer for an empty list. Cypher gives null for each.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 158: list operations over null ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:R158 {id: 1, l: [1, 2]})') as setup;

INSERT INTO _assert SELECT 'list comprehension over null',
    json(cypher('RETURN [x IN null | x] AS a, [x IN null WHERE x > 1] AS b, [x IN null WHERE x > 1 | x * 2] AS c'))
    = json('[{"a":null,"b":null,"c":null}]');
INSERT INTO _assert SELECT 'it is null, not a list',
    json(cypher('WITH null AS l RETURN [x IN l | x] IS NULL AS n, size([x IN l | x]) AS s, coalesce([x IN l | x], ''dflt'') AS c'))
    = json('[{"n":true,"s":null,"c":"dflt"}]');
INSERT INTO _assert SELECT 'over a missing property',
    json(cypher('MATCH (r:R158) RETURN [x IN r.l | x + 1] AS a, [x IN r.nope | x] AS b'))
    = json('[{"a":[2,3],"b":null}]');
INSERT INTO _assert SELECT 'over the nodes of a null path',
    json(cypher('OPTIONAL MATCH p = (a:Nope158)-->(b) RETURN [n IN nodes(p) | n.name] AS ns, [x IN relationships(p) | type(x)] AS ts'))
    = json('[{"ns":null,"ts":null}]');
INSERT INTO _assert SELECT 'row by row',
    json(cypher('UNWIND [[1, 2], null, []] AS l RETURN [x IN l | x * 2] AS d'))
    = json('[{"d":[2,4]},{"d":null},{"d":[]}]');

INSERT INTO _assert SELECT 'list predicates over null',
    json(cypher('RETURN any(x IN null WHERE x > 1) AS an, all(x IN null WHERE x > 1) AS al, none(x IN null WHERE x > 1) AS no, single(x IN null WHERE x > 1) AS si'))
    = json('[{"an":null,"al":null,"no":null,"si":null}]');
INSERT INTO _assert SELECT 'in WHERE, null keeps no row',
    json(cypher('MATCH (r:R158) WHERE none(x IN r.nope WHERE x > 0) RETURN r.id AS id')) = json('[]')
    AND json(cypher('MATCH (r:R158) WHERE NOT any(x IN r.nope WHERE x > 0) RETURN r.id AS id')) = json('[]')
    AND json(cypher('MATCH (r:R158) WHERE coalesce(none(x IN r.nope WHERE x > 0), true) RETURN r.id AS id')) = json('[{"id":1}]');

-- unchanged ------------------------------------------------------------------
INSERT INTO _assert SELECT 'an empty list and a list with nulls are as before',
    json(cypher('RETURN [x IN [] | x] AS a, [x IN [1, null, 3] | x] AS b, all(x IN [] WHERE x > 1) AS c, any(x IN [] WHERE x > 1) AS d, none(x IN [null] WHERE x > 1) AS e'))
    = json('[{"a":[],"b":[1,null,3],"c":true,"d":false,"e":null}]');
INSERT INTO _assert SELECT 'values, booleans and nested lists keep their types',
    json(cypher('RETURN [x IN [1, 2, 3] WHERE x > 1 | x * 2] AS a, [x IN [true, false] | x] AS b, [x IN [[1], [2]] | x] AS n, size([x IN [1, 2] | x]) AS s'))
    = json('[{"a":[4,6],"b":[true,false],"n":[[1],[2]],"s":2}]');
INSERT INTO _assert SELECT 'reduce over null was null already',
    json(cypher('RETURN reduce(s = 0, x IN null | s + x) AS r')) = json('[{"r":null}]');
INSERT INTO _assert SELECT 'a pattern comprehension from a null node is an empty list',
    json(cypher('MATCH (r:R158) OPTIONAL MATCH (r)-[:NOPE]->(z) RETURN [(z)-->(y) | y.name] AS pc')) = json('[{"pc":[]}]');

SELECT 'Test 158 passed: ' || count(*) || ' assertions' AS result FROM _assert;
