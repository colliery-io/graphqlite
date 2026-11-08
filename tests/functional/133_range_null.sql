-- ========================================================================
-- Test 133: range() with a null bound is null (GQLITE-T-0534)
-- ========================================================================
-- PURPOSE: range() with a null start, end or step gave []. It is null, as
--          other functions with a null argument (Neo4j raises a TypeError
--          instead). Found by the differential test against Neo4j.
--          Assertions are hard: _assert has CHECK (ok = 1).
-- ========================================================================

.load ./build/graphqlite

SELECT '=== Test 133: range() with null ===' as test_section;

CREATE TEMP TABLE _assert(name TEXT, ok INTEGER CHECK (ok = 1));

INSERT INTO _assert SELECT 'null start, end or step',
    json(cypher('RETURN range(0, null) AS a, range(null, 3) AS b, range(0, 3, null) AS c'))
        = json('[{"a":null,"b":null,"c":null}]');
INSERT INTO _assert SELECT 'ranges unchanged',
    json(cypher('RETURN range(0, 3) AS a, range(5, 1, -2) AS b, range(1, 0) AS c'))
        = json('[{"a":[0,1,2,3],"b":[5,3,1],"c":[]}]');
SELECT cypher('CREATE (:A {id: 2})');
INSERT INTO _assert SELECT 'after OPTIONAL MATCH',
    json(cypher('MATCH (a:A) OPTIONAL MATCH (a)-->(d) RETURN range(0, d.id) AS r, range(0, a.id) AS s'))
        = json('[{"r":null,"s":[0,1,2]}]');

SELECT 'Test 133 passed: ' || count(*) || ' assertions' AS result FROM _assert;
