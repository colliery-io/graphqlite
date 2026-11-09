-- ========================================================================
-- Test 144: graph algorithms with integer ids (GQLITE-T-0546)
-- ========================================================================
-- PURPOSE: The algorithms read only a text id property: with integer ids
--          user_id was null (triangleCount gave ""), bfs(0) found nothing
--          and dijkstra(0, 1) failed. A text id with a quote broke the
--          JSON. Now an id keeps its type, is escaped, and an integer
--          argument names a node. Assertions are hard: _assert has
--          CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 144: algorithms with integer ids ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (a:G {id: 0})-[:R]->(b:G {id: 1})-[:R]->(c:G {id: ''x"y''}), (d:G {name: ''none''})');

INSERT INTO _assert SELECT 'user_id keeps its type',
    (SELECT group_concat(json_type(value, '$.user_id'), ',') FROM json_each(cypher('CALL degreeCentrality()')))
        = 'integer,integer,text,null';
INSERT INTO _assert SELECT 'triangleCount user_id like the others',
    (SELECT group_concat(json_type(value, '$.user_id'), ',') FROM json_each(cypher('CALL triangleCount()')))
        = 'integer,integer,text,null';
INSERT INTO _assert SELECT 'quoted id is valid JSON',
    json_valid(cypher('CALL pageRank()')) = 1;
INSERT INTO _assert SELECT 'bfs from an integer id',
    json(cypher('CALL bfs(0) YIELD user_id, depth RETURN user_id, depth'))
        = json('[{"user_id":0,"depth":0},{"user_id":1,"depth":1},{"user_id":"x\"y","depth":2}]');
INSERT INTO _assert SELECT 'dijkstra between integer ids',
    json(cypher('CALL dijkstra(0, 1)')) = json('[{"path":[0,1],"distance":1,"found":true}]');
INSERT INTO _assert SELECT 'text argument still finds an integer id',
    json_array_length(cypher('CALL dfs(''0'')')) = 3;
INSERT INTO _assert SELECT 'apsp with mixed ids',
    json(cypher('CALL apsp()'))
        = json('[{"source":0,"target":1,"distance":1},{"source":0,"target":"x\"y","distance":2},{"source":1,"target":"x\"y","distance":1}]');

SELECT 'Test 144 passed: ' || count(*) || ' assertions' AS result FROM _assert;
