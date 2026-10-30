-- ========================================================================
-- Test 86: a list comprehension variable is bound inside it only
--          (GQLITE-T-0468)
-- ========================================================================
-- PURPOSE: the comprehension bound its variable over the outer one and put
--          back only its SQL name, as a projected value: after
--          `[x IN l | ...]` a relationship x was no longer one ("no such
--          column", "argument must be a relationship variable"). The
--          binding is now saved and restored whole. The source list is
--          read under the outer binding, and each comprehension has its own
--          json_each alias (a nested one hid the outer element). Found by
--          the metamorphic harness.
-- NOTE:    Assertions are hard: _assert has CHECK (ok = 1), so under
--          `sqlite3 -bail` any failed expectation aborts the run.
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 86: comprehension variable scope ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

SELECT cypher('CREATE (:A86 {v: 1, l: [5, 6]})-[:R86]->(:B86 {v: 2})') as setup;

INSERT INTO _assert SELECT 'relationship after a comprehension of the same name',
    json(cypher('MATCH (a:A86)-[x]->(b) RETURN [x IN [1, 2] | x * 2] AS l, type(x) AS t'))
    = json('[{"l":[2,4],"t":"R86"}]');
INSERT INTO _assert SELECT 'startNode() after it',
    json(cypher('MATCH (a:A86)-[x]->(h) RETURN [x IN [1, 2, 3] WHERE x > h.v | x * 2] AS c0, startNode(x).v AS c1'))
    = json('[{"c0":[6],"c1":1}]');
INSERT INTO _assert SELECT 'the source list reads the outer binding',
    json(cypher('MATCH (x:A86) RETURN [x IN x.l | x + 1] AS l, x.v AS v'))
    = json('[{"l":[6,7],"v":1}]');
INSERT INTO _assert SELECT 'nested, same name',
    json(cypher('RETURN [y IN [1, 2] | [y IN [y, 10] | y * 3]] AS l'))
    = json('[{"l":[[3,30],[6,30]]}]');
INSERT INTO _assert SELECT 'nested, the inner one reads the outer element',
    json(cypher('RETURN [y IN [1, 2] | [z IN [10] | z + y]] AS l'))
    = json('[{"l":[[11],[12]]}]');

SELECT 'Test 86 passed: ' || count(*) || ' assertions' AS result FROM _assert;
